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

#include "runtime/class_initializer.h"

#include "libpandafile/file_items.h"
#include "mem/vm_handle.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/runtime.h"
#include "runtime/monitor_object_lock.h"

#include "verification/job_queue/job_queue.h"

namespace panda {

static void WrapException(ClassLinker *class_linker, ManagedThread *thread)
{
    ASSERT(thread->HasPendingException());

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*thread->GetException()->ClassAddr<Class>());

    auto *error_class = class_linker->GetExtension(ctx)->GetClass(ctx.GetErrorClassDescriptor(), false);
    ASSERT(error_class != nullptr);

    auto *cause = thread->GetException();
    if (cause->IsInstanceOf(error_class)) {
        return;
    }

    ThrowException(ctx, thread, ctx.GetExceptionInInitializerErrorDescriptor(), nullptr);
}

static void ThrowNoClassDefFoundError(ManagedThread *thread, Class *klass)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    auto name = klass->GetName();
    ThrowException(ctx, thread, ctx.GetNoClassDefFoundErrorDescriptor(), utf::CStringAsMutf8(name.c_str()));
}

static void ThrowEarlierInitializationException(ManagedThread *thread, Class *klass)
{
    ASSERT(klass->IsErroneous());

    ThrowNoClassDefFoundError(thread, klass);
}

/* static */
bool ClassInitializer::Initialize(ClassLinker *class_linker, ManagedThread *thread, Class *klass)
{
    if (klass->IsInitialized()) {
        return true;
    }

    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    VMHandle<ObjectHeader> managed_class_obj_handle(thread, klass->GetManagedObject());
    {
        ObjectLock lock(managed_class_obj_handle.GetPtr());

        if (klass->IsInitialized()) {
            return true;
        }

        if (klass->IsErroneous()) {
            ThrowEarlierInitializationException(thread, klass);
            return false;
        }

        if (!klass->IsVerified()) {
            if (!VerifyClass(klass)) {
                klass->SetState(Class::State::ERRONEOUS);
                panda::ThrowVerificationException(utf::Mutf8AsCString(klass->GetDescriptor()));
                return false;
            }
        }

        if (klass->IsInitializing()) {
            if (klass->GetInitTid() == thread->GetId()) {
                return true;
            }

            while (true) {
                lock.Wait(true);

                if (thread->HasPendingException()) {
                    WrapException(class_linker, thread);
                    klass->SetState(Class::State::ERRONEOUS);
                    return false;
                }

                if (klass->IsInitializing()) {
                    continue;
                }

                if (klass->IsErroneous()) {
                    ThrowNoClassDefFoundError(thread, klass);
                    return false;
                }

                if (klass->IsInitialized()) {
                    return true;
                }

                UNREACHABLE();
            }
        }

        klass->SetInitTid(thread->GetId());
        klass->SetState(Class::State::INITIALIZING);
        if (!ClassInitializer::InitializeFields(klass)) {
            LOG(ERROR, CLASS_LINKER) << "Cannot initialize fields of class '" << klass->GetName() << "'";
            return false;
        }
    }

    LOG(DEBUG, CLASS_LINKER) << "Initializing class " << klass->GetName();

    if (!klass->IsInterface()) {
        auto *base = klass->GetBase();
        if (base != nullptr) {
            if (!Initialize(class_linker, thread, base)) {
                ObjectLock lock(managed_class_obj_handle.GetPtr());
                klass->SetState(Class::State::ERRONEOUS);
                lock.NotifyAll();
                return false;
            }
        }

        for (auto *iface : klass->GetInterfaces()) {
            if (iface->IsInitialized()) {
                continue;
            }

            if (!InitializeInterface(class_linker, thread, iface)) {
                ObjectLock lock(managed_class_obj_handle.GetPtr());
                klass->SetState(Class::State::ERRONEOUS);
                lock.NotifyAll();
                return false;
            }
        }
    }

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    Method::Proto proto(PandaVector<panda_file::Type> {panda_file::Type(panda_file::Type::TypeId::VOID)},
                        PandaVector<std::string_view> {});
    auto *cctor_name = ctx.GetCctorName();
    auto *cctor = klass->GetDirectMethod(cctor_name, proto);
    if (cctor != nullptr) {
        cctor->InvokeVoid(thread, nullptr);
    }

    {
        ObjectLock lock(managed_class_obj_handle.GetPtr());

        if (thread->HasPendingException()) {
            WrapException(class_linker, thread);
            klass->SetState(Class::State::ERRONEOUS);
            lock.NotifyAll();
            return false;
        }

        klass->SetState(Class::State::INITIALIZED);

        lock.NotifyAll();
    }

    return true;
}

template <class T>
static void InitializePrimitiveField(Class *klass, const Field &field)
{
    panda_file::FieldDataAccessor fda(*field.GetPandaFile(), field.GetFileId());
    auto value = fda.GetValue<T>();
    klass->SetFieldPrimitive<T>(field, value ? value.value() : 0);
}

