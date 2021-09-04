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

#ifndef PANDA_RUNTIME_INCLUDE_METHOD_H_
#define PANDA_RUNTIME_INCLUDE_METHOD_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string_view>

#include "intrinsics.h"
#include "libpandabase/utils/arch.h"
#include "libpandabase/utils/logger.h"
#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "libpandafile/modifiers.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/interpreter/frame.h"
#include "libpandabase/utils/aligned_storage.h"
#include "value.h"

namespace panda {

class Class;
class ManagedThread;
class ProfilingData;

#ifdef PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES
namespace interpreter {
class AccVRegister;
}  // namespace interpreter
using interpreter::AccVRegister;
#else
namespace interpreter {
using AccVRegister = Frame::VRegister;
}  // namespace interpreter
#endif

using FrameDeleter = void (*)(Frame *);

class Method {
public:
    using UniqId = uint64_t;

    enum CompilationStage {
        NOT_COMPILED,
        WAITING,
        COMPILATION,
        COMPILED,
        FAILED,
    };

    enum class VerificationStage {
        // There is a separate bit allocated for each state. Totally 3 bits are used.
        // When the method is not verified all bits are zero.
        // The next state is waiting for verification uses 2nd bit.
        // The final result (ok or fail) is stored in 1st and 0th bits.
        // State is changing as follow:
        //       000      ->    100    --+-->  110
        // (not verified)    (waiting)   |    (ok)
        //                               |
        //                               +-->  101
        //                                    (fail)
        // To read the state __builtin_ffs is used which returns index + 1 of the first set bit
        // or zero for 0 value. See BitsToVerificationStage for details about conversion set bit
        // index to VerificationStage.
        // So the value's order is chosen in a such way in the early stage must have highest value.
        NOT_VERIFIED = 0,
        VERIFIED_FAIL = 1,
        VERIFIED_OK = 2,
        WAITING = 4,
    };

    enum AnnotationField : uint32_t {
        IC_SIZE = 0,
        FUNCTION_LENGTH,
        FUNCTION_NAME,
        STRING_DATA_BEGIN = FUNCTION_NAME,
        STRING_DATA_END = FUNCTION_NAME
    };

    class Proto {
    public:
        Proto(const panda_file::File &pf, panda_file::File::EntityId proto_id);

        Proto(PandaVector<panda_file::Type> shorty, PandaVector<std::string_view> ref_types)
            : shorty_(std::move(shorty)), ref_types_(std::move(ref_types))
        {
        }

        bool operator==(const Proto &other) const
        {
            return shorty_ == other.shorty_ && ref_types_ == other.ref_types_;
        }

        panda_file::Type GetReturnType() const
        {
            return shorty_[0];
        }

        std::string_view GetReturnTypeDescriptor() const;

        const PandaVector<panda_file::Type> &GetShorty() const
        {
            return shorty_;
        }

        const PandaVector<std::string_view> &GetRefTypes() const
        {
            return ref_types_;
        }

        ~Proto() = default;

        DEFAULT_COPY_SEMANTIC(Proto);
        DEFAULT_MOVE_SEMANTIC(Proto);

    private:
        PandaVector<panda_file::Type> shorty_;
        PandaVector<std::string_view> ref_types_;
    };

    Method(Class *klass, const panda_file::File *pf, panda_file::File::EntityId file_id,
           panda_file::File::EntityId code_id, uint32_t access_flags, uint32_t num_args, const uint16_t *shorty);

    explicit Method(const Method *method)
        : stor_32_ {{},
                    method->stor_32_.access_flags_.load(),
                    method->stor_32_.vtable_index_,
                    method->stor_32_.num_args_,
                    0},
          stor_ptr_ {{}, method->stor_ptr_.class_, nullptr, method->stor_ptr_.native_pointer_},
          panda_file_(method->panda_file_),
          file_id_(method->file_id_),
          code_id_(method->code_id_),
          shorty_(method->shorty_)
    {
        stor_ptr_.compiled_entry_point_.store(method->IsNative() ? method->GetCompiledEntryPoint()
                                                                 : GetCompiledCodeToInterpreterBridge(method),
                                              std::memory_order_release);
        SetCompilationStatus(CompilationStage::NOT_COMPILED);
    }

    Method() = delete;
    Method(const Method &) = delete;
    Method(Method &&) = delete;
    Method &operator=(const Method &) = delete;
    Method &operator=(Method &&) = delete;

    uint32_t GetNumArgs() const
    {
        return stor_32_.num_args_;
    }

