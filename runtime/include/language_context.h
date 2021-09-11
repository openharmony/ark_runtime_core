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

#ifndef PANDA_RUNTIME_INCLUDE_LANGUAGE_CONTEXT_H_
#define PANDA_RUNTIME_INCLUDE_LANGUAGE_CONTEXT_H_

#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/file_items.h"
#include "libpandabase/utils/utf.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/imtable_builder.h"
#include "runtime/include/itable_builder.h"
#include "runtime/include/tooling/pt_lang_extension.h"
#include "runtime/include/vtable_builder.h"
#include "runtime/mem/gc/gc_types.h"

namespace panda {
class Thread;
class Runtime;
class RuntimeOptions;

namespace mem {
class GC;
class ObjectAllocatorBase;
struct GCSettings;
}  // namespace mem

class LanguageContextBase {
public:
    LanguageContextBase() = default;

    DEFAULT_COPY_SEMANTIC(LanguageContextBase);
    DEFAULT_MOVE_SEMANTIC(LanguageContextBase);

    virtual ~LanguageContextBase() = default;

    virtual panda_file::SourceLang GetLanguage() const = 0;

    virtual const uint8_t *GetStringClassDescriptor() const = 0;

    virtual const uint8_t *GetObjectClassDescriptor() const = 0;

    virtual const uint8_t *GetClassClassDescriptor() const = 0;

    virtual const uint8_t *GetClassArrayClassDescriptor() const = 0;

    virtual const uint8_t *GetStringArrayClassDescriptor() const = 0;

    virtual const uint8_t *GetCtorName() const = 0;

    virtual const uint8_t *GetCctorName() const = 0;

    virtual const uint8_t *GetNullPointerExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetArrayIndexOutOfBoundsExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetIndexOutOfBoundsExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetIllegalStateExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetNegativeArraySizeExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetStringIndexOutOfBoundsExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetArithmeticExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetClassCastExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetAbstractMethodErrorClassDescriptor() const = 0;

    virtual const uint8_t *GetArrayStoreExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetRuntimeExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetFileNotFoundExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetIOExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetIllegalArgumentExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetOutOfMemoryErrorClassDescriptor() const = 0;

    virtual const uint8_t *GetNoClassDefFoundErrorDescriptor() const = 0;

    virtual const uint8_t *GetClassCircularityErrorDescriptor() const = 0;

    virtual const uint8_t *GetNoSuchFieldErrorDescriptor() const = 0;

    virtual const uint8_t *GetNoSuchMethodErrorDescriptor() const = 0;

    virtual coretypes::TaggedValue GetInitialTaggedValue() const = 0;

    virtual DecodedTaggedValue GetInitialDecodedValue() const = 0;

    virtual DecodedTaggedValue GetDecodedTaggedValue(const coretypes::TaggedValue &value) const = 0;

    virtual coretypes::TaggedValue GetEncodedTaggedValue(int64_t value, int64_t tag) const = 0;

    virtual std::pair<Method *, uint32_t> GetCatchMethodAndOffset(Method *method, ManagedThread *thread) const;

    virtual PandaVM *CreateVM(Runtime *runtime, const RuntimeOptions &options) const = 0;

    virtual mem::GC *CreateGC(mem::GCType gc_type, mem::ObjectAllocatorBase *object_allocator,
                              const mem::GCSettings &settings) const = 0;

    virtual std::unique_ptr<ClassLinkerExtension> CreateClassLinkerExtension() const;

    virtual PandaUniquePtr<tooling::PtLangExt> CreatePtLangExt() const;

    virtual void ThrowException(ManagedThread *thread, const uint8_t *mutf8_name, const uint8_t *mutf8_msg) const;

    virtual void SetExceptionToVReg(
        [[maybe_unused]] Frame::VRegister &vreg,  // NOLINTNEXTLINE(google-runtime-references)
        [[maybe_unused]] ObjectHeader *obj) const = 0;

    virtual uint64_t GetTypeTag(interpreter::TypeTag tag) const
    {
        // return default TypeTag
        return tag;
    }

    virtual bool IsCallableObject([[maybe_unused]] ObjectHeader *obj) const = 0;

    virtual Method *GetCallTarget(ObjectHeader *obj) const = 0;

    virtual const uint8_t *GetExceptionInInitializerErrorDescriptor() const = 0;

    virtual const uint8_t *GetClassNotFoundExceptionDescriptor() const = 0;

    virtual const uint8_t *GetInstantiationErrorDescriptor() const = 0;

    virtual const uint8_t *GetUnsupportedOperationExceptionClassDescriptor() const = 0;

    virtual const uint8_t *GetVerifyErrorClassDescriptor() const = 0;

    virtual const uint8_t *GetReferenceErrorDescriptor() const = 0;

    virtual const uint8_t *GetTypedErrorDescriptor() const = 0;

    virtual const uint8_t *GetIllegalMonitorStateExceptionDescriptor() const = 0;

    virtual const uint8_t *GetErrorClassDescriptor() const;

