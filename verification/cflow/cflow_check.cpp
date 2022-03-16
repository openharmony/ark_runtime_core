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

#include "cflow_check.h"
#include "cflow_common.h"

#include "runtime/include/method-inl.h"

#include "utils/logger.h"

#include "verification/util/str.h"

#include "verifier_messages.h"

namespace panda::verifier {

template <class F>
static bool CheckCode(const CflowMethodInfo &cflow_info, const uint8_t *method_pc_end_ptr,
                      const F &report_incorrect_jump)
{
    // check method code jumps (body + exc handlers, i.e all code)
    {
        const uint8_t *pc_jump_ptr = nullptr;
        const uint8_t *pc_target_ptr = nullptr;
        if (cflow_info.JmpsMap().GetFirstConflictingJump<const uint8_t *>(cflow_info.InstMap(), &pc_jump_ptr,
                                                                          &pc_target_ptr)) {
            report_incorrect_jump(pc_jump_ptr, pc_target_ptr,
                                  "Invalid jump in the method body into middle of instruction.");
            return false;
        }
    }

    // check method body last instruction, if body spans till the method end
    {
        const auto &last_body_block = cflow_info.BodyInfo().back();
        if (last_body_block.End == method_pc_end_ptr && last_body_block.LastInstType != InstructionType::RETURN &&
            last_body_block.LastInstType != InstructionType::THROW &&
            last_body_block.LastInstType != InstructionType::JUMP) {
            LOG(DEBUG, VERIFIER) << "Invalid last instruction in method, execution beyond the method code boundary.";
            return false;
        }
    }

    return true;
}

static bool CheckFallthroughFromBlock(CflowCheckFlags options, const CflowMethodInfo &cflow_info,
                                      const CacheOfRuntimeThings::CachedMethod &method)
{
    if (!options[CflowCheckOptions::ALLOW_JMP_BODY_INTO_HANDLER] &&
        !options[CflowCheckOptions::ALLOW_JMP_BODY_TO_HANDLER]) {
        // fall through into exception handlers from body is disallowed
        // iterate all blocks and check last instruction
        for (const auto &block_info : cflow_info.BodyInfo()) {
            if (block_info.LastInstType != InstructionType::RETURN &&
                block_info.LastInstType != InstructionType::THROW && block_info.LastInstType != InstructionType::JUMP) {
                LOG_VERIFIER_CFLOW_BODY_FALL_INTO_EXC_HANDLER(CacheOfRuntimeThings::GetName(method),
                                                              (OffsetAsHexStr(method.bytecode, block_info.End)));
                return false;
            }
        }
    }
    return true;
}

template <class F>
static bool CheckJmpIntoExcHandler(CflowCheckFlags options, const CflowMethodInfo &cflow_info,
                                   const F &report_incorrect_jump)
{
    // check body jumps in/into exception handlers
    if (!options[CflowCheckOptions::ALLOW_JMP_BODY_INTO_HANDLER]) {
        InstructionsMap inst_map {cflow_info.InstMap()};
        if (!options[CflowCheckOptions::ALLOW_JMP_BODY_TO_HANDLER]) {
            // jumps from code to any place in handlers are prohibited
            for (const auto &handler : cflow_info.ExcHandlers()) {
                inst_map.MarkCodeBlock(handler.Info.Start, handler.Info.End);
            }
        } else {
            // allow jumps to start of any handler
            for (const auto &handler : cflow_info.ExcHandlers()) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                inst_map.MarkCodeBlock(handler.Info.Start + 1, handler.Info.End);
            }
        }
        for (const auto &block_info : cflow_info.BodyInfo()) {
            if (block_info.JmpsMap.IsConflictingWith(inst_map)) {
                const uint8_t *jmp_pc = nullptr;
                const uint8_t *tgt_pc = nullptr;
                if (!block_info.JmpsMap.GetFirstConflictingJump(inst_map, &jmp_pc, &tgt_pc)) {
                    LOG_VERIFIER_CFLOW_INTERNAL_ERROR();
                    return false;
                }
                report_incorrect_jump(jmp_pc, tgt_pc, "Prohibited jump from method body to/into exception handler.");
                return false;
            }
        }
    }
    return true;
}

static bool CheckFallthroughFromExcHandler(CflowCheckFlags options, const CflowMethodInfo &cflow_info,
                                           const CacheOfRuntimeThings::CachedMethod &method)
{
    // Temporarily mark all method mody, and check first mark after handler end
    // check exception handlers fallthrough to exception handlers
    if (!options[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_HANDLER] &&
        !options[CflowCheckOptions::ALLOW_JMP_HANDLER_TO_HANDLER]) {
        // falling through is prohibited, so last instruction in each handler should be
        // linear cflow terminating
        for (const auto &handler_info : cflow_info.ExcHandlers()) {
            if (handler_info.Info.LastInstType != InstructionType::RETURN &&
                handler_info.Info.LastInstType != InstructionType::THROW &&
                handler_info.Info.LastInstType != InstructionType::JUMP) {
                LOG_VERIFIER_CFLOW_INVALID_LAST_INST_OF_EXC_HANDLER_FALL_INTO_OTHER_EXC_HANDLER(
                    CacheOfRuntimeThings::GetName(method), (OffsetAsHexStr(method.bytecode, handler_info.Info.Start)),
                    (OffsetAsHexStr(method.bytecode, handler_info.Info.End)));
                return false;
            }
        }
    }
    return true;
}

