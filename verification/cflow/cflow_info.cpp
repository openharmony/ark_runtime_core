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

#include "cflow_info.h"
#include "bytecode_instruction-inl.h"
#include "file_items.h"
#include "macros.h"
#include "include/runtime.h"
#include "utils/logger.h"
#include "util/str.h"
#include "cflow_status.h"
#include "cflow_iterate_inl.h"
#include "verification/job_queue/cache.h"
#include "verification/job_queue/job_queue.h"
#include "verification/cflow/cflow_common.h"
#include "verifier_messages.h"

#include <iomanip>

namespace panda::verifier {

CflowStatus FillInstructionsMap(InstructionsMap *inst_map_ptr, ExceptionSourceMap *exc_src_map_ptr)
{
    auto &inst_map = *inst_map_ptr;
    auto &exc_src_map = *exc_src_map_ptr;
    auto status = IterateOverInstructions(
        inst_map.AddrStart<const uint8_t *>(), inst_map.AddrStart<const uint8_t *>(),
        inst_map.AddrEnd<const uint8_t *>(),
        [&inst_map, &exc_src_map]([[maybe_unused]] auto typ, const uint8_t *pc, size_t sz, bool exception_source,
                                  [[maybe_unused]] auto tgt) -> std::optional<CflowStatus> {
            if (!inst_map.PutInstruction(pc, sz)) {
                LOG_VERIFIER_CFLOW_INVALID_INSTRUCTION(OffsetAsHexStr(inst_map.AddrStart<void *>(), pc));
                return CflowStatus::ERROR;
            }
            if (exception_source && !exc_src_map.PutExceptionSource(pc)) {
                LOG_VERIFIER_CFLOW_INVALID_INSTRUCTION(OffsetAsHexStr(inst_map.AddrStart<void *>(), pc));
                return CflowStatus::ERROR;
            }
            const uint8_t *next_inst_pc = &pc[sz];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (next_inst_pc <= inst_map.AddrEnd<const uint8_t *>()) {
                return std::nullopt;
            }
            return CflowStatus::OK;
        });
    return status;
}

CflowStatus FillJumpsMapAndGetLastInstructionType(const InstructionsMap &inst_map, JumpsMap *jumps_map_ptr,
                                                  const uint8_t *pc_start_ptr, const uint8_t *pc_end_ptr,
                                                  InstructionType *last_inst_type_ptr)
{
    ASSERT(jumps_map_ptr != nullptr);

    JumpsMap &jumps_map = *jumps_map_ptr;
    auto result = IterateOverInstructions(
        pc_start_ptr, inst_map.AddrStart<const uint8_t *>(), inst_map.AddrEnd<const uint8_t *>(),
        [&pc_end_ptr, &inst_map, &jumps_map, last_inst_type_ptr](InstructionType typ, const uint8_t *pc, size_t sz,
                                                                 [[maybe_unused]] bool exception_source,
                                                                 const uint8_t *target) -> std::optional<CflowStatus> {
            const uint8_t *next_inst_pc = &pc[sz];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (typ == InstructionType::JUMP || typ == InstructionType::COND_JUMP) {
                if (!inst_map.CanJumpTo(target)) {
                    LOG_VERIFIER_CFLOW_INVALID_JUMP_INTO_MIDDLE_OF_INSTRUCTION(
                        OffsetAsHexStr(inst_map.AddrStart<void *>(), pc),
                        OffsetAsHexStr(inst_map.AddrStart<void *>(), target));
                    return CflowStatus::ERROR;
                }
                if (!jumps_map.PutJump(pc, target)) {
                    LOG_VERIFIER_CFLOW_INVALID_JUMP(OffsetAsHexStr(inst_map.AddrStart<void *>(), pc),
                                                    OffsetAsHexStr(inst_map.AddrStart<void *>(), target));
                    return CflowStatus::ERROR;
                }
            }
            if (next_inst_pc > pc_end_ptr) {
                // last instruction should terminate control-flow: jump, return, throw
                // conditional jumps is problem here, since condition in general could not be precisely
                // evaluated
                if (last_inst_type_ptr != nullptr) {
                    *last_inst_type_ptr = typ;
                }

                return CflowStatus::OK;
            }
            return std::nullopt;
        });
    return result;
}

CflowStatus FillCflowCodeBlockInfo(const InstructionsMap &inst_map, CflowCodeBlockInfo *code_block_info)
{
    return FillJumpsMapAndGetLastInstructionType(inst_map, &code_block_info->JmpsMap, code_block_info->Start,
                                                 code_block_info->End, &code_block_info->LastInstType);
}

#ifndef NDEBUG
template <class F>
static void DebugDump(const CacheOfRuntimeThings::CachedCatchBlock &catch_block, const F &get_offset)
{
    auto try_start_pc = catch_block.try_block_start;
    auto try_end_pc = catch_block.try_block_end;
    auto &exception = catch_block.exception_type;
    auto pc_start_ptr = catch_block.handler_bytecode;
    auto size = catch_block.handler_bytecode_size;

    bool catch_all =
        CacheOfRuntimeThings::IsDescriptor(exception) && !CacheOfRuntimeThings::GetDescriptor(exception).IsValid();

    CacheOfRuntimeThings::CachedClass *cached_class_of_exception =
        CacheOfRuntimeThings::IsRef(exception) ? &CacheOfRuntimeThings::GetRef(exception) : nullptr;

    auto exc_name = (catch_all || cached_class_of_exception == nullptr)
                        ? PandaString {"null"}
                        : ClassHelper::GetName<PandaString>(cached_class_of_exception->name);
    auto try_range = PandaString {"[ "} + get_offset(try_start_pc) + ", " + get_offset(try_end_pc) + " ]";
    PandaString exc_handler_range;
    if (size == 0) {
        exc_handler_range = get_offset(pc_start_ptr);
    } else {
        exc_handler_range = PandaString {"[ "};
        exc_handler_range += get_offset(pc_start_ptr) + ", ";
        exc_handler_range += get_offset(&pc_start_ptr[size - 1]);  // NOLINT
        exc_handler_range += " ]";
    }
    LOG_VERIFIER_CFLOW_EXC_HANDLER_INFO(exc_handler_range, try_range, exc_name);
}
#else
template <class F>
static void DebugDump([[maybe_unused]] const CacheOfRuntimeThings::CachedCatchBlock &catch_block,
                      [[maybe_unused]] const F &get_offset)
{
}
#endif

template <class F>
static bool ProcessCatchBlocks(const CacheOfRuntimeThings::CachedMethod &method, AddrMap *addr_map, const F &get_offset,
                               const InstructionsMap *inst_map, PandaVector<CflowExcHandlerInfo> *exc_handlers,
                               bool *sizeless_handlers_present)
{
    bool result = true;

    *sizeless_handlers_present = false;

    LOG(DEBUG, VERIFIER) << "Tracing exception handlers.";

    for (const auto &catch_block : method.catch_blocks) {
        auto try_start_pc = catch_block.try_block_start;
        auto try_end_pc = catch_block.try_block_end;
        auto &exception = catch_block.exception_type;
        auto pc_start_ptr = catch_block.handler_bytecode;
        auto size = catch_block.handler_bytecode_size;

        CacheOfRuntimeThings::CachedClass *cached_class_of_exception =
            CacheOfRuntimeThings::IsRef(exception) ? &CacheOfRuntimeThings::GetRef(exception) : nullptr;

        DebugDump(catch_block, get_offset);

        if (size == 0) {
            LOG_VERIFIER_CFLOW_CANNOT_CHECK_EXC_HANDLER_DUE_TO_SIZE();
            CflowCodeBlockInfo block_info {
                pc_start_ptr, pc_start_ptr, {pc_start_ptr, pc_start_ptr}, InstructionType::NORMAL};
            exc_handlers->push_back({block_info, try_start_pc, try_end_pc, cached_class_of_exception});
            *sizeless_handlers_present = true;
        } else {
            const uint8_t *pc_end_ptr =
                &pc_start_ptr[size - 1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

            if (!addr_map->Clear(pc_start_ptr, pc_end_ptr)) {
                LOG_VERIFIER_CFLOW_CANNOT_CLEAR_MARKS_OF_EXC_HANDLER_BLOCK();
                result = false;
                break;
            }

            CflowCodeBlockInfo block_info {
                pc_start_ptr,
                pc_end_ptr,
                {inst_map->AddrStart<const uint8_t *>(), inst_map->AddrEnd<const uint8_t *>()},
                InstructionType::NORMAL};

            exc_handlers->push_back({block_info, try_start_pc, try_end_pc, cached_class_of_exception});
            if (FillCflowCodeBlockInfo(*inst_map, &exc_handlers->back().Info) == CflowStatus::ERROR) {
                LOG_VERIFIER_CFLOW_CANNOT_FILL_JUMPS_OF_EXC_HANDLER_BLOCK();
                result = false;
                break;
            }
        }
    }
    return result;
}

PandaUniquePtr<CflowMethodInfo> GetCflowMethodInfo(const CacheOfRuntimeThings::CachedMethod &method,
                                                   bool *sizeless_handlers_present)
{
    const uint8_t *method_pc_start_ptr = method.bytecode;
    size_t code_size = method.bytecode_size;
    const uint8_t *method_pc_end_ptr =
        &method_pc_start_ptr[code_size - 1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    auto cflow_info = MakePandaUnique<CflowMethodInfo>(method_pc_start_ptr, code_size);

    LOG(DEBUG, VERIFIER) << method.name << "'";

    // 1. fill instructions map
    LOG(DEBUG, VERIFIER) << "Build instructions map.";
    if (FillInstructionsMap(&(*cflow_info).InstMap_, &(*cflow_info).ExcSrcMap_) == CflowStatus::ERROR) {
        LOG_VERIFIER_CFLOW_CANNOT_FILL_INSTRUCTIONS_MAP()
        return {};
    }

    // 2. fill jumps map
    LOG(DEBUG, VERIFIER) << "Build jumps map.";
    if (FillJumpsMapAndGetLastInstructionType((*cflow_info).InstMap_, &(*cflow_info).JmpsMap_, method_pc_start_ptr,
                                              method_pc_end_ptr, nullptr) == CflowStatus::ERROR) {
        LOG_VERIFIER_CFLOW_CANNOT_FILL_JUMPS_MAP()
        return {};
    }

    // 3. get method body blocks (exception handlers are not limited to the end of method)
    //    and exception handlers blocks at once
    AddrMap addr_map {method_pc_start_ptr, method_pc_end_ptr};
    addr_map.InvertMarks();

    auto get_offset = [&addr_map](const uint8_t *ptr) { return OffsetAsHexStr(addr_map.AddrStart<void *>(), ptr); };

    bool result = ProcessCatchBlocks(method, &addr_map, get_offset, &cflow_info->InstMap_, &cflow_info->ExcHandlers_,
                                     sizeless_handlers_present);

    if (!result) {
        return {};
    }

    LOG(DEBUG, VERIFIER) << "Trace method body code blocks.";
    addr_map.EnumerateMarkedBlocks<const uint8_t *>(
        [&result, &cflow_info, &get_offset](const uint8_t *pc_start_ptr, const uint8_t *pc_end_ptr) {
            (*cflow_info)
                .BodyInfo_.push_back(CflowCodeBlockInfo {pc_start_ptr, pc_end_ptr,
                                                         JumpsMap {(*cflow_info).InstMap_.AddrStart<const uint8_t *>(),
                                                                   (*cflow_info).InstMap_.AddrEnd<const uint8_t *>()},
                                                         InstructionType::NORMAL});
            if (FillCflowCodeBlockInfo((*cflow_info).InstMap_, &(*cflow_info).BodyInfo_.back()) == CflowStatus::ERROR) {
                LOG_VERIFIER_CFLOW_CANNOT_FILL_JUMPS_OF_CODE_BLOCK(get_offset(pc_start_ptr), get_offset(pc_end_ptr));
                return result = false;
            }
            return true;
        });

    if (!result) {
        return {};
    }

    return cflow_info;
}  // namespace panda::verifier

}  // namespace panda::verifier