    uint32_t GetNumVregs() const
    {
        if (!code_id_.IsValid()) {
            return 0;
        }
        panda_file::CodeDataAccessor cda(*panda_file_, code_id_);
        return cda.GetNumVregs();
    }

    uint32_t GetCodeSize() const
    {
        if (!code_id_.IsValid()) {
            return 0;
        }
        panda_file::CodeDataAccessor cda(*panda_file_, code_id_);
        return cda.GetCodeSize();
    }

    const uint8_t *GetInstructions() const
    {
        if (!code_id_.IsValid()) {
            return nullptr;
        }
        panda_file::CodeDataAccessor cda(*panda_file_, code_id_);
        return cda.GetInstructions();
    }

    /*
     * Invoke the method as a static method.
     * Number of arguments and their types must match the method's signature
     */
    Value Invoke(ManagedThread *thread, Value *args, bool proxy_call = false);

    void InvokeVoid(ManagedThread *thread, Value *args)
    {
        Invoke(thread, args);
    }

    /*
     * Invoke the method as a dynamic function.
     * Number of arguments may vary, all arguments must be of type DecodedTaggedValue.
     * args - array of arguments. The first value must be the callee function object
     * num_args - length of args array
     * data - ConstantPool for JS. For other languages is not used at the moment
     */
    Value InvokeDyn(ManagedThread *thread, uint32_t num_args, Value *args, bool proxy_call = false,
                    void *data = nullptr);

    /*
     * Using in JS for generators
     * num_actual_args - length of args array
     * args - array of arguments.
     * data - ConstantPool for JS. For other languages is not used at the moment
     */
    Value InvokeGen(ManagedThread *thread, const uint8_t *pc, Value acc, uint32_t num_actual_args, Value *args,
                    void *data);

    Class *GetClass() const
    {
        return stor_ptr_.class_;
    }

    void SetClass(Class *cls)
    {
        stor_ptr_.class_ = cls;
    }

    void SetPandaFile(const panda_file::File *file)
    {
        panda_file_ = file;
    }

    const panda_file::File *GetPandaFile() const
    {
        return panda_file_;
    }

    panda_file::File::EntityId GetFileId() const
    {
        return file_id_;
    }

    panda_file::File::EntityId GetCodeId() const
    {
        return code_id_;
    }

    inline uint32_t GetHotnessCounter() const
    {
        return stor_32_.hotness_counter_;
    }

    inline NO_THREAD_SANITIZE void IncrementHotnessCounter()
    {
        ++stor_32_.hotness_counter_;
    }

    NO_THREAD_SANITIZE void ResetHotnessCounter()
    {
        stor_32_.hotness_counter_ = 0;
    }

    template <class AccVRegisterPtrT>
    NO_THREAD_SANITIZE void SetAcc([[maybe_unused]] AccVRegisterPtrT acc);

    // NO_THREAD_SANITIZE because of performance degradation (see commit 7c913cb1 and MR 997#note_113500)
    template <class AccVRegisterPtrT>
    NO_THREAD_SANITIZE bool IncrementHotnessCounter([[maybe_unused]] uintptr_t bytecode_offset,
                                                    [[maybe_unused]] AccVRegisterPtrT cc,
                                                    [[maybe_unused]] bool osr = false);

    inline NO_THREAD_SANITIZE void SetHotnessCounter(size_t counter)
    {
        stor_32_.hotness_counter_ = counter;
    }

    const void *GetCompiledEntryPoint()
    {
        return stor_ptr_.compiled_entry_point_.load(std::memory_order_acquire);
    }

    const void *GetCompiledEntryPoint() const
    {
        return stor_ptr_.compiled_entry_point_.load(std::memory_order_acquire);
    }

    void SetCompiledEntryPoint(const void *entry_point)
    {
        stor_ptr_.compiled_entry_point_.store(entry_point, std::memory_order_release);
    }

    void SetInterpreterEntryPoint()
    {
        if (!IsNative()) {
            SetCompiledEntryPoint(GetCompiledCodeToInterpreterBridge(this));
        }
    }

    bool HasCompiledCode() const
    {
        return GetCompiledEntryPoint() != GetCompiledCodeToInterpreterBridge(this);
    }

    inline CompilationStage GetCompilationStatus() const
    {
        return static_cast<CompilationStage>((stor_32_.access_flags_.load() & COMPILATION_STATUS_MASK) >>
                                             COMPILATION_STATUS_SHIFT);
    }

    inline CompilationStage GetCompilationStatus(uint32_t value)
    {
        return static_cast<CompilationStage>((value & COMPILATION_STATUS_MASK) >> COMPILATION_STATUS_SHIFT);
    }

