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

#ifndef PANDA_RUNTIME_INTERPRETER_FRAME_H_
#define PANDA_RUNTIME_INTERPRETER_FRAME_H_

#include <cstddef>
#include <cstdint>

#include "libpandabase/macros.h"
#include "libpandabase/utils/bit_helpers.h"
#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/logger.h"
#include "runtime/interpreter/vregister-inl.h"
#include "libpandafile/bytecode_instruction-inl.h"

namespace panda {

class Method;
class ObjectHeader;

class Frame {
public:
    class VRegister : public interpreter::VRegisterIface<VRegister> {
    public:
        VRegister() = default;

        ALWAYS_INLINE inline VRegister(int64_t v, uint64_t tag)
        {
            SetValue(v);
            SetTag(tag);
        }

        ALWAYS_INLINE inline void SetValue(int64_t v)
        {
            v_ = v;
        }

        ALWAYS_INLINE inline int64_t GetValue() const
        {
            return v_;
        }

        ALWAYS_INLINE inline void SetTag(uint64_t tag)
        {
            tag_ = tag;
        }

        ALWAYS_INLINE inline uint64_t GetTag() const
        {
            return tag_;
        }

        ALWAYS_INLINE static inline constexpr uint32_t GetTagSize()
        {
            return sizeof(tag_);
        }

        ALWAYS_INLINE static inline constexpr uint32_t GetValueOffset()
        {
            return MEMBER_OFFSET(VRegister, v_);
        }

        ALWAYS_INLINE static inline constexpr uint32_t GetTagOffset()
        {
            return MEMBER_OFFSET(VRegister, tag_);
        }

        ~VRegister() = default;

        DEFAULT_COPY_SEMANTIC(VRegister);
        DEFAULT_MOVE_SEMANTIC(VRegister);

    private:
        // Stores the bit representation of the register value, regardless of the real type.
        // It can contain int/uint 8/16/32/64, float, double and ObjectHeader *.
        int64_t v_ {0};
        uint64_t tag_ {0};
    };

    // Instrumentation: indicate what the frame must be force poped
    static constexpr size_t FORCE_POP = 1U;
    // Instrumentation: indicate what the frame must retry last instruction
    static constexpr size_t RETRY_INSTRUCTION = 2U;
    // Instrumentation: indicate what the frame must notify when poped
    static constexpr size_t NOTIFY_POP = 4U;
    // Indicate that the frame was created after deoptimization.
    // This flag is needed to avoid OSR for deoptimized frames. Because the OSR consumes stack that isn't released after
    // deoptimization, stack overflow may occur. This constrain may be removed once asm interpreter is introduced.
    static constexpr size_t IS_DEOPTIMIZED = 8U;
    // Indicate whether this frame is stackless frame, only take effects under stackless interpreter mode.
    static constexpr size_t IS_STACKLESS = 16U;
    // Indicate whether this frame is initobj frame, only take effects under stackless interpreter mode.
    static constexpr size_t IS_INITOBJ = 32U;

    ALWAYS_INLINE inline Frame(Method *method, Frame *prev, uint32_t nregs)
        : prev_(prev),
          method_(method),
          nregs_(nregs),
          num_actual_args_(0),
          bc_offset_(0),
          flags_(0),
          next_inst_(nullptr),
          inst_(nullptr)
    {
    }
    ALWAYS_INLINE inline Frame(Method *method, Frame *prev, uint32_t nregs, uint32_t num_actual_args)
        : prev_(prev),
          method_(method),
          nregs_(nregs),
          num_actual_args_(num_actual_args),
          bc_offset_(0),
          flags_(0),
          next_inst_(nullptr),
          inst_(nullptr)
    {
    }

    ALWAYS_INLINE inline const VRegister &GetVReg(size_t i) const
    {
        return vregs_[i];
    }

    ALWAYS_INLINE inline VRegister &GetVReg(size_t i)
    {
        return vregs_[i];
    }

    ALWAYS_INLINE inline void SetAcc(const VRegister &acc)
    {
        acc_ = acc;
    }

    ALWAYS_INLINE inline VRegister &GetAcc()
    {
        return acc_;
    }

    ALWAYS_INLINE inline const VRegister &GetAcc() const
    {
        return acc_;
    }

    ALWAYS_INLINE inline void SetMethod(Method *method)
    {
        method_ = method;
    }

    ALWAYS_INLINE inline Method *GetMethod() const
    {
        return method_;
    }

    ALWAYS_INLINE const uint8_t *GetInstrOffset();

    ALWAYS_INLINE inline void SetPrevFrame(Frame *prev)
    {
        prev_ = prev;
    }

    ALWAYS_INLINE inline Frame *GetPrevFrame() const
    {
        return prev_;
    }

    ALWAYS_INLINE inline uint32_t GetSize() const
    {
        return nregs_;
    }

    ALWAYS_INLINE inline uint32_t GetNumActualArgs() const
    {
        return num_actual_args_;
    }

