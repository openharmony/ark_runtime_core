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

#ifndef PANDA_RUNTIME_DYN_CLASS_LINKER_EXTENSION_H_
#define PANDA_RUNTIME_DYN_CLASS_LINKER_EXTENSION_H_

#include "libpandafile/file_items.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/hclass.h"
#include "runtime/include/mem/allocator.h"

namespace panda {
class ClassLinker;

using DynClass = coretypes::DynClass;

class DynamicClassLinkerExtension : public ClassLinkerExtension {
public:
    static DynamicClassLinkerExtension *GetDynamicExtension(LanguageContext ctx);

    explicit DynamicClassLinkerExtension(panda_file::SourceLang lang) : ClassLinkerExtension(lang) {}

    ~DynamicClassLinkerExtension() override;

    void InitializeArrayClass(Class *arrayClass, Class *componentClass) override;

    void InitializePrimitiveClass(Class *primitiveClass) override;

    size_t GetClassVTableSize(ClassRoot root) override;

    size_t GetClassIMTSize(ClassRoot root) override;

    size_t GetClassSize(ClassRoot root) override;

    size_t GetArrayClassVTableSize() override;

    size_t GetArrayClassSize() override;

    Class *CreateClass(const uint8_t *descriptor, size_t vtableSize, size_t imt_size, size_t size) override;

    void FreeClass(Class *klass) override;

    void InitializeClass([[maybe_unused]] Class *klass) override {}

    const void *GetNativeEntryPointFor([[maybe_unused]] Method *method) const override
    {
        return nullptr;
    }

    ClassLinkerErrorHandler *GetErrorHandler() override
    {
        return nullptr;
    }

    template <class Callback>
    void EnumerateClasses([[maybe_unused]] const Callback &cb,
                          [[maybe_unused]] mem::VisitGCRootFlags flags = mem::VisitGCRootFlags::ACCESS_ROOT_ALL)
    {
    }

private:
    bool InitializeImpl(bool cmpStrEnabled) override;

    class ErrorHandler : public ClassLinkerErrorHandler {
    public:
        void OnError(ClassLinker::Error error, const PandaString &message) override;
    };

    ErrorHandler errorHandler_;

    NO_COPY_SEMANTIC(DynamicClassLinkerExtension);
    NO_MOVE_SEMANTIC(DynamicClassLinkerExtension);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_DYN_CLASS_LINKER_EXTENSION_H_
