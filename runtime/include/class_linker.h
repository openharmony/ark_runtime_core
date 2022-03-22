/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_LINKER_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_LINKER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libpandabase/mem/arena_allocator.h"
#include "libpandabase/os/mutex.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "runtime/class_linker_context.h"
#include "runtime/include/class.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/field.h"
#include "runtime/include/itable_builder.h"
#include "runtime/include/imtable_builder.h"
#include "runtime/include/language_context.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/method.h"
#include "runtime/include/vtable_builder.h"

namespace panda {

class ClassLinkerErrorHandler;

class ClassLinker {
public:
    enum class Error {
        CLASS_NOT_FOUND,
        FIELD_NOT_FOUND,
        METHOD_NOT_FOUND,
        NO_CLASS_DEF,
    };

    ClassLinker(mem::InternalAllocatorPtr allocator, std::vector<std::unique_ptr<ClassLinkerExtension>> &&extensions);

    ~ClassLinker();

    bool Initialize(bool compressed_string_enabled = true);

    bool InitializeRoots(ManagedThread *thread);

    Class *GetClass(const uint8_t *descriptor, bool need_copy_descriptor, ClassLinkerContext *context,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    Class *GetClass(const panda_file::File &pf, panda_file::File::EntityId id, ClassLinkerContext *context,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    Class *GetClass(const Method &caller, panda_file::File::EntityId id,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    Class *LoadClass(const panda_file::File &pf, panda_file::File::EntityId class_id, ClassLinkerContext *context,
                     ClassLinkerErrorHandler *error_handler = nullptr)
    {
        return LoadClass(&pf, class_id, pf.GetStringData(class_id).data, context, error_handler);
    }

    Method *GetMethod(const panda_file::File &pf, panda_file::File::EntityId id, ClassLinkerContext *context = nullptr,
                      ClassLinkerErrorHandler *error_handler = nullptr);

    Method *GetMethod(const Method &caller, panda_file::File::EntityId id,
                      ClassLinkerErrorHandler *error_handler = nullptr);

    Method *GetMethod(std::string_view panda_file, panda_file::File::EntityId id);

    Field *GetField(const panda_file::File &pf, panda_file::File::EntityId id, ClassLinkerContext *context = nullptr,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    Field *GetField(const Method &caller, panda_file::File::EntityId id,
                    ClassLinkerErrorHandler *error_handler = nullptr);

    void AddPandaFile(std::unique_ptr<const panda_file::File> &&pf, ClassLinkerContext *context = nullptr);

    template <typename Callback>
    void EnumeratePandaFiles(Callback cb) const
    {
        os::memory::LockHolder lock(panda_files_lock_);
        for (const auto &file_data : panda_files_) {
            if (!cb(*(file_data.pf.get()))) {
                break;
            }
        }
    }

    template <typename Callback>
    void EnumerateBootPandaFiles(Callback cb) const
    {
        for (const auto &file : boot_panda_files_) {
            if (!cb(*file)) {
                break;
            }
        }
    }

    const PandaVector<const panda_file::File *> &GetBootPandaFiles() const
    {
        return boot_panda_files_;
    }

    template <class Callback>
    void EnumerateClasses(const Callback &cb, mem::VisitGCRootFlags flags = mem::VisitGCRootFlags::ACCESS_ROOT_ALL)
    {
        for (auto &ext : extensions_) {
            if (ext == nullptr) {
                continue;
            }

            if (!ext->EnumerateClasses(cb, flags)) {
                return;
            }
        }
    }

    template <class Callback>
    void EnumerateContexts(const Callback &cb)
    {
        for (auto &ext : extensions_) {
            if (ext == nullptr) {
                continue;
            }
            ext->EnumerateContexts(cb);
        }
    }

    template <class Callback>
    void EnumerateContextsForDump(const Callback &cb, std::ostream &os)
    {
        size_t register_index = 0;
        ClassLinkerContext *parent = nullptr;
        ClassLinkerExtension *ext = nullptr;
        auto enum_callback = [&register_index, &parent, &cb, &os, &ext](ClassLinkerContext *ctx) {
            os << "#" << register_index << " ";
            if (!cb(ctx, os, parent)) {
                // if not a java class loader, break it;
                return true;
            }
            if (parent != nullptr) {
                size_t parent_index = 0;
                bool founded = false;
                ext->EnumerateContexts([parent, &parent_index, &founded](ClassLinkerContext *ctx_ptr) {
                    if (parent == ctx_ptr) {
                        founded = true;
                        return false;
                    }
                    parent_index++;
                    return true;
                });
                if (founded) {
                    os << "|Parent class loader: #" << parent_index << "\n";
                } else {
                    os << "|Parent class loader: unknown\n";
                }
            } else {
                os << "|Parent class loader: empty\n";
            }
            register_index++;
            return true;
        };
        for (auto &ext_ptr : extensions_) {
            if (ext_ptr == nullptr) {
                continue;
            }
            ext = ext_ptr.get();
            ext->EnumerateContexts(enum_callback);
        }
    }

    bool InitializeClass(ManagedThread *thread, Class *klass);

    bool HasExtension(const LanguageContext &ctx)
    {
        return extensions_[ToExtensionIndex(ctx.GetLanguage())].get() != nullptr;
    }

    bool HasExtension(panda_file::SourceLang lang)
    {
        return extensions_[ToExtensionIndex(lang)].get() != nullptr;
    }

    ClassLinkerExtension *GetExtension(const LanguageContext &ctx)
    {
        ClassLinkerExtension *extension = extensions_[ToExtensionIndex(ctx.GetLanguage())].get();
        ASSERT(extension != nullptr);
        return extension;
    };

    ClassLinkerExtension *GetExtension(panda_file::SourceLang lang)
    {
        ClassLinkerExtension *extension = extensions_[ToExtensionIndex(lang)].get();
        ASSERT(extension != nullptr);
        return extension;
    };

    Class *ObjectToClass(const ObjectHeader *object)
    {
        ASSERT(object->ClassAddr<Class>()->IsClassClass());
        return extensions_[ToExtensionIndex(object->ClassAddr<BaseClass>()->GetSourceLang())]->FromClassObject(
            const_cast<ObjectHeader *>(object));
    }

    size_t GetClassObjectSize(Class *cls)
    {
        return extensions_[ToExtensionIndex(cls->GetSourceLang())]->GetClassObjectSizeFromClassSize(
            cls->GetClassSize());
    }

    void AddClassRoot(ClassRoot root, Class *klass);

    Class *CreateArrayClass(ClassLinkerExtension *ext, const uint8_t *descriptor, bool need_copy_descriptor,
                            Class *component_class);

    void FreeClassData(Class *class_ptr);

    void FreeClass(Class *class_ptr);

    mem::InternalAllocatorPtr GetAllocator() const
    {
        return allocator_;
    }

    bool IsInitialized() const
    {
        return is_initialized_;
    }

    Class *FindLoadedClass(const uint8_t *descriptor, ClassLinkerContext *context = nullptr);

    size_t NumLoadedClasses();

    void VisitLoadedClasses(size_t flag);

    Class *BuildClass(const uint8_t *descriptor, bool need_copy_descriptor, uint32_t access_flags, Span<Method> methods,
                      Span<Field> fields, Class *base_class, Span<Class *> interfaces, ClassLinkerContext *context,
                      bool is_interface);

    static constexpr size_t GetLangCount()
    {
        return LANG_EXTENSIONS_COUNT;
    }

    bool IsPandaFileRegistered(const panda_file::File *file)
    {
        os::memory::LockHolder lock(panda_files_lock_);
        for (const auto &data : panda_files_) {
            if (data.pf.get() == file) {
                return true;
            }
        }

        return false;
    }

    ClassLinkerContext *GetAppContext(std::string_view panda_file)
    {
        ClassLinkerContext *app_context = nullptr;
        EnumerateContexts([panda_file, &app_context](ClassLinkerContext *context) -> bool {
            auto file_paths = context->GetPandaFilePaths();
            for (auto &file : file_paths) {
                if (file == panda_file) {
                    app_context = context;
                    return false;
                }
            }
            return true;
        });
        return app_context;
    }

    void RemoveCreatedClassInExtension(Class *klass);

private:
    static constexpr size_t LANG_EXTENSIONS_COUNT = static_cast<size_t>(panda_file::SourceLang::LAST) + 1;

    struct ClassInfo {
        size_t size;
        size_t num_sfields;
        PandaUniquePtr<VTableBuilder> vtable_builder;
        PandaUniquePtr<ITableBuilder> itable_builder;
        PandaUniquePtr<IMTableBuilder> imtable_builder;
    };

    Field *GetFieldById(Class *klass, const panda_file::FieldDataAccessor &field_data_accessor,
                        ClassLinkerErrorHandler *error_handler);

    Field *GetFieldBySignature(Class *klass, const panda_file::FieldDataAccessor &field_data_accessor,
                               ClassLinkerErrorHandler *error_handler);

    Method *GetMethod(const Class *klass, const panda_file::MethodDataAccessor &method_data_accessor,
                      ClassLinkerErrorHandler *error_handler);

    bool LinkBootClass(Class *klass);

    Class *LoadArrayClass(const uint8_t *descriptor, bool need_copy_descriptor, ClassLinkerContext *context,
                          ClassLinkerErrorHandler *error_handler);

    Class *LoadClass(const panda_file::File *pf, panda_file::File::EntityId class_id, const uint8_t *descriptor,
                     ClassLinkerContext *context, ClassLinkerErrorHandler *error_handler);

    Class *LoadClass(panda_file::ClassDataAccessor *class_data_accessor, const uint8_t *descriptor, Class *base_class,
                     Span<Class *> interfaces, ClassLinkerContext *context, ClassLinkerExtension *ext,
                     ClassLinkerErrorHandler *error_handler);

    Class *LoadBaseClass(panda_file::ClassDataAccessor *cda, LanguageContext ctx, ClassLinkerContext *context,
                         ClassLinkerErrorHandler *error_handler);

    std::optional<Span<Class *>> LoadInterfaces(panda_file::ClassDataAccessor *cda, ClassLinkerContext *context,
                                                ClassLinkerErrorHandler *error_handler);

    bool LinkFields(Class *klass, ClassLinkerErrorHandler *error_handler);

    bool LoadFields(Class *klass, panda_file::ClassDataAccessor *data_accessor, ClassLinkerErrorHandler *error_handler);

    bool LinkMethods(Class *klass, ClassInfo *class_info, ClassLinkerErrorHandler *error_handler);

    bool LoadMethods(Class *klass, ClassInfo *class_info, panda_file::ClassDataAccessor *data_accessor,
                     ClassLinkerErrorHandler *error_handler);

    ClassInfo GetClassInfo(panda_file::ClassDataAccessor *data_accessor, Class *base, Span<Class *> interfaces,
                           ClassLinkerContext *context);

    ClassInfo GetClassInfo(Span<Method> methods, Span<Field> fields, Class *base, Span<Class *> interfaces,
                           bool is_interface);

    void OnError(ClassLinkerErrorHandler *error_handler, Error error, const PandaString &msg);

    static bool LayoutFields(Class *klass, Span<Field> fields, bool is_static, ClassLinkerErrorHandler *error_handler);

    static constexpr size_t ToExtensionIndex(panda_file::SourceLang lang)
    {
        return static_cast<size_t>(lang);
    }

    mem::InternalAllocatorPtr allocator_;

    PandaVector<const panda_file::File *> boot_panda_files_;

    struct PandaFileLoadData {
        ClassLinkerContext *context;
        std::unique_ptr<const panda_file::File> pf;
    };

    mutable os::memory::Mutex panda_files_lock_;
    PandaVector<PandaFileLoadData> panda_files_ GUARDED_BY(panda_files_lock_);

    // Just to free them at destroy
    os::memory::Mutex copied_names_lock_;
    PandaList<const uint8_t *> copied_names_ GUARDED_BY(copied_names_lock_);

    std::array<std::unique_ptr<ClassLinkerExtension>, LANG_EXTENSIONS_COUNT> extensions_;

    bool is_initialized_ {false};

    NO_COPY_SEMANTIC(ClassLinker);
    NO_MOVE_SEMANTIC(ClassLinker);
};

class ClassLinkerErrorHandler {
public:
    virtual void OnError(ClassLinker::Error error, const PandaString &message) = 0;

public:
    ClassLinkerErrorHandler() = default;
    virtual ~ClassLinkerErrorHandler() = default;

    NO_MOVE_SEMANTIC(ClassLinkerErrorHandler);
    NO_COPY_SEMANTIC(ClassLinkerErrorHandler);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_LINKER_H_
