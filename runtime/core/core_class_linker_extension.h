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

#ifndef PANDA_RUNTIME_CORE_CORE_CLASS_LINKER_EXTENSION_H_
#define PANDA_RUNTIME_CORE_CORE_CLASS_LINKER_EXTENSION_H_

#include "runtime/include/class_linker-inl.h"
#include "runtime/include/class_linker_extension.h"

namespace panda {

class CoreClassLinkerExtension : public ClassLinkerExtension {
public:
    CoreClassLinkerExtension() : ClassLinkerExtension(panda_file::SourceLang::PANDA_ASSEMBLY) {}

    ~CoreClassLinkerExtension() override;

    void InitializeArrayClass(Class *array_class, Class *component_class) override;

    void InitializePrimitiveClass(Class *primitive_class) override;

    size_t GetClassVTableSize(ClassRoot root) override;

    size_t GetClassIMTSize(ClassRoot root) override;

    size_t GetClassSize(ClassRoot root) override;

    size_t GetArrayClassVTableSize() override;

    size_t GetArrayClassIMTSize() override;

    size_t GetArrayClassSize() override;

    Class *CreateClass(const uint8_t *descriptor, size_t vtable_size, size_t imt_size, size_t size) override;

    void FreeClass(Class *klass) override;

    void InitializeClass([[maybe_unused]] Class *klass) override {}

    const void *GetNativeEntryPointFor([[maybe_unused]] Method *method) const override
    {
        return reinterpret_cast<const void *>(intrinsics::UnknownIntrinsic);
    }

    bool CanThrowException([[maybe_unused]] const Method *method) const override
    {
        return true;
    }

    ClassLinkerErrorHandler *GetErrorHandler() override
    {
        return &error_handler_;
    };

    NO_COPY_SEMANTIC(CoreClassLinkerExtension);
    NO_MOVE_SEMANTIC(CoreClassLinkerExtension);

private:
    bool InitializeImpl(bool compressed_string_enabled) override;

    class ErrorHandler : public ClassLinkerErrorHandler {
    public:
        void OnError(ClassLinker::Error error, const PandaString &message) override;
    };

    ErrorHandler error_handler_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_CORE_CORE_CLASS_LINKER_EXTENSION_H_