    inline void SetCompilationStatus(enum CompilationStage new_status)
    {
        stor_32_.access_flags_ &= ~COMPILATION_STATUS_MASK;
        stor_32_.access_flags_ |= static_cast<uint32_t>(new_status) << COMPILATION_STATUS_SHIFT;
    }

    inline bool AtomicSetCompilationStatus(enum CompilationStage old_status, enum CompilationStage new_status)
    {
        uint32_t old_value = stor_32_.access_flags_.load();
        while (GetCompilationStatus(old_value) == old_status) {
            uint32_t new_value = MakeCompilationStatusValue(old_value, new_status);
            if (stor_32_.access_flags_.compare_exchange_strong(old_value, new_value)) {
                return true;
            }
        }
        return false;
    }

    panda_file::Type GetReturnType() const;

    // idx - index number of the argument in the signature
    panda_file::Type GetArgType(size_t idx) const;

    panda_file::File::StringData GetRefArgType(size_t idx) const;

    template <typename Callback>
    void EnumerateTypes(Callback handler) const;

    panda_file::File::StringData GetName() const;

    panda_file::File::StringData GetClassName() const;

    PandaString GetFullName(bool with_signature = false) const;

    uint32_t GetFullNameHash() const;
    static uint32_t GetFullNameHashFromString(const uint8_t *str);
    static uint32_t GetClassNameHashFromString(const uint8_t *str);

    Proto GetProto() const;

    size_t GetFrameSize() const
    {
        return Frame::GetSize(GetNumArgs() + GetNumVregs());
    }

    uint32_t GetNumericalAnnotation(AnnotationField field_id) const;
    panda_file::File::StringData GetStringDataAnnotation(AnnotationField field_id) const;

    uint32_t GetAccessFlags() const
    {
        return stor_32_.access_flags_.load();
    }

    void SetAccessFlags(uint32_t access_flags)
    {
        stor_32_.access_flags_ = access_flags;
    }

    bool IsStatic() const
    {
        return (stor_32_.access_flags_.load() & ACC_STATIC) != 0;
    }

    bool IsNative() const
    {
        return (stor_32_.access_flags_.load() & ACC_NATIVE) != 0;
    }

    bool IsPublic() const
    {
        return (stor_32_.access_flags_.load() & ACC_PUBLIC) != 0;
    }

    bool IsPrivate() const
    {
        return (stor_32_.access_flags_.load() & ACC_PRIVATE) != 0;
    }

    bool IsProtected() const
    {
        return (stor_32_.access_flags_.load() & ACC_PROTECTED) != 0;
    }

    bool IsIntrinsic() const
    {
        return (stor_32_.access_flags_.load() & ACC_INTRINSIC) != 0;
    }

    bool IsSynthetic() const
    {
        return (stor_32_.access_flags_.load() & ACC_SYNTHETIC) != 0;
    }

    bool IsAbstract() const
    {
        return (stor_32_.access_flags_.load() & ACC_ABSTRACT) != 0;
    }

    bool IsFinal() const
    {
        return (stor_32_.access_flags_.load() & ACC_FINAL) != 0;
    }

    bool IsSynchronized() const
    {
        return (stor_32_.access_flags_.load() & ACC_SYNCHRONIZED) != 0;
    }

    bool HasSingleImplementation() const
    {
        return (stor_32_.access_flags_.load() & ACC_SINGLE_IMPL) != 0;
    }

    void SetHasSingleImplementation(bool v)
    {
        if (v) {
            stor_32_.access_flags_ |= ACC_SINGLE_IMPL;
        } else {
            stor_32_.access_flags_ &= ~ACC_SINGLE_IMPL;
        }
    }

    Method *GetSingleImplementation()
    {
        return HasSingleImplementation() ? this : nullptr;
    }

    void SetIntrinsic(intrinsics::Intrinsic intrinsic)
    {
        ASSERT(!IsIntrinsic());
        ASSERT((stor_32_.access_flags_.load() & INTRINSIC_MASK) == 0);
        stor_32_.access_flags_ |= ACC_INTRINSIC;
        stor_32_.access_flags_ |= static_cast<uint32_t>(intrinsic) << INTRINSIC_SHIFT;
    }

    intrinsics::Intrinsic GetIntrinsic() const
    {
        ASSERT(IsIntrinsic());
        return static_cast<intrinsics::Intrinsic>((stor_32_.access_flags_.load() & INTRINSIC_MASK) >> INTRINSIC_SHIFT);
    }

