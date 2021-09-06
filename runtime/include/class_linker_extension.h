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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_LINKER_EXTENSION_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_LINKER_EXTENSION_H_

#include "libpandabase/os/mutex.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "runtime/class_linker_context.h"
#include "runtime/include/class_root.h"
#include "runtime/include/class.h"
#include "runtime/include/mem/panda_containers.h"

namespace panda {

class ClassLinker;
class ClassLinkerErrorHandler;

class ClassLinkerExtension {
public:
    explicit ClassLinkerExtension(panda_file::SourceLang lang) : lang_(lang), boot_context_(this) {}

    virtual ~ClassLinkerExtension();

    bool Initialize(ClassLinker *class_linker, bool compressed_string_enabled);

    bool InitializeFinish();

    bool InitializeRoots(ManagedThread *thread);

    virtual void InitializeArrayClass(Class *array_class, Class *component_class) = 0;

    virtual void InitializePrimitiveClass(Class *primitive_class) = 0;

    virtual size_t GetClassVTableSize(ClassRoot root) = 0;

    virtual size_t GetClassIMTSize(ClassRoot root) = 0;

    virtual size_t GetClassSize(ClassRoot root) = 0;

    virtual size_t GetArrayClassVTableSize() = 0;

    virtual size_t GetArrayClassIMTSize() = 0;

    virtual size_t GetArrayClassSize() = 0;

    virtual Class *CreateClass(const uint8_t *descriptor, size_t vtable_size, size_t imt_size, size_t size) = 0;

    virtual void FreeClass(Class *klass) = 0;

    virtual void InitializeClass(Class *klass) = 0;

    virtual const void *GetNativeEntryPointFor(Method *method) const = 0;

    virtual bool CanThrowException(const Method *method) const = 0;

    virtual ClassLinkerErrorHandler *GetErrorHandler() = 0;

    virtual ClassLinkerContext *CreateApplicationClassLinkerContext(const PandaVector<PandaString> &path);

    Class *GetClassRoot(ClassRoot root) const
    {
        return class_roots_[ToIndex(root)];
    }

    ClassLinkerContext *GetBootContext()
    {
        return &boot_context_;
    }

    void SetClassRoot(ClassRoot root, Class *klass)
    {
        class_roots_[ToIndex(root)] = klass;
        boot_context_.InsertClass(klass);
    }

    Class *FindLoadedClass(const uint8_t *descriptor, ClassLinkerContext *context = nullptr);

