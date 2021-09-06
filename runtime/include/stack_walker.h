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

#ifndef PANDA_RUNTIME_INCLUDE_STACK_WALKER_H_
#define PANDA_RUNTIME_INCLUDE_STACK_WALKER_H_

#include <variant>

#include "macros.h"
#include "runtime/include/cframe.h"
#include "runtime/include/cframe_iterators.h"
#include "runtime/interpreter/frame.h"

namespace panda {

enum class FrameKind { NONE, INTERPRETER, COMPILER };

enum class UnwindPolicy {
    ALL,           // unwing all frames including inlined
    SKIP_INLINED,  // unwing all frames excluding inlined
    ONLY_INLINED,  // unwind all inlined frames within single cframe
};

template <FrameKind kind>
struct BoundaryFrame;

template <>
struct BoundaryFrame<FrameKind::INTERPRETER> {
    static constexpr ssize_t METHOD_OFFSET = 1;
    static constexpr ssize_t FP_OFFSET = 0;
    static constexpr ssize_t RETURN_OFFSET = 2;
    static constexpr ssize_t CALLEES_OFFSET = -1;
};

static_assert((BoundaryFrame<FrameKind::INTERPRETER>::METHOD_OFFSET) * sizeof(uintptr_t) == Frame::GetMethodOffset());
static_assert((BoundaryFrame<FrameKind::INTERPRETER>::FP_OFFSET) * sizeof(uintptr_t) == Frame::GetPrevFrameOffset());

template <>
struct BoundaryFrame<FrameKind::COMPILER> {
    static constexpr ssize_t METHOD_OFFSET = -1;
    static constexpr ssize_t FP_OFFSET = 0;
    static constexpr ssize_t RETURN_OFFSET = 1;
    static constexpr ssize_t CALLEES_OFFSET = -2;
};

class FrameAccessor {
public:
    using CFrameType = CFrame;
    using FrameVariant = std::variant<Frame *, CFrame>;

    explicit FrameAccessor(const FrameVariant &frame) : frame_(frame) {}

    bool IsValid() const
    {
        return IsCFrame() || GetIFrame() != nullptr;
    }

    bool IsCFrame() const
    {
        return std::holds_alternative<CFrameType>(frame_);
    }

    CFrameType &GetCFrame()
    {
        ASSERT(IsCFrame());
        return std::get<CFrameType>(frame_);
    }

    const CFrameType &GetCFrame() const
    {
        ASSERT(IsCFrame());
        return std::get<CFrameType>(frame_);
    }

    Frame *GetIFrame()
    {
        return std::get<Frame *>(frame_);
    }

