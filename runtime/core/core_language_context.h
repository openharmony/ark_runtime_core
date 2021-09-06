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

#ifndef PANDA_RUNTIME_CORE_CORE_LANGUAGE_CONTEXT_H_
#define PANDA_RUNTIME_CORE_CORE_LANGUAGE_CONTEXT_H_

#include "runtime/include/language_context.h"

#include "runtime/class_initializer.h"
#include "runtime/core/core_class_linker_extension.h"

namespace panda {

class CoreLanguageContext : public LanguageContextBase {
public:
    CoreLanguageContext() = default;

    DEFAULT_COPY_SEMANTIC(CoreLanguageContext);
    DEFAULT_MOVE_SEMANTIC(CoreLanguageContext);

    ~CoreLanguageContext() override = default;

    panda_file::SourceLang GetLanguage() const override
    {
        return panda_file::SourceLang::PANDA_ASSEMBLY;
    }

    const uint8_t *GetStringClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/String;");
    }

    const uint8_t *GetObjectClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/Object;");
    }

    const uint8_t *GetClassClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/Class;");
    }

    const uint8_t *GetClassArrayClassDescriptor() const override
    {
        return utf::CStringAsMutf8("[Lpanda/Class;");
    }

    const uint8_t *GetStringArrayClassDescriptor() const override
    {
        return utf::CStringAsMutf8("[Lpanda/String;");
    }

    const uint8_t *GetCtorName() const override
    {
        return utf::CStringAsMutf8(".ctor");
    }

    const uint8_t *GetCctorName() const override
    {
        return utf::CStringAsMutf8(".cctor");
    }

    const uint8_t *GetNullPointerExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/NullPointerException;");
    }

    const uint8_t *GetArrayIndexOutOfBoundsExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ArrayIndexOutOfBoundsException;");
    }

    const uint8_t *GetIndexOutOfBoundsExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/IndexOutOfBoundsException;");
    }

    const uint8_t *GetIllegalStateExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/IllegalStateException;");
    }
    const uint8_t *GetNegativeArraySizeExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/NegativeArraySizeException;");
    }

    const uint8_t *GetStringIndexOutOfBoundsExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/StringIndexOutOfBoundsException;");
    }

    const uint8_t *GetArithmeticExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ArithmeticException;");
    }

    const uint8_t *GetClassCastExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ClassCastException;");
    }

    const uint8_t *GetAbstractMethodErrorClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/AbstractMethodError;");
    }

    const uint8_t *GetArrayStoreExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ArrayStoreException;");
    }

    const uint8_t *GetRuntimeExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/RuntimeException;");
    }

    const uint8_t *GetFileNotFoundExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/FileNotFoundException;");
    }

    const uint8_t *GetIOExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/IOException;");
    }

    const uint8_t *GetIllegalArgumentExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/IllegalArgumentException;");
    }

    const uint8_t *GetOutOfMemoryErrorClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/OutOfMemoryError;");
    }

    const uint8_t *GetNoClassDefFoundErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/NoClassDefFoundError;");
    }

    const uint8_t *GetClassCircularityErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ClassCircularityError;");
    }

    const uint8_t *GetNoSuchFieldErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/NoSuchFieldError;");
    }

    const uint8_t *GetNoSuchMethodErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/NoSuchMethodError;");
    }

    const uint8_t *GetExceptionInInitializerErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ExceptionInInitializerError;");
    }

    const uint8_t *GetClassNotFoundExceptionDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/ClassNotFoundException;");
    }

    const uint8_t *GetInstantiationErrorDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/InstantiationError;");
    }

    const uint8_t *GetUnsupportedOperationExceptionClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/UnsupportedOperationException;");
    }

    const uint8_t *GetVerifyErrorClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/VerifyError;");
    }

    const uint8_t *GetErrorClassDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/Error;");
    }

    coretypes::TaggedValue GetInitialTaggedValue() const override
    {
        return coretypes::TaggedValue(coretypes::TaggedValue::VALUE_UNDEFINED);
    }

    DecodedTaggedValue GetInitialDecodedValue() const override
    {
        return DecodedTaggedValue(coretypes::TaggedValue::VALUE_UNDEFINED, 0);
    }

    DecodedTaggedValue GetDecodedTaggedValue([[maybe_unused]] const coretypes::TaggedValue &value) const override
    {
        // Unreachable
        return DecodedTaggedValue(coretypes::TaggedValue::VALUE_UNDEFINED, 0);
    }

    coretypes::TaggedValue GetEncodedTaggedValue([[maybe_unused]] int64_t value,
                                                 [[maybe_unused]] int64_t tag) const override
    {
        return coretypes::TaggedValue(coretypes::TaggedValue::VALUE_UNDEFINED);
    }

    PandaVM *CreateVM(Runtime *runtime, const RuntimeOptions &options) const override;

    mem::GC *CreateGC(mem::GCType gc_type, mem::ObjectAllocatorBase *object_allocator,
                      const mem::GCSettings &settings) const override;

    void SetExceptionToVReg([[maybe_unused]] Frame::VRegister &vreg, [[maybe_unused]] ObjectHeader *obj) const override
    {
        vreg.SetReference(obj);
    }

    bool IsCallableObject([[maybe_unused]] ObjectHeader *obj) const override
    {
        return false;
    }

    Method *GetCallTarget([[maybe_unused]] ObjectHeader *obj) const override
    {
        return nullptr;
    }

    const uint8_t *GetReferenceErrorDescriptor() const override
    {
        return nullptr;
    }

    const uint8_t *GetTypedErrorDescriptor() const override
    {
        return nullptr;
    }

    const uint8_t *GetIllegalMonitorStateExceptionDescriptor() const override
    {
        return utf::CStringAsMutf8("Lpanda/IllegalMonitorStateException;");
    }

    void ThrowException(ManagedThread *thread, const uint8_t *mutf8_name, const uint8_t *mutf8_msg) const override;

    PandaUniquePtr<ITableBuilder> CreateITableBuilder() const override;

    PandaUniquePtr<VTableBuilder> CreateVTableBuilder() const override;

    bool InitializeClass(ClassLinker *class_linker, ManagedThread *thread, Class *klass) const override
    {
        return ClassInitializer::Initialize(class_linker, thread, klass);
    }

    std::unique_ptr<ClassLinkerExtension> CreateClassLinkerExtension() const override
    {
        return std::make_unique<CoreClassLinkerExtension>();
    }

    PandaUniquePtr<tooling::PtLangExt> CreatePtLangExt() const override
    {
        return nullptr;
    }
};

}  // namespace panda

#endif  // PANDA_RUNTIME_CORE_CORE_LANGUAGE_CONTEXT_H_