    bool IsDynamicLanguage() const
    {
        switch (GetLanguage()) {
            case panda_file::SourceLang::PANDA_ASSEMBLY:
                return false;
            case panda_file::SourceLang::ECMASCRIPT:
                return true;
            default:
                UNREACHABLE();
                return false;
        }
    }

    virtual PandaUniquePtr<IMTableBuilder> CreateIMTableBuilder() const
    {
        return MakePandaUnique<IMTableBuilder>();
    }

    virtual PandaUniquePtr<ITableBuilder> CreateITableBuilder() const;

    virtual PandaUniquePtr<VTableBuilder> CreateVTableBuilder() const;

    virtual bool InitializeClass([[maybe_unused]] ClassLinker *class_linker, [[maybe_unused]] ManagedThread *thread,
                                 [[maybe_unused]] Class *klass) const
    {
        return true;
    }
};

class LanguageContext {
public:
    explicit LanguageContext(const LanguageContextBase *context) : base_(context) {}

    DEFAULT_COPY_SEMANTIC(LanguageContext);
    DEFAULT_MOVE_SEMANTIC(LanguageContext);

    ~LanguageContext() = default;

    panda_file::SourceLang GetLanguage() const
    {
        return base_->GetLanguage();
    }

    coretypes::TaggedValue GetInitialTaggedValue() const
    {
        return base_->GetInitialTaggedValue();
    }

    DecodedTaggedValue GetDecodedTaggedValue(const coretypes::TaggedValue &value) const
    {
        return base_->GetDecodedTaggedValue(value);
    }

    coretypes::TaggedValue GetEncodedTaggedValue(int64_t value, int64_t tag) const
    {
        return base_->GetEncodedTaggedValue(value, tag);
    }

    std::pair<Method *, uint32_t> GetCatchMethodAndOffset(Method *method, ManagedThread *thread) const
    {
        return base_->GetCatchMethodAndOffset(method, thread);
    }

    PandaVM *CreateVM(Runtime *runtime, const RuntimeOptions &options)
    {
        return base_->CreateVM(runtime, options);
    }

    mem::GC *CreateGC(mem::GCType gc_type, mem::ObjectAllocatorBase *object_allocator,
                      const mem::GCSettings &settings) const
    {
        return base_->CreateGC(gc_type, object_allocator, settings);
    }

    std::unique_ptr<ClassLinkerExtension> CreateClassLinkerExtension()
    {
        return base_->CreateClassLinkerExtension();
    }

    PandaUniquePtr<tooling::PtLangExt> CreatePtLangExt()
    {
        return base_->CreatePtLangExt();
    }

    void ThrowException(ManagedThread *thread, const uint8_t *mutf8_name, const uint8_t *mutf8_msg)
    {
        base_->ThrowException(thread, mutf8_name, mutf8_msg);
    }

    void SetExceptionToVReg([[maybe_unused]] Frame::VRegister &vreg,  // NOLINTNEXTLINE(google-runtime-references)
                            [[maybe_unused]] ObjectHeader *obj) const
    {
        base_->SetExceptionToVReg(vreg, obj);
    }

    DecodedTaggedValue GetInitialDecodedValue() const
    {
        return base_->GetInitialDecodedValue();
    }

    const uint8_t *GetStringClassDescriptor() const
    {
        return base_->GetStringClassDescriptor();
    }

    const uint8_t *GetObjectClassDescriptor() const
    {
        return base_->GetObjectClassDescriptor();
    }

    const uint8_t *GetClassClassDescriptor() const
    {
        return base_->GetClassClassDescriptor();
    }

    const uint8_t *GetClassArrayClassDescriptor() const
    {
        return base_->GetClassArrayClassDescriptor();
    }

    const uint8_t *GetStringArrayClassDescriptor() const
    {
        return base_->GetStringArrayClassDescriptor();
    }

    const uint8_t *GetCtorName() const
    {
        return base_->GetCtorName();
    }

    const uint8_t *GetCctorName() const
    {
        return base_->GetCctorName();
    }

    const uint8_t *GetNullPointerExceptionClassDescriptor() const
    {
        return base_->GetNullPointerExceptionClassDescriptor();
    }

    const uint8_t *GetArrayIndexOutOfBoundsExceptionClassDescriptor() const
    {
        return base_->GetArrayIndexOutOfBoundsExceptionClassDescriptor();
    }

    const uint8_t *GetIndexOutOfBoundsExceptionClassDescriptor() const
    {
        return base_->GetIndexOutOfBoundsExceptionClassDescriptor();
    }

    const uint8_t *GetIllegalStateExceptionClassDescriptor() const
    {
        return base_->GetIllegalStateExceptionClassDescriptor();
    }

    const uint8_t *GetNegativeArraySizeExceptionClassDescriptor() const
    {
        return base_->GetNegativeArraySizeExceptionClassDescriptor();
    }

    const uint8_t *GetStringIndexOutOfBoundsExceptionClassDescriptor() const
    {
        return base_->GetStringIndexOutOfBoundsExceptionClassDescriptor();
    }