    Class *GetClass(const uint8_t *descriptor, bool need_copy_descriptor = true, ClassLinkerContext *context = nullptr,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    Class *GetClass(const panda_file::File &pf, panda_file::File::EntityId id, ClassLinkerContext *context = nullptr,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    panda_file::SourceLang GetLanguage() const
    {
        return lang_;
    }

    ClassLinker *GetClassLinker() const
    {
        return class_linker_;
    }

    bool IsInitialized() const
    {
        return class_linker_ != nullptr;
    }

    bool CanInitializeClasses()
    {
        return can_initialize_classes_;
    }

    template <class Callback>
    bool EnumerateClasses(const Callback &cb, mem::VisitGCRootFlags flags = mem::VisitGCRootFlags::ACCESS_ROOT_ALL)
    {
        if (((flags & mem::VisitGCRootFlags::ACCESS_ROOT_ALL) != 0) ||
            ((flags & mem::VisitGCRootFlags::ACCESS_ROOT_ONLY_NEW) != 0)) {
            os::memory::LockHolder lock(created_classes_lock_);
            for (const auto &cls : created_classes_) {
                if (!cb(cls)) {
                    return false;
                }
            }
        }
        if (!boot_context_.EnumerateClasses(cb, flags)) {
            return false;
        }

        {
            os::memory::LockHolder lock(contexts_lock_);
            for (auto *ctx : contexts_) {
                if (!ctx->EnumerateClasses(cb, flags)) {
                    return false;
                }
            }
        }
        return true;
    }

    template <class ContextGetterFn>
    void RegisterContext(const ContextGetterFn &fn)
    {
        os::memory::LockHolder lock(contexts_lock_);
        auto *context = fn();
        if (context != nullptr) {
            contexts_.push_back(context);
        }
    }

    template <class Callback>
    void EnumerateContexts(const Callback &cb)
    {
        if (!cb(&boot_context_)) {
            return;
        }

        os::memory::LockHolder lock(contexts_lock_);
        for (auto *context : contexts_) {
            if (!cb(context)) {
                return;
            }
        }
    }

    size_t NumLoadedClasses();

    void VisitLoadedClasses(size_t flag);

    ClassLinkerContext *ResolveContext(ClassLinkerContext *context)
    {
        if (context == nullptr) {
            return &boot_context_;
        }

        return context;
    }

    void OnClassPrepared(Class *klass);

    virtual Class *FromClassObject(ObjectHeader *obj);

    virtual size_t GetClassObjectSizeFromClassSize(uint32_t size);

    NO_COPY_SEMANTIC(ClassLinkerExtension);
    NO_MOVE_SEMANTIC(ClassLinkerExtension);

protected:
    void InitializePrimitiveClassRoot(ClassRoot root, panda_file::Type::TypeId type_id, const char *descriptor);

    void InitializeArrayClassRoot(ClassRoot root, ClassRoot component_root, const char *descriptor);

    void FreeLoadedClasses();

    Class *AddClass(Class *klass);

    // Add the class to the list, when it is just be created and not added to class linker context.
    void AddCreatedClass(Class *klass);

    // Remove class in the list, when it has been added to class linker context.
    void RemoveCreatedClass(Class *klass);

    using PandaFilePtr = std::unique_ptr<const panda_file::File>;
    virtual ClassLinkerContext *CreateApplicationClassLinkerContext(PandaVector<PandaFilePtr> &&app_files);

private:
    class BootContext : public ClassLinkerContext {
    public:
        explicit BootContext(ClassLinkerExtension *extension) : extension_(extension)
        {
#ifndef NDEBUG
            lang_ = extension->GetLanguage();
#endif  // NDEBUG
        }
        ~BootContext() override = default;
        NO_COPY_SEMANTIC(BootContext);
        NO_MOVE_SEMANTIC(BootContext);

        bool IsBootContext() const override
        {
            return true;
        }

        Class *LoadClass(const uint8_t *descriptor, bool need_copy_descriptor,
                         ClassLinkerErrorHandler *error_handler) override;

    private:
        ClassLinkerExtension *extension_;
    };

    class AppContext : public ClassLinkerContext {
    public:
        explicit AppContext(ClassLinkerExtension *extension, PandaVector<const panda_file::File *> &&pf_list)
            : extension_(extension), pfs_(pf_list)
        {
#ifndef NDEBUG
            lang_ = extension_->GetLanguage();
#endif  // NDEBUG
        }
        ~AppContext() override = default;
        NO_COPY_SEMANTIC(AppContext);
        NO_MOVE_SEMANTIC(AppContext);

        Class *LoadClass(const uint8_t *descriptor, bool need_copy_descriptor,
                         ClassLinkerErrorHandler *error_handler) override;

        PandaVector<std::string_view> GetPandaFilePaths() const override
        {
            PandaVector<std::string_view> file_paths;
            for (auto &pf : pfs_) {
                if (pf != nullptr) {
                    file_paths.emplace_back(pf->GetFilename());
                }
            }
            return file_paths;
        }

    private:
        ClassLinkerExtension *extension_;
        PandaVector<const panda_file::File *> pfs_;
    };

    static constexpr size_t CLASS_ROOT_COUNT = static_cast<size_t>(ClassRoot::LAST_CLASS_ROOT_ENTRY) + 1;

    virtual bool InitializeImpl(bool compressed_string_enabled) = 0;

    static constexpr size_t ToIndex(ClassRoot root)
    {
        return static_cast<size_t>(root);
    }
    ClassLinkerErrorHandler *ResolveErrorHandler(ClassLinkerErrorHandler *error_handler)
    {
        if (error_handler == nullptr) {
            return GetErrorHandler();
        }

        return error_handler;
    }

    panda_file::SourceLang lang_;
    BootContext boot_context_;

    std::array<Class *, CLASS_ROOT_COUNT> class_roots_ {};
    ClassLinker *class_linker_ {nullptr};

    os::memory::RecursiveMutex contexts_lock_;
    PandaVector<ClassLinkerContext *> contexts_ GUARDED_BY(contexts_lock_);

    os::memory::RecursiveMutex created_classes_lock_;
    PandaVector<Class *> created_classes_ GUARDED_BY(created_classes_lock_);

    bool can_initialize_classes_ {false};
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_LINKER_EXTENSION_H_
