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

#ifndef PANDA_RUNTIME_INCLUDE_METHOD_INL_H_
#define PANDA_RUNTIME_INCLUDE_METHOD_INL_H_

#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/code_data_accessor.h"
#include "libpandafile/file.h"
#include "libpandafile/method_data_accessor-inl.h"
#include "libpandafile/method_data_accessor.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "libpandafile/proto_data_accessor.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/interpreter/interpreter.h"
#include "runtime/interpreter/runtime_interface.h"

namespace panda {

template <bool is_dynamic>
Value Method::InvokeCompiledCode(ManagedThread *thread, uint32_t num_actual_args, Value *args)
{
    Frame *current_frame = thread->GetCurrentFrame();
    Span<Value> args_span(args, num_actual_args);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
    DecodedTaggedValue ret_value {};
    bool is_compiled = thread->IsCurrentFrameCompiled();
    // Use frame allocator to alloc memory for parameters as thread can be terminated and
    // InvokeCompiledCodeWithArgArray will not return in this case we will get memory leak with internal
    // allocator
    mem::StackFrameAllocator *allocator = thread->GetStackFrameAllocator();
    auto values_deleter = [allocator](int64_t *values) {
        if (values != nullptr) {
            allocator->Free(values);
        }
    };
    auto values = PandaUniquePtr<int64_t, decltype(values_deleter)>(nullptr, values_deleter);
    size_t values_count = 0;
    if (num_actual_args > 0) {
        // In the worse case we are calling a dynamic method in which all arguments are pairs ot int64_t
        // That is why we allocate 2 x num_actual_args
        size_t capacity = 2 * num_actual_args * sizeof(int64_t);
        // All allocations through FrameAllocator must be aligned
        capacity = AlignUp(capacity, GetAlignmentInBytes(DEFAULT_FRAME_ALIGNMENT));
        values.reset(reinterpret_cast<int64_t *>(allocator->Alloc(capacity)));
        Span<int64_t> values_span(values.get(), capacity);
        for (uint32_t i = 0; i < num_actual_args; ++i, ++values_count) {
            if (args_span[i].IsReference()) {
                values_span[values_count] = reinterpret_cast<int64_t>(args_span[i].GetAs<ObjectHeader *>());
            } else if (args_span[i].IsDecodedTaggedValue()) {
                DecodedTaggedValue v = args_span[i].GetDecodedTaggedValue();
                values_span[values_count++] = v.value;
                values_span[values_count] = v.tag;
            } else {
                values_span[values_count] = args_span[i].GetAs<int64_t>();
            }
        }
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (is_dynamic) {
        ASSERT(values_count >= 2U);
        ASSERT(values_count % 2U == 0);
        // All arguments must be pair of int64_t. That is why we divide by 2
        // -1 means we don't count function object;
        uint32_t num_args = static_cast<uint32_t>(values_count) / 2 - 1;  // 2 - look at the comment above
        ret_value = InvokeCompiledCodeWithArgArrayDyn(values.get(), num_args, current_frame, this, thread);
    } else {  // NOLINTE(readability-braces-around-statements)
        ret_value = InvokeCompiledCodeWithArgArray(values.get(), current_frame, this, thread);
    }
    thread->SetCurrentFrameIsCompiled(is_compiled);
    thread->SetCurrentFrame(current_frame);
    if (UNLIKELY(thread->HasPendingException())) {
        ret_value = DecodedTaggedValue(0, 0);
    }
    return GetReturnValueFromTaggedValue(ret_value);
}

template <bool is_dynamic>
Value Method::InvokeInterpretedCode(ManagedThread *thread, uint32_t num_actual_args, Value *args, void *data)
{
    Frame *current_frame = thread->GetCurrentFrame();
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
    Value res(static_cast<int64_t>(0));
    panda_file::Type ret_type = GetReturnType();
    if (!Verify()) {
        auto ctx = Runtime::GetCurrent()->GetLanguageContext(*this);
        panda::ThrowVerificationException(ctx, GetFullName());
        if (ret_type.IsReference()) {
            res = Value(nullptr);
        } else {
            res = Value(static_cast<int64_t>(0));
        }
    } else {
        PandaUniquePtr<Frame, FrameDeleter> frame =
            InitFrame<is_dynamic>(thread, num_actual_args, args, current_frame, data);
        if (UNLIKELY(frame.get() == nullptr)) {
            panda::ThrowOutOfMemoryError("CreateFrame failed: " + GetFullName());
            if (ret_type.IsReference()) {
                res = Value(nullptr);
            } else {
                res = Value(static_cast<int64_t>(0));
            }
            return res;
        }
        auto is_compiled = thread->IsCurrentFrameCompiled();
        thread->SetCurrentFrameIsCompiled(false);
        thread->SetCurrentFrame(frame.get());
        if (is_compiled && current_frame != nullptr) {
            // Create C2I bridge frame in case of previous frame is a JNI frame or other compiler frame.
            // But create only if the previous frame is not a C2I bridge already.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
            C2IBridge bridge;
            if (!StackWalker::IsBoundaryFrame<FrameKind::INTERPRETER>(current_frame)) {
                bridge = {0, reinterpret_cast<uintptr_t>(current_frame), COMPILED_CODE_TO_INTERPRETER,
                          thread->GetNativePc()};
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                frame->SetPrevFrame(reinterpret_cast<Frame *>(&bridge.v_[1]));
            }
            // Workaround for #2888 #2925
            // We cannot make OSR on the methods called from here, because:
            // 1. If caller is JNI method, then C2I bridge, created above, is not complete. It can be fixed by
            //    allocating full size boundary frame.
            // 2. If caller is compiled method, then we got here from entrypoint. But currently compiler creates
            //    boundary frame with pseudo LR value, that doesn't point to the instruction after call, thereby
            //    OSR will fail. It can be fixed by addresses patching, currently codegen hasn't no such mechanism.
            frame->DisableOsr();
            Runtime::GetCurrent()->GetNotificationManager()->MethodEntryEvent(thread, this);
            interpreter::Execute(thread, GetInstructions(), frame.get());
            Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, this);
            thread->SetCurrentFrameIsCompiled(true);
        } else {
            Runtime::GetCurrent()->GetNotificationManager()->MethodEntryEvent(thread, this);
            interpreter::Execute(thread, GetInstructions(), frame.get());
            Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, this);
        }
        thread->SetCurrentFrame(current_frame);
        res = GetReturnValueFromAcc(ret_type, thread->HasPendingException(), frame->GetAcc());
    }
    return res;
}