    const Frame *GetIFrame() const
    {
        return std::get<Frame *>(frame_);
    }

private:
    FrameVariant frame_;
};

struct CalleeStorage {
    std::array<uintptr_t *, GetCalleeRegsCount(RUNTIME_ARCH, true) + GetCalleeRegsCount(RUNTIME_ARCH, false)> stack;
    uint32_t callee_regs_mask;
    uint32_t callee_fp_regs_mask;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class StackWalker {
public:
    static constexpr Arch ARCH = RUNTIME_ARCH;
    using SlotType = std::conditional_t<ArchTraits<ARCH>::IS_64_BITS, uint64_t, uint32_t>;
    using FrameVariant = std::variant<Frame *, CFrame>;
    using CFrameType = CFrame;

    StackWalker() = default;
    explicit StackWalker(ManagedThread *thread, UnwindPolicy policy = UnwindPolicy::ALL);
    StackWalker(void *fp, bool is_frame_compiled, uintptr_t npc, UnwindPolicy policy = UnwindPolicy::ALL);

    virtual ~StackWalker() = default;

    NO_COPY_SEMANTIC(StackWalker);
    NO_MOVE_SEMANTIC(StackWalker);

    void Reset(ManagedThread *thread);

    void Verify();

    void NextFrame();

    Method *GetMethod();

    const Method *GetMethod() const
    {
        return IsCFrame() ? GetCFrame().GetMethod() : GetIFrame()->GetMethod();
    }

    size_t GetBytecodePc() const
    {
        return IsCFrame() ? GetCFrameBytecodePc() : GetIFrame()->GetBytecodeOffset();
    }

    size_t GetNativePc() const
    {
        return IsCFrame() ? GetCFrameNativePc() : 0;
    }

    void *GetFp()
    {
        return IsCFrame() ? reinterpret_cast<void *>(GetCFrame().GetFrameOrigin())
                          : reinterpret_cast<void *>(GetIFrame());
    }

    bool HasFrame() const
    {
        return IsCFrame() || GetIFrame() != nullptr;
    }

    template <typename Func>
    bool IterateObjects(Func func)
    {
        return IterateRegs<true, false>(func);
    }

    template <typename Func>
    bool IterateVRegs(Func func)
    {
        return IterateRegs<false, false>(func);
    }

    template <typename Func>
    bool IterateObjectsWithInfo(Func func)
    {
        return IterateRegs<true, true>(func);
    }

    template <typename Func>
    bool IterateVRegsWithInfo(Func func)
    {
        return IterateRegs<false, true>(func);
    }

    bool IsCFrame() const
    {
        return std::holds_alternative<CFrameType>(frame_);
    }

    Frame::VRegister GetVRegValue(size_t vreg_num);

    template <typename T>
    void SetVRegValue(VRegInfo reg_info, T value);

    CFrameType &GetCFrame()
    {
        ASSERT(IsCFrame());
        return std::get<CFrameType>(frame_);
    }

    const CFrameType &GetCFrame() const
    {
        ASSERT(IsCFrame());
        return std::get<CFrameType>(frame_);
    }

    Frame *GetIFrame()
    {
        return std::get<Frame *>(frame_);
    }

    const Frame *GetIFrame() const
    {
        return std::get<Frame *>(frame_);
    }

    Frame *ConvertToIFrame(FrameKind *prev_frame_kind, uint32_t *num_inlined_methods);

    bool IsCompilerBoundFrame(SlotType *prev);

    FrameKind GetPreviousFrameKind() const;

    FrameAccessor GetNextFrame();

    FrameAccessor GetCurrentFrame()
    {
        return FrameAccessor(frame_);
    }

    uint32_t GetCalleeRegsMask(bool is_fp) const
    {
        return is_fp ? callee_stack_.callee_fp_regs_mask : callee_stack_.callee_regs_mask;
    }

    bool IsDynamicMethod() const;

    void DumpFrame(std::ostream &os);

    template <FrameKind kind>
    static SlotType *GetPrevFromBoundary(void *ptr)
    {
        // In current implementation fp must point to previous fp
        static_assert(BoundaryFrame<kind>::FP_OFFSET == 0);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *(reinterpret_cast<SlotType **>(ptr));
    }

    template <FrameKind kind>
    static bool IsBoundaryFrame(const void *ptr)
    {
        if constexpr (kind == FrameKind::INTERPRETER) {  // NOLINT
            return GetBoundaryFrameMethod<kind>(ptr) == COMPILED_CODE_TO_INTERPRETER;
        } else {  // NOLINT
            return GetBoundaryFrameMethod<kind>(ptr) == INTERPRETER_TO_COMPILED_CODE;
        }
    }

    // Dump modify walker state - you must call it only for rvalue object
    void Dump(std::ostream &os, bool print_vregs = false) &&;

private:
    CFrameType CreateCFrame(void *ptr, uintptr_t npc, SlotType *callee_stack, CalleeStorage *prev_callees = nullptr);

    template <bool create>
    CFrameType CreateCFrameForC2IBridge(Frame *frame);
    void InitCalleeBuffer(SlotType *callee_stack, CalleeStorage *prev_callees);

    template <bool objects, bool with_reg_info, typename Func>
    bool IterateRegs(Func func);

    template <bool objects, bool with_reg_info, typename Func>
    bool IterateRegsForCFrame(Func func);

    template <bool objects, bool with_reg_info, typename Func>
    bool IterateRegsForIFrame(Func func);

    FrameVariant GetTopFrameFromFp(void *ptr, bool is_frame_compiled, uintptr_t npc);

    void NextFromCFrame();
    void NextFromIFrame();

    static Method *GetMethodFromCBoundary(void *ptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *reinterpret_cast<Method **>(reinterpret_cast<SlotType *>(ptr) - CFrameLayout::MethodSlot::Start());
    }

    template <FrameKind kind>
    static Method *GetMethodFromBoundary(void *ptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *(reinterpret_cast<Method **>(ptr) + BoundaryFrame<kind>::METHOD_OFFSET);
    }

    template <FrameKind kind>
    static const Method *GetMethodFromBoundary(const void *ptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *(reinterpret_cast<Method *const *>(ptr) + BoundaryFrame<kind>::METHOD_OFFSET);
    }

    template <FrameKind kind>
    static uintptr_t GetReturnAddressFromBoundary(const void *ptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *(reinterpret_cast<const uintptr_t *>(ptr) + BoundaryFrame<kind>::RETURN_OFFSET);
    }

    template <FrameKind kind>
    static SlotType *GetCalleeStackFromBoundary(void *ptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return reinterpret_cast<SlotType *>(ptr) + BoundaryFrame<kind>::CALLEES_OFFSET;
    }

    template <FrameKind kind>
    static uintptr_t GetBoundaryFrameMethod(const void *ptr)
    {
        auto frame_method = reinterpret_cast<uintptr_t>(GetMethodFromBoundary<kind>(ptr));
        return frame_method;
    }

    bool IsInlined() const
    {
        return inline_depth_ != -1;
    }

    uintptr_t GetCFrameBytecodePc() const
    {
        return 0;
    }
    uintptr_t GetCFrameNativePc() const
    {
        return 0;
    }

    bool HandleAddingAsCFrame();

    bool HandleAddingAsIFrame();

    void SetPrevFrame(FrameKind *prev_frame_kind, void **prev_frame, CFrameType *cframe);

private:
    FrameVariant frame_ {nullptr};
    UnwindPolicy policy_ {UnwindPolicy::ALL};
    int inline_depth_ {-1};
    CalleeStorage callee_stack_;
    CalleeStorage prev_callee_stack_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_STACK_WALKER_H_