    void SetVTableIndex(uint32_t vtable_index)
    {
        stor_32_.vtable_index_ = vtable_index;
    }

    uint32_t GetVTableIndex() const
    {
        return stor_32_.vtable_index_;
    }

    void SetNativePointer(void *native_pointer)
    {
        using AtomicType = std::atomic<decltype(GetNativePointer())>;
        auto *atomic_native_pointer = reinterpret_cast<AtomicType *>(&stor_ptr_.native_pointer_);
        atomic_native_pointer->store(native_pointer, std::memory_order_relaxed);
    }

    void *GetNativePointer() const
    {
        using AtomicType = std::atomic<decltype(GetNativePointer())>;
        auto *atomic_native_pointer = reinterpret_cast<const AtomicType *>(&stor_ptr_.native_pointer_);
        return atomic_native_pointer->load();
    }

    const uint16_t *GetShorty() const
    {
        return shorty_;
    }

    uint32_t FindCatchBlock(Class *cls, uint32_t pc) const;

    panda_file::Type GetEffectiveArgType(size_t idx) const;

    panda_file::Type GetEffectiveReturnType() const;

    void SetIsDefaultInterfaceMethod()
    {
        stor_32_.access_flags_ |= ACC_DEFAULT_INTERFACE_METHOD;
    }

    bool IsDefaultInterfaceMethod() const
    {
        return (stor_32_.access_flags_.load() & ACC_DEFAULT_INTERFACE_METHOD) != 0;
    }

    bool IsConstructor() const
    {
        return (stor_32_.access_flags_.load() & ACC_CONSTRUCTOR) != 0;
    }

    bool IsInstanceConstructor() const
    {
        return IsConstructor() && !IsStatic();
    }

    bool IsStaticConstructor() const
    {
        return IsConstructor() && IsStatic();
    }

    static constexpr uint32_t GetCompilerEntryPointOffset(Arch arch)
    {
        return MEMBER_OFFSET(Method, stor_ptr_) +
               StoragePackedPtr::ConvertOffset(PointerSize(arch),
                                               MEMBER_OFFSET(StoragePackedPtr, compiled_entry_point_));
    }
    static constexpr uint32_t GetNativePointerOffset(Arch arch)
    {
        return MEMBER_OFFSET(Method, stor_ptr_) +
               StoragePackedPtr::ConvertOffset(PointerSize(arch), MEMBER_OFFSET(StoragePackedPtr, native_pointer_));
    }
    static constexpr uint32_t GetClassOffset(Arch arch)
    {
        return MEMBER_OFFSET(Method, stor_ptr_) +
               StoragePackedPtr::ConvertOffset(PointerSize(arch), MEMBER_OFFSET(StoragePackedPtr, class_));
    }
    static constexpr uint32_t GetAccessFlagsOffset()
    {
        return MEMBER_OFFSET(Method, stor_32_) + MEMBER_OFFSET(StoragePacked32, access_flags_);
    }
    static constexpr uint32_t GetNumArgsOffset()
    {
        return MEMBER_OFFSET(Method, stor_32_) + MEMBER_OFFSET(StoragePacked32, num_args_);
    }
    static constexpr uint32_t GetShortyOffset()
    {
        return MEMBER_OFFSET(Method, shorty_);
    }
    static constexpr uint32_t GetVTableIndexOffset()
    {
        return MEMBER_OFFSET(Method, stor_32_) + MEMBER_OFFSET(StoragePacked32, vtable_index_);
    }

    template <typename Callback>
    void EnumerateTryBlocks(Callback callback) const;

    template <typename Callback>
    void EnumerateCatchBlocks(Callback callback) const;

    template <typename Callback>
    void EnumerateExceptionHandlers(Callback callback) const;

    static inline UniqId CalcUniqId(const panda_file::File *file, panda_file::File::EntityId file_id)
    {
        constexpr uint64_t HALF = 32ULL;
        uint64_t uid = file->GetUniqId();
        uid <<= HALF;
        uid |= file_id.GetOffset();
        return uid;
    }

    // for synthetic methods, like arrays .ctor
    static UniqId CalcUniqId(const uint8_t *class_descr, const uint8_t *name);

    UniqId GetUniqId() const
    {
        return CalcUniqId(panda_file_, file_id_);
    }

    int32_t GetLineNumFromBytecodeOffset(uint32_t bc_offset) const;

    panda_file::File::StringData GetClassSourceFile() const;

    void StartProfiling();
    void StopProfiling();

    ProfilingData *GetProfilingData()
    {
        return profiling_data_.load(std::memory_order_acquire);
    }