template <bool is_dynamic>
PandaUniquePtr<Frame, FrameDeleter> Method::InitFrame(ManagedThread *thread, uint32_t num_actual_args, Value *args,
                                                      Frame *current_frame, void *data)
{
    ASSERT(code_id_.IsValid());
    panda_file::CodeDataAccessor cda(*panda_file_, code_id_);
    auto num_vregs = cda.GetNumVregs();

    Span<Value> args_span(args, num_actual_args);

    uint32_t num_declared_args = GetNumArgs();
    size_t frame_size;
    // NOLINTNEXTLINE(readability-braces-around-statements)
    if constexpr (is_dynamic) {
        frame_size = num_vregs + std::max(num_declared_args, num_actual_args);
    } else {  // NOLINTE(readability-braces-around-statements)
        frame_size = num_vregs + num_declared_args;
    }
    auto frame_deleter = [](Frame *frame) { FreeFrame(frame); };
    PandaUniquePtr<Frame, FrameDeleter> frame(
        CreateFrameWithActualArgs(frame_size, num_actual_args, this, current_frame), frame_deleter);
    if (UNLIKELY(frame.get() == nullptr)) {
        return frame;
    }

    for (size_t i = 0; i < num_actual_args; i++) {
        if (args_span[i].IsDecodedTaggedValue()) {
            DecodedTaggedValue decoded = args_span[i].GetDecodedTaggedValue();
            frame->GetVReg(num_vregs + i).SetValue(decoded.value);
            frame->GetVReg(num_vregs + i).SetTag(decoded.tag);
        } else if (args_span[i].IsReference()) {
            frame->GetVReg(num_vregs + i).SetReference(args_span[i].GetAs<ObjectHeader *>());
        } else {
            frame->GetVReg(num_vregs + i).SetPrimitive(args_span[i].GetAs<int64_t>());
        }
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (is_dynamic) {
        LanguageContext ctx = thread->GetLanguageContext();
        DecodedTaggedValue initial_value = ctx.GetInitialDecodedValue();
        for (size_t i = num_actual_args; i < num_declared_args; i++) {
            frame->GetVReg(num_vregs + i).SetValue(initial_value.value);
            frame->GetVReg(num_vregs + i).SetTag(initial_value.tag);
        }
    }

    frame->SetData(data);
    return frame;
}

template <bool is_dynamic>
Value Method::InvokeImpl(ManagedThread *thread, uint32_t num_actual_args, Value *args, bool proxy_call, void *data)
{
    IncrementHotnessCounter(0, nullptr);

    // Currently, proxy methods should always be invoked in the interpreter. This constraint should be relaxed once
    // we support same frame layout for interpreter and compiled methods.
    bool run_interpreter = !HasCompiledCode() || proxy_call;
    ASSERT(!(proxy_call && IsNative()));
    if (!run_interpreter) {
        return InvokeCompiledCode<is_dynamic>(thread, num_actual_args, args);
    }

    return InvokeInterpretedCode<is_dynamic>(thread, num_actual_args, args, data);
}

template <class AccVRegisterPtrT>
inline void Method::SetAcc([[maybe_unused]] AccVRegisterPtrT acc)
{
    if constexpr (!std::is_same_v<AccVRegisterPtrT, std::nullptr_t>) {  // NOLINT
        if (acc != nullptr) {
            ManagedThread::GetCurrent()->GetCurrentFrame()->SetAcc(*acc);
        }
    }
}

/**
 * Increment method's hotness counter.
 * @param bytecode_offset Offset of the target bytecode instruction. Used only for OSR.
 * @param acc Pointer to the accumulator, it is needed because interpreter uses own Frame, not the one in the method.
 *            Used only for OSR.
 * @return true if OSR has been occurred
 */
template <class AccVRegisterPtrT>
inline bool Method::IncrementHotnessCounter([[maybe_unused]] uintptr_t bytecode_offset,
                                            [[maybe_unused]] AccVRegisterPtrT acc, [[maybe_unused]] bool osr)
{
    ++stor_32_.hotness_counter_;
    return false;
}

template <typename Callback>
void Method::EnumerateTypes(Callback handler) const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    mda.EnumerateTypesInProto(handler);
}