template <class F>
static bool CheckJmpOutExcHandler(CflowCheckFlags options, const CflowMethodInfo &cflow_info,
                                  const F &report_incorrect_jump)
{
    // check jumps out from handlers
    InstructionsMap inst_map {cflow_info.InstMap()};
    if (!options[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_BODY]) {
        // prohibit jumps from handler to body
        for (const auto &block_info : cflow_info.BodyInfo()) {
            inst_map.MarkCodeBlock(block_info.Start, block_info.End);
        }
    }

    if (!options[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_HANDLER]) {
        // mark all handlers
        // since we already checked jumps for correctness wrt instructions,
        // we may just clear block, w/o bothering with exact instructions map
        for (const auto &each_handler_info : cflow_info.ExcHandlers()) {
            inst_map.MarkCodeBlock(each_handler_info.Info.Start, each_handler_info.Info.End);
        }

        if (options[CflowCheckOptions::ALLOW_JMP_HANDLER_TO_HANDLER]) {
            // jumps from handler to start of other handlers are allowed, so
            // prepare mask accordingly
            // [....handler1...]......
            // .....[....handler2....]
            // allow or not jumps to the start of handler2 ?
            // currently such jumps are allowed, i.e. priority of allowance is higher

            // remove marks at starts
            for (const auto &h_info : cflow_info.ExcHandlers()) {
                inst_map.ClearCodeBlock(h_info.Info.Start, 1);
            }
        }
    }

    const CflowExcHandlerInfo *prev_handler_info = nullptr;
    for (const auto &handler_info : cflow_info.ExcHandlers()) {
        if (!options[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_HANDLER]) {
            if (prev_handler_info != nullptr) {
                inst_map.MarkCodeBlock(prev_handler_info->Info.Start, prev_handler_info->Info.End);
                if (options[CflowCheckOptions::ALLOW_JMP_HANDLER_TO_HANDLER]) {
                    inst_map.ClearCodeBlock(prev_handler_info->Info.Start, 1);
                }
            }
            inst_map.ClearCodeBlock(handler_info.Info.Start, handler_info.Info.End);
            prev_handler_info = &handler_info;
        }

        // finally check jumps in the handler with built inst_map
        if (handler_info.Info.JmpsMap.IsConflictingWith(inst_map)) {
            const uint8_t *jmp_pc = nullptr;
            const uint8_t *tgt_pc = nullptr;
            if (!handler_info.Info.JmpsMap.GetFirstConflictingJump(inst_map, &jmp_pc, &tgt_pc)) {
                LOG_VERIFIER_CFLOW_INTERNAL_ERROR();
                return {};
            }
            report_incorrect_jump(jmp_pc, tgt_pc, "Prohibited jump out from exception handler detected.");
            return {};
        }
    }

    return true;
}

template <class F>
static bool CheckExcHandlers(CflowCheckFlags options, const CacheOfRuntimeThings::CachedMethod &method,
                             const CflowMethodInfo &cflow_info, const F &report_incorrect_jump)
{
    if (!CheckFallthroughFromBlock(options, cflow_info, method)) {
        return false;
    }

    // no handlers - no problems :)
    if (cflow_info.ExcHandlers().empty()) {
        return true;
    }

    if (!CheckJmpIntoExcHandler(options, cflow_info, report_incorrect_jump)) {
        return false;
    }

    if (!CheckFallthroughFromExcHandler(options, cflow_info, method)) {
        return false;
    }

    return CheckJmpOutExcHandler(options, cflow_info, report_incorrect_jump);
}

PandaUniquePtr<CflowMethodInfo> CheckCflow(CflowCheckFlags options, const CacheOfRuntimeThings::CachedMethod &method)
{
    bool sizeless_handlers_present = false;
    auto cflow_info = GetCflowMethodInfo(method, &sizeless_handlers_present);
    if (!cflow_info) {
        return {};
    }

    const uint8_t *method_pc_start_ptr = method.bytecode;
    const uint8_t *method_pc_end_ptr =
        &method_pc_start_ptr[method.bytecode_size - 1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    auto report_incorrect_jump = [&method, &method_pc_start_ptr](const uint8_t *jump_pc, const uint8_t *jump_target,
                                                                 const char *msg) {
        LOG_VERIFIER_CFLOW_INVALID_JUMP_TARGET(CacheOfRuntimeThings::GetName(method),
                                               (OffsetAsHexStr(method_pc_start_ptr, jump_target)),
                                               (OffsetAsHexStr(method_pc_start_ptr, jump_pc)), msg);
    };

    if (!CheckCode(*cflow_info, method_pc_end_ptr, report_incorrect_jump)) {
        return {};
    }

    // stop checks if there are any exception handlers w/o size
    if (sizeless_handlers_present) {
        return cflow_info;
    }

    if (!CheckExcHandlers(options, method, *cflow_info, report_incorrect_jump)) {
        return {};
    }

    return cflow_info;
}

}  // namespace panda::verifier