    const ProfilingData *GetProfilingData() const
    {
        return profiling_data_.load(std::memory_order_acquire);
    }

    bool IsProfiling() const
    {
        return GetProfilingData() != nullptr;
    }

    bool IsProfilingWithoutLock() const
    {
        return profiling_data_.load() != nullptr;
    }

    bool AddJobInQueue();
    void WaitForVerification();
    void SetVerified(bool result);
    bool IsVerified() const;
    bool Verify();
    void EnqueueForVerification();

    ~Method();

private:
    VerificationStage GetVerificationStage() const;
    void SetVerificationStage(VerificationStage stage);
    VerificationStage ExchangeVerificationStage(VerificationStage stage);
    static VerificationStage BitsToVerificationStage(uint32_t bits);

    template <bool is_dynamic>
    Value InvokeCompiledCode(ManagedThread *thread, uint32_t num_actual_args, Value *args);

    Value GetReturnValueFromTaggedValue(DecodedTaggedValue ret_value)
    {
        Value res(static_cast<int64_t>(0));

        panda_file::Type ret_type = GetReturnType();

        if (ret_type.GetId() != panda_file::Type::TypeId::VOID) {
            if (ret_type.GetId() == panda_file::Type::TypeId::REFERENCE) {
                res = Value(reinterpret_cast<ObjectHeader *>(ret_value.value));
            } else if (ret_type.GetId() == panda_file::Type::TypeId::TAGGED) {
                res = Value(ret_value.value, ret_value.tag);
            } else {
                res = Value(ret_value.value);
            }
        }

        return res;
    }

    inline static uint32_t MakeCompilationStatusValue(uint32_t value, CompilationStage new_status)
    {
        value &= ~COMPILATION_STATUS_MASK;
        value |= static_cast<uint32_t>(new_status) << COMPILATION_STATUS_SHIFT;
        return value;
    }

    template <bool is_dynamic>
    Value InvokeInterpretedCode(ManagedThread *thread, uint32_t num_actual_args, Value *args, void *data = nullptr);

    template <bool is_dynamic>
    PandaUniquePtr<Frame, FrameDeleter> InitFrame(ManagedThread *thread, uint32_t num_actual_args, Value *args,
                                                  Frame *current_frame, void *data = nullptr);
    Value GetReturnValueFromAcc(const panda_file::Type &ret_type, bool has_pending_exception,
                                const Frame::VRegister &ret_value)
    {
        if (UNLIKELY(has_pending_exception)) {
            if (ret_type.IsReference()) {
                return Value(nullptr);
            }

            return Value(static_cast<int64_t>(0));
        }

        if (ret_type.GetId() != panda_file::Type::TypeId::VOID) {
            if (ret_type.GetId() == panda_file::Type::TypeId::TAGGED) {
                return Value(ret_value.GetValue(), ret_value.GetTag());
            }
            if (ret_value.HasObject()) {
                return Value(ret_value.GetReference());
            }

            return Value(ret_value.GetLong());
        }

        return Value(static_cast<int64_t>(0));
    }

    template <bool is_dynamic>
    Value InvokeImpl(ManagedThread *thread, uint32_t num_actual_args, Value *args, bool proxy_call,
                     void *data = nullptr);

private:
    static constexpr size_t STORAGE_32_NUM = 4;
    static constexpr size_t STORAGE_PTR_NUM = 3;

    struct StoragePacked32 : public AlignedStorage<sizeof(uint64_t), sizeof(uint32_t), STORAGE_32_NUM> {
        Aligned<std::atomic_uint32_t> access_flags_;
        Aligned<uint32_t> vtable_index_;
        Aligned<uint32_t> num_args_;
        Aligned<uint32_t> hotness_counter_;
    } stor_32_;
    static_assert(sizeof(stor_32_) == StoragePacked32::GetSize());

    struct StoragePackedPtr : public AlignedStorage<sizeof(uintptr_t), sizeof(uintptr_t), STORAGE_PTR_NUM> {
        Aligned<Class> *class_;
        Aligned<std::atomic<const void *>> compiled_entry_point_ {nullptr};
        Aligned<void *> native_pointer_ {nullptr};
    } stor_ptr_;
    static_assert(sizeof(stor_ptr_) == StoragePackedPtr::GetSize());

    const panda_file::File *panda_file_;
    panda_file::File::EntityId file_id_;
    panda_file::File::EntityId code_id_;
    const uint16_t *shorty_;
    std::atomic<ProfilingData *> profiling_data_ {nullptr};

    friend class Offsets_Method_Test;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_METHOD_H_