template <typename Callback>
void Method::EnumerateTryBlocks(Callback callback) const
{
    ASSERT(!IsAbstract());

    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    panda_file::CodeDataAccessor cda(*panda_file_, mda.GetCodeId().value());

    cda.EnumerateTryBlocks(callback);
}

template <typename Callback>
void Method::EnumerateCatchBlocks(Callback callback) const
{
    ASSERT(!IsAbstract());

    using TryBlock = panda_file::CodeDataAccessor::TryBlock;
    using CatchBlock = panda_file::CodeDataAccessor::CatchBlock;

    EnumerateTryBlocks([&callback, code = GetInstructions()](const TryBlock &try_block) {
        bool next = true;
        const uint8_t *try_start_pc = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(code) +
                                                                  static_cast<uintptr_t>(try_block.GetStartPc()));
        const uint8_t *try_end_pc = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(try_start_pc) +
                                                                static_cast<uintptr_t>(try_block.GetLength()));
        // ugly, but API of TryBlock is badly designed: enumaration is paired with mutation & updating
        const_cast<TryBlock &>(try_block).EnumerateCatchBlocks(
            [&callback, &next, try_start_pc, try_end_pc](const CatchBlock &catch_block) {
                return next = callback(try_start_pc, try_end_pc, catch_block);
            });
        return next;
    });
}

template <typename Callback>
void Method::EnumerateExceptionHandlers(Callback callback) const
{
    ASSERT(!IsAbstract());

    using CatchBlock = panda_file::CodeDataAccessor::CatchBlock;

    EnumerateCatchBlocks([this, callback = std::move(callback)](const uint8_t *try_start_pc, const uint8_t *try_end_pc,
                                                                const CatchBlock &catch_block) {
        auto type_idx = catch_block.GetTypeIdx();
        const uint8_t *pc =
            &GetInstructions()[catch_block.GetHandlerPc()];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        size_t size = catch_block.GetCodeSize();
        const Class *cls = nullptr;
        if (type_idx != panda_file::INVALID_INDEX) {
            Runtime *runtime = Runtime::GetCurrent();
            auto type_id = GetClass()->ResolveClassIndex(type_idx);
            LanguageContext ctx = runtime->GetLanguageContext(*this);
            cls = runtime->GetClassLinker()->GetExtension(ctx)->GetClass(*panda_file_, type_id);
        }
        return callback(try_start_pc, try_end_pc, cls, pc, size);
    });
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_METHOD_INL_H_