static void InitializeTaggedField(Class *klass, const Field &field)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    klass->SetFieldPrimitive<coretypes::TaggedValue>(field, ctx.GetInitialTaggedValue());
}

static void InitializeStringField(Class *klass, const Field &field)
{
    panda_file::FieldDataAccessor fda(*field.GetPandaFile(), field.GetFileId());
    auto value = fda.GetValue<uint32_t>();
    coretypes::String *str = nullptr;
    if (value) {
        panda_file::File::EntityId id(value.value());
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
        str =
            Runtime::GetCurrent()->ResolveString(Runtime::GetCurrent()->GetPandaVM(), *klass->GetPandaFile(), id, ctx);
    } else {
        str = nullptr;
    }
    klass->SetFieldObject(field, str);
}

/* static */
bool ClassInitializer::InitializeFields(Class *klass)
{
    using Type = panda_file::Type;

    for (const auto &field : klass->GetStaticFields()) {
        switch (field.GetType().GetId()) {
            case Type::TypeId::U1:
            case Type::TypeId::U8:
                InitializePrimitiveField<uint8_t>(klass, field);
                break;
            case Type::TypeId::I8:
                InitializePrimitiveField<int8_t>(klass, field);
                break;
            case Type::TypeId::I16:
                InitializePrimitiveField<int16_t>(klass, field);
                break;
            case Type::TypeId::U16:
                InitializePrimitiveField<uint16_t>(klass, field);
                break;
            case Type::TypeId::I32:
                InitializePrimitiveField<int32_t>(klass, field);
                break;
            case Type::TypeId::U32:
                InitializePrimitiveField<uint32_t>(klass, field);
                break;
            case Type::TypeId::I64:
                InitializePrimitiveField<int64_t>(klass, field);
                break;
            case Type::TypeId::U64:
                InitializePrimitiveField<uint64_t>(klass, field);
                break;
            case Type::TypeId::F32:
                InitializePrimitiveField<float>(klass, field);
                break;
            case Type::TypeId::F64:
                InitializePrimitiveField<double>(klass, field);
                break;
            case Type::TypeId::TAGGED:
                InitializeTaggedField(klass, field);
                break;
            case Type::TypeId::REFERENCE:
                InitializeStringField(klass, field);
                break;
            default: {
                UNREACHABLE();
                break;
            }
        }
    }

    return true;
}

/* static */
bool ClassInitializer::InitializeInterface(ClassLinker *class_linker, ManagedThread *thread, Class *iface)
{
    ASSERT(iface->IsInterface());

    for (auto *base_iface : iface->GetInterfaces()) {
        if (base_iface->IsInitialized()) {
            continue;
        }

        if (!InitializeInterface(class_linker, thread, base_iface)) {
            return false;
        }
    }

    if (!iface->HasDefaultMethods()) {
        return true;
    }

    return Initialize(class_linker, thread, iface);
}

bool IsVerifySuccInAppInstall(const Class *klass)
{
    using panda::os::file::Mode;
    using panda::os::file::Open;

    auto *file = klass->GetPandaFile();
    if (file != nullptr && file->GetFilename().rfind("base.") != std::string::npos) {
        auto filename = file->GetFilename().substr(0, file->GetFilename().rfind('/')) + "/cacheFile";
        auto verifyFail = Open(filename, Mode::READONLY);
        if (verifyFail.IsValid()) {
            return false;
        }
    }
    return true;
}

/* static */
bool ClassInitializer::VerifyClass(Class *klass)
{
    ASSERT(!klass->IsVerified());

    auto &runtime = *Runtime::GetCurrent();
    auto &verif_opts = runtime.GetVerificationOptions();

    if (!IsVerifySuccInAppInstall(klass)) {
        LOG(ERROR, CLASS_LINKER) << "verify fail";
        return false;
    }

    if (verif_opts.Enable) {
        auto *file = klass->GetPandaFile();
        bool is_system_class = file == nullptr || verifier::JobQueue::IsSystemFile(file);
        bool skip_verification = is_system_class && !verif_opts.Mode.DoNotAssumeLibraryMethodsVerified;
        if (skip_verification) {
            for (auto &method : klass->GetMethods()) {
                method.SetVerified(true);
            }
        } else {
            LOG(INFO, VERIFIER) << "Verification of class '" << klass->GetName() << "'";
            for (auto &method : klass->GetMethods()) {
                method.EnqueueForVerification();
            }
            // sync point
            if (verif_opts.Mode.SyncOnClassInitialization) {
                for (auto &method : klass->GetMethods()) {
                    if (!method.Verify()) {
                        return false;
                    }
                }
            }
        }
    }

    klass->SetState(Class::State::VERIFIED);
    return true;
}

}  // namespace panda