    const uint8_t *GetArithmeticExceptionClassDescriptor() const
    {
        return base_->GetArithmeticExceptionClassDescriptor();
    }

    const uint8_t *GetClassCastExceptionClassDescriptor() const
    {
        return base_->GetClassCastExceptionClassDescriptor();
    }

    const uint8_t *GetAbstractMethodErrorClassDescriptor() const
    {
        return base_->GetAbstractMethodErrorClassDescriptor();
    }

    const uint8_t *GetArrayStoreExceptionClassDescriptor() const
    {
        return base_->GetArrayStoreExceptionClassDescriptor();
    }

    const uint8_t *GetRuntimeExceptionClassDescriptor() const
    {
        return base_->GetRuntimeExceptionClassDescriptor();
    }

    const uint8_t *GetFileNotFoundExceptionClassDescriptor() const
    {
        return base_->GetFileNotFoundExceptionClassDescriptor();
    }

    const uint8_t *GetIOExceptionClassDescriptor() const
    {
        return base_->GetIOExceptionClassDescriptor();
    }

    const uint8_t *GetIllegalArgumentExceptionClassDescriptor() const
    {
        return base_->GetIllegalArgumentExceptionClassDescriptor();
    }

    const uint8_t *GetOutOfMemoryErrorClassDescriptor() const
    {
        return base_->GetOutOfMemoryErrorClassDescriptor();
    }

    const uint8_t *GetNoClassDefFoundErrorDescriptor() const
    {
        return base_->GetNoClassDefFoundErrorDescriptor();
    }

    const uint8_t *GetClassCircularityErrorDescriptor() const
    {
        return base_->GetClassCircularityErrorDescriptor();
    }

    const uint8_t *GetNoSuchFieldErrorDescriptor() const
    {
        return base_->GetNoSuchFieldErrorDescriptor();
    }

    const uint8_t *GetNoSuchMethodErrorDescriptor() const
    {
        return base_->GetNoSuchMethodErrorDescriptor();
    }

    const uint8_t *GetExceptionInInitializerErrorDescriptor() const
    {
        return base_->GetExceptionInInitializerErrorDescriptor();
    }

    const uint8_t *GetClassNotFoundExceptionDescriptor() const
    {
        return base_->GetClassNotFoundExceptionDescriptor();
    }

    const uint8_t *GetInstantiationErrorDescriptor() const
    {
        return base_->GetInstantiationErrorDescriptor();
    }

    const uint8_t *GetUnsupportedOperationExceptionClassDescriptor() const
    {
        return base_->GetUnsupportedOperationExceptionClassDescriptor();
    }

    const uint8_t *GetVerifyErrorClassDescriptor() const
    {
        return base_->GetVerifyErrorClassDescriptor();
    }

    const uint8_t *GetIllegalMonitorStateExceptionDescriptor() const
    {
        return base_->GetIllegalMonitorStateExceptionDescriptor();
    }

    friend std::ostream &operator<<(std::ostream &stream, const LanguageContext &ctx)
    {
        switch (ctx.base_->GetLanguage()) {
            case panda_file::SourceLang::PANDA_ASSEMBLY:
                return stream << "PandaAssembly";
            case panda_file::SourceLang::ECMASCRIPT:
                return stream << "ECMAScript";
            default: {
                break;
            }
        }

        UNREACHABLE();
        return stream;
    }
    uint64_t GetTypeTag(interpreter::TypeTag tag) const
    {
        // return TypeTag default
        return base_->GetTypeTag(tag);
    }

    bool IsCallableObject([[maybe_unused]] ObjectHeader *obj) const
    {
        return base_->IsCallableObject(obj);
    }

    Method *GetCallTarget(ObjectHeader *obj) const
    {
        return base_->GetCallTarget(obj);
    }

    const uint8_t *GetReferenceErrorDescriptor() const
    {
        return base_->GetReferenceErrorDescriptor();
    }

    const uint8_t *GetTypedErrorDescriptor() const
    {
        return base_->GetTypedErrorDescriptor();
    }

    const uint8_t *GetErrorClassDescriptor() const
    {
        return base_->GetErrorClassDescriptor();
    }

    bool IsDynamicLanguage() const
    {
        return base_->IsDynamicLanguage();
    }

    PandaUniquePtr<IMTableBuilder> CreateIMTableBuilder()
    {
        return base_->CreateIMTableBuilder();
    }

    PandaUniquePtr<ITableBuilder> CreateITableBuilder()
    {
        return base_->CreateITableBuilder();
    }

    PandaUniquePtr<VTableBuilder> CreateVTableBuilder()
    {
        return base_->CreateVTableBuilder();
    }

    bool InitializeClass(ClassLinker *class_linker, ManagedThread *thread, Class *klass) const
    {
        return base_->InitializeClass(class_linker, thread, klass);
    }

private:
    const LanguageContextBase *base_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_LANGUAGE_CONTEXT_H_