    ALWAYS_INLINE inline void SetBytecodeOffset(uint32_t bc_offset)
    {
        bc_offset_ = bc_offset;
    }

    ALWAYS_INLINE inline uint32_t GetBytecodeOffset() const
    {
        return bc_offset_;
    }

    ALWAYS_INLINE inline void SetNextInstruction(BytecodeInstruction inst)
    {
        next_inst_ = inst;
    }

    ALWAYS_INLINE inline BytecodeInstruction GetNextInstruction() const
    {
        return next_inst_;
    }

    ALWAYS_INLINE inline void SetInstruction(const uint8_t *inst)
    {
        inst_ = inst;
    }

    ALWAYS_INLINE inline const uint8_t *GetInstruction() const
    {
        return inst_;
    }

    ALWAYS_INLINE static inline size_t GetSize(size_t nregs)
    {
        return AlignUp(sizeof(Frame) + sizeof(VRegister) * nregs, GetAlignmentInBytes(DEFAULT_FRAME_ALIGNMENT));
    }

    ALWAYS_INLINE inline bool IsForcePop() const
    {
        return (flags_ & FORCE_POP) != 0;
    }

    ALWAYS_INLINE inline void ClearForcePop()
    {
        flags_ = flags_ & ~FORCE_POP;
    }

    ALWAYS_INLINE inline void SetForcePop()
    {
        flags_ = flags_ | FORCE_POP;
    }

    ALWAYS_INLINE inline bool IsRetryInstruction() const
    {
        return (flags_ & RETRY_INSTRUCTION) != 0;
    }

    ALWAYS_INLINE inline void ClearRetryInstruction()
    {
        flags_ = flags_ & ~RETRY_INSTRUCTION;
    }

    ALWAYS_INLINE inline void SetRetryInstruction()
    {
        flags_ = flags_ | RETRY_INSTRUCTION;
    }

    ALWAYS_INLINE inline bool IsNotifyPop() const
    {
        return (flags_ & NOTIFY_POP) != 0;
    }

    ALWAYS_INLINE inline void ClearNotifyPop()
    {
        flags_ = flags_ & ~NOTIFY_POP;
    }

    ALWAYS_INLINE inline void SetNotifyPop()
    {
        flags_ = flags_ | NOTIFY_POP;
    }

    ALWAYS_INLINE inline bool IsDeoptimized() const
    {
        return (flags_ & IS_DEOPTIMIZED) != 0;
    }

    ALWAYS_INLINE inline void SetDeoptimized()
    {
        flags_ |= IS_DEOPTIMIZED;
    }

    ALWAYS_INLINE inline void DisableOsr()
    {
        SetDeoptimized();
    }

    ALWAYS_INLINE inline bool IsStackless() const
    {
        return (flags_ & IS_STACKLESS) != 0;
    }

    ALWAYS_INLINE inline void SetStackless()
    {
        flags_ = flags_ | IS_STACKLESS;
    }

    ALWAYS_INLINE inline bool IsInitobj() const
    {
        return (flags_ & IS_INITOBJ) != 0;
    }

    ALWAYS_INLINE inline void SetInitobj()
    {
        flags_ = flags_ | IS_INITOBJ;
    }

    ALWAYS_INLINE static inline constexpr uint32_t GetMethodOffset()
    {
        return MEMBER_OFFSET(Frame, method_);
    }

    ALWAYS_INLINE static inline constexpr uint32_t GetPrevFrameOffset()
    {
        return MEMBER_OFFSET(Frame, prev_);
    }

    ALWAYS_INLINE static inline constexpr uint32_t GetNumVregsOffset()
    {
        return MEMBER_OFFSET(Frame, nregs_);
    }

    ALWAYS_INLINE static inline constexpr uint32_t GetVregsOffset()
    {
        return MEMBER_OFFSET(Frame, vregs_);
    }

    ALWAYS_INLINE static inline constexpr uint32_t GetAccOffset()
    {
        return MEMBER_OFFSET(Frame, acc_);
    }

    ALWAYS_INLINE inline void *GetData()
    {
        return data_;
    }

    ALWAYS_INLINE inline void SetData(void *data)
    {
        data_ = data;
    }

    ~Frame() = default;

    DEFAULT_COPY_SEMANTIC(Frame);
    DEFAULT_MOVE_SEMANTIC(Frame);

private:
    Frame *prev_;
    Method *method_;
    uint32_t nregs_;
    uint32_t num_actual_args_;
    uint32_t bc_offset_;
    size_t flags_;

    // It is some language-specific data. Now it is used for JS constant_pool.
    // 'void' because of it can be used for other purposes
    void *data_ {nullptr};
    VRegister acc_;
    BytecodeInstruction next_inst_;
    const uint8_t *inst_;

    __extension__ VRegister vregs_[0];  // NOLINT(modernize-avoid-c-arrays)
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INTERPRETER_FRAME_H_
