/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/include/class_linker_extension.h"

#include "libpandabase/utils/utf.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"

namespace panda {

ClassLinkerExtension::~ClassLinkerExtension()
{
    os::memory::LockHolder lock(contexts_lock_);
    for (auto *ctx : contexts_) {
        class_linker_->GetAllocator()->Delete(ctx);
    }
}

Class *ClassLinkerExtension::BootContext::LoadClass(const uint8_t *descriptor, bool need_copy_descriptor,
                                                    ClassLinkerErrorHandler *error_handler)
{
    ASSERT(extension_->IsInitialized());

    return extension_->GetClassLinker()->GetClass(descriptor, need_copy_descriptor, this, error_handler);
}

class SuppressErrorHandler : public ClassLinkerErrorHandler {
    void OnError([[maybe_unused]] ClassLinker::Error error, [[maybe_unused]] const PandaString &message) override {}
};

Class *ClassLinkerExtension::AppContext::LoadClass(const uint8_t *descriptor, bool need_copy_descriptor,
                                                   ClassLinkerErrorHandler *error_handler)
{
    ASSERT(extension_->IsInitialized());

    SuppressErrorHandler handler;
    auto *cls = extension_->GetClass(descriptor, need_copy_descriptor, nullptr, &handler);
    if (cls != nullptr) {
        return cls;
    }

    for (auto &pf : pfs_) {
        auto class_id = pf->GetClassId(descriptor);
        if (!class_id.IsValid() || pf->IsExternal(class_id)) {
            continue;
        }
        return extension_->GetClassLinker()->LoadClass(*pf, class_id, this, error_handler);
    }

    if (error_handler != nullptr) {
        PandaStringStream ss;
        ss << "Cannot find class " << descriptor << " in all app panda files";
        error_handler->OnError(ClassLinker::Error::CLASS_NOT_FOUND, ss.str());
    }
    return nullptr;
}

void ClassLinkerExtension::InitializeArrayClassRoot(ClassRoot root, ClassRoot component_root, const char *descriptor)
{
    ASSERT(IsInitialized());

    auto *array_class = CreateClass(utf::CStringAsMutf8(descriptor), GetClassVTableSize(root), GetClassIMTSize(root),
                                    GetClassSize(root));
    array_class->SetLoadContext(&boot_context_);
    auto *component_class = GetClassRoot(component_root);
    InitializeArrayClass(array_class, component_class);

    AddClass(array_class);
    SetClassRoot(root, array_class);
}

void ClassLinkerExtension::InitializePrimitiveClassRoot(ClassRoot root, panda_file::Type::TypeId type_id,
                                                        const char *descriptor)
{
    ASSERT(IsInitialized());

    auto *primitive_class = CreateClass(utf::CStringAsMutf8(descriptor), GetClassVTableSize(root),
                                        GetClassIMTSize(root), GetClassSize(root));
    primitive_class->SetType(panda_file::Type(type_id));
    primitive_class->SetLoadContext(&boot_context_);
    InitializePrimitiveClass(primitive_class);
    AddClass(primitive_class);
    SetClassRoot(root, primitive_class);
}

bool ClassLinkerExtension::Initialize(ClassLinker *class_linker, bool compressed_string_enabled)
{
    class_linker_ = class_linker;
    InitializeImpl(compressed_string_enabled);

    can_initialize_classes_ = true;
    // Copy classes to separate container as ClassLinkerExtension::InitializeClass
    // can load more classes and modify boot context
    PandaVector<Class *> klasses;
    boot_context_.EnumerateClasses([&klasses](Class *klass) {
        if (!klass->IsLoaded()) {
            klasses.push_back(klass);
        }
        return true;
    });

    for (auto *klass : klasses) {
        if (klass->IsLoaded()) {
            continue;
        }

        InitializeClass(klass);
        klass->SetState(Class::State::LOADED);
    }
    return true;
}

bool ClassLinkerExtension::InitializeRoots(ManagedThread *thread)
{
    ASSERT(IsInitialized());

    for (auto *klass : class_roots_) {
        if (klass == nullptr) {
            continue;
        }

        if (!class_linker_->InitializeClass(thread, klass)) {
            LOG(FATAL, CLASS_LINKER) << "Failed to initialize class '" << klass->GetName() << "'";
            return false;
        }
    }

    return true;
}

Class *ClassLinkerExtension::FindLoadedClass(const uint8_t *descriptor, ClassLinkerContext *context /* = nullptr */)
{
    return class_linker_->FindLoadedClass(descriptor, ResolveContext(context));
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
Class *ClassLinkerExtension::GetClass(const uint8_t *descriptor, bool need_copy_descriptor /* = true */,
                                      // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
                                      ClassLinkerContext *context /* = nullptr */,
                                      ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    ASSERT(IsInitialized());

    return class_linker_->GetClass(descriptor, need_copy_descriptor, ResolveContext(context),
                                   ResolveErrorHandler(error_handler));
}

static void WrapClassNotFoundExceptionIfNeeded(ClassLinker *class_linker, const uint8_t *descriptor,
                                               LanguageContext ctx)
{
    auto *thread = ManagedThread::GetCurrent();
    if (thread == nullptr || !thread->HasPendingException()) {
        return;
    }

    auto *class_not_found_exception_class =
        class_linker->GetExtension(ctx)->GetClass(ctx.GetClassNotFoundExceptionDescriptor());
    ASSERT(class_not_found_exception_class != nullptr);

    auto *cause = thread->GetException();
    if (cause->IsInstanceOf(class_not_found_exception_class)) {
        auto name = ClassHelper::GetName(descriptor);
        panda::ThrowException(ctx, thread, ctx.GetNoClassDefFoundErrorDescriptor(), utf::CStringAsMutf8(name.c_str()));
    }
}

Class *ClassLinkerExtension::GetClass(const panda_file::File &pf, panda_file::File::EntityId id,
                                      // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
                                      ClassLinkerContext *context /* = nullptr */,
                                      ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    ASSERT(IsInitialized());

    auto *cls = class_linker_->GetClass(pf, id, ResolveContext(context), ResolveErrorHandler(error_handler));
    if (UNLIKELY(cls == nullptr)) {
        auto *descriptor = pf.GetStringData(id).data;
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(GetLanguage());
        WrapClassNotFoundExceptionIfNeeded(class_linker_, descriptor, ctx);
    }

    return cls;
}

Class *ClassLinkerExtension::AddClass(Class *klass)
{
    ASSERT(IsInitialized());

    auto *context = klass->GetLoadContext();
    auto *other_klass = ResolveContext(context)->InsertClass(klass);
    if (other_klass != nullptr) {
        class_linker_->FreeClass(klass);
        return other_klass;
    }
    RemoveCreatedClass(klass);

    return klass;
}

size_t ClassLinkerExtension::NumLoadedClasses()
{
    ASSERT(IsInitialized());

    size_t sum = boot_context_.NumLoadedClasses();
    {
        os::memory::LockHolder lock(contexts_lock_);
        for (auto *ctx : contexts_) {
            sum += ctx->NumLoadedClasses();
        }
    }
    return sum;
}

void ClassLinkerExtension::VisitLoadedClasses(size_t flag)
{
    boot_context_.VisitLoadedClasses(flag);
    {
        os::memory::LockHolder lock(contexts_lock_);
        for (auto *ctx : contexts_) {
            ctx->VisitLoadedClasses(flag);
        }
    }
}

void ClassLinkerExtension::FreeLoadedClasses()
{
    ASSERT(IsInitialized());

    boot_context_.EnumerateClasses([this](Class *klass) {
        FreeClass(klass);
        class_linker_->FreeClassData(klass);
        return true;
    });
    {
        os::memory::LockHolder lock(contexts_lock_);
        for (auto *ctx : contexts_) {
            ctx->EnumerateClasses([this](Class *klass) {
                FreeClass(klass);
                class_linker_->FreeClassData(klass);
                return true;
            });
        }
    }
}

ClassLinkerContext *ClassLinkerExtension::CreateApplicationClassLinkerContext(const PandaVector<PandaString> &path)
{
    PandaVector<PandaFilePtr> app_files;
    for (auto &pf_path : path) {
        auto pf = panda_file::OpenPandaFileOrZip(pf_path);
        if (pf == nullptr) {
            return nullptr;
        }
        app_files.push_back(std::move(pf));
    }
    return CreateApplicationClassLinkerContext(std::move(app_files));
}

ClassLinkerContext *ClassLinkerExtension::CreateApplicationClassLinkerContext(PandaVector<PandaFilePtr> &&app_files)
{
    PandaVector<const panda_file::File *> app_file_ptrs;
    app_file_ptrs.reserve(app_files.size());
    for (auto &pf : app_files) {
        app_file_ptrs.emplace_back(pf.get());
    }

    auto allocator = class_linker_->GetAllocator();
    auto *ctx = allocator->New<AppContext>(this, std::move(app_file_ptrs));
    RegisterContext([ctx]() { return ctx; });
    for (auto &pf : app_files) {
        class_linker_->AddPandaFile(std::move(pf), ctx);
    }
    return ctx;
}

void ClassLinkerExtension::AddCreatedClass(Class *klass)
{
    os::memory::LockHolder lock(created_classes_lock_);
    created_classes_.push_back(klass);
}

void ClassLinkerExtension::RemoveCreatedClass(Class *klass)
{
    os::memory::LockHolder lock(created_classes_lock_);
    auto it = find(created_classes_.begin(), created_classes_.end(), klass);
    if (it != created_classes_.end()) {
        created_classes_.erase(it);
    }
}

void ClassLinkerExtension::OnClassPrepared(Class *klass)
{
    RemoveCreatedClass(klass);
}

Class *ClassLinkerExtension::FromClassObject(ObjectHeader *obj)
{
    return obj != nullptr ? (reinterpret_cast<panda::coretypes::Class *>(obj))->GetRuntimeClass() : nullptr;
}

size_t ClassLinkerExtension::GetClassObjectSizeFromClassSize(uint32_t size)
{
    return panda::coretypes::Class::GetSize(size);
}

}  // namespace panda
