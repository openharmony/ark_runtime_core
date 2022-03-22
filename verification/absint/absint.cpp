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

#include "absint.h"
#include "exec_context.h"
#include "verification_context.h"

#include "verification/job_queue/job.h"
#include "verification/job_queue/cache.h"

#include "value/abstract_typed_value.h"
#include "type/type_system.h"

#include "cflow/cflow_info.h"

#include "runtime/include/method.h"

#include "macros.h"

#include <optional>

#include "abs_int_inl.h"

#include "util/str.h"
#include "util/misc.h"

#include <utility>
#include <tuple>
#include <functional>

namespace panda::verifier {

#include "abs_int_inl_gen.h"

}  // namespace panda::verifier

namespace panda::verifier {

VerificationContext PrepareVerificationContext(PandaTypes *panda_types_ptr, const Job &job)
{
    ASSERT(panda_types_ptr != nullptr);
    auto &panda_types = *panda_types_ptr;

    auto &cached_method = job.JobCachedMethod();

    auto &klass = cached_method.klass.get();

    Type method_class_type = panda_types.TypeOf(klass);

    VerificationContext verif_ctx {panda_types, job, method_class_type};

    auto &cflow_info = verif_ctx.CflowInfo();
    auto &exec_ctx = verif_ctx.ExecCtx();

    LOG_VERIFIER_DEBUG_METHOD_VERIFICATION(cached_method.name);

    // 1. Build initial reg_context for the method entry

    RegContext &reg_ctx = verif_ctx.ExecCtx().CurrentRegContext();
    reg_ctx.Clear();

    auto num_vregs = cached_method.num_vregs;

    const auto &signature = verif_ctx.Types().MethodSignature(cached_method);

    for (size_t idx = 0; idx < signature.size() - 1; ++idx) {
        const Type &t = panda_types.TypeOf(signature[idx]);
        reg_ctx[num_vregs++] = AbstractTypedValue {t, verif_ctx.NewVar(), AbstractTypedValue::Start {}, idx};
    }
    LOG_VERIFIER_DEBUG_REGISTERS("registers =",
                                 reg_ctx.DumpRegs([&panda_types](const auto &t) { return panda_types.ImageOf(t); }));

    verif_ctx.SetReturnType(panda_types.TypeOf(signature.back()));

    LOG_VERIFIER_DEBUG_RESULT(panda_types.ImageOf(verif_ctx.ReturnType()));

    // 2. Add checkpoint for exc. handlers

    for (const auto &exc_handler : cflow_info.ExcHandlers()) {
        auto &&handler = [&exec_ctx](const uint8_t *pc) {
            exec_ctx.SetCheckPoint(pc);
            return true;
        };
        cflow_info.ExcSrcMap().ForSourcesInRange(exc_handler.ScopeStart, exc_handler.ScopeEnd, handler);
    }

    // 3. Add start entry of method

    const uint8_t *method_pc_start_ptr = cached_method.bytecode;

    verif_ctx.ExecCtx().AddEntryPoint(method_pc_start_ptr, EntryPointType::METHOD_BODY);
    verif_ctx.ExecCtx().StoreCurrentRegContextForAddr(method_pc_start_ptr);

    return verif_ctx;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
VerificationStatus VerifyMethod(VerificationLevel v_level, VerificationContext *v_ctx)
{
    ASSERT(v_level == VerificationLevel::LEVEL0);
    ASSERT(v_ctx != nullptr);

    const auto &debug_opts = Runtime::GetCurrent()->GetVerificationOptions().Debug;

    VerificationContext &verif_ctx = *v_ctx;
    auto &cflow_info = verif_ctx.CflowInfo();
    auto &exec_ctx = verif_ctx.ExecCtx();

    // 1. Start main loop: get entry point with context, process, repeat

    const uint8_t *entry_point = nullptr;
    EntryPointType entry_type;
    ExecContext::Status status;
    bool was_warnings = false;
    while ((status = exec_ctx.GetEntryPointForChecking(&entry_point, &entry_type)) == ExecContext::Status::OK) {
#ifndef NDEBUG
        const void *code_start = cflow_info.InstMap().AddrStart<const void *>();
        LOG_VERIFIER_DEBUG_CODE_BLOCK_VERIFICATION(
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry_point) - reinterpret_cast<uintptr_t>(code_start)),
            (entry_type == EntryPointType::METHOD_BODY ? "method body" : "exception handler"));
#endif
        auto result = AbstractInterpret(v_level, &verif_ctx, entry_point, entry_type);
        if (debug_opts.Allow.ErrorInExceptionHandler && entry_type == EntryPointType::EXCEPTION_HANDLER &&
            result == VerificationStatus::ERROR) {
            result = VerificationStatus::WARNING;
        }
        if (result == VerificationStatus::ERROR) {
            return result;
        }
        was_warnings = was_warnings || (result == VerificationStatus::WARNING);
    }

    // 2. Calculate contexts for exception handlers scopes

    PandaUnorderedMap<std::pair<const uint8_t *, const uint8_t *>, RegContext> scope_reg_context;

    PandaMultiMap<const uint8_t *, std::reference_wrapper<const CflowExcHandlerInfo>> sorted_handlers;

    for (const auto &exc_handler : cflow_info.ExcHandlers()) {
        sorted_handlers.insert(std::make_pair(exc_handler.ScopeStart, std::cref(exc_handler)));
    }

    for (const auto &kv : sorted_handlers) {
        const auto &exc_handler = kv.second.get();
        auto scope = std::make_pair(exc_handler.ScopeStart, exc_handler.ScopeEnd);
#ifndef NDEBUG
        const void *code_start = cflow_info.InstMap().AddrStart<const void *>();
        auto take_address = [&](const void *ptr) {
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(code_start));
        };
#endif

        if (scope_reg_context.count(scope) == 0) {
#ifndef NDEBUG
            LOG_VERIFIER_DEBUG_EXCEPTION_HANDLER_COMMON_CONTEXT_COMPUTATION(take_address(exc_handler.Info.Start), "",
                                                                            take_address(exc_handler.ScopeStart),
                                                                            take_address(exc_handler.ScopeEnd));
#endif

#ifndef NDEBUG
            auto image_of = [&verif_ctx](const auto &t) { return verif_ctx.Types().ImageOf(t); };
#endif

            RegContext reg_context;
            bool first = true;
            exec_ctx.ForContextsOnCheckPointsInRange(
#ifndef NDEBUG
                exc_handler.ScopeStart, exc_handler.ScopeEnd,
                [&cflow_info, &reg_context, &image_of, &first](const uint8_t *pc, const RegContext &ctx) {
#else
                exc_handler.ScopeStart, exc_handler.ScopeEnd,
                [&cflow_info, &reg_context, &first](const uint8_t *pc, const RegContext &ctx) {
#endif
                    if (cflow_info.ExcSrcMap().IsExceptionSource(pc)) {
#ifndef NDEBUG
                        LOG_VERIFIER_DEBUG_REGISTERS("+", ctx.DumpRegs(image_of));
#endif
                        if (first) {
                            first = false;
                            reg_context = ctx;
                        } else {
                            reg_context &= ctx;
                        }
                    }
                    return true;
                });
#ifndef NDEBUG
            LOG_VERIFIER_DEBUG_REGISTERS("=", reg_context.DumpRegs(image_of));
#endif

            reg_context.RemoveInconsistentRegs();

#ifndef NDEBUG
            if (reg_context.HasInconsistentRegs()) {
                LOG_VERIFIER_COMMON_CONTEXT_INCONSISTENT_REGISTER_HEADER();
                for (int reg_num : reg_context.InconsistentRegsNums()) {
                    LOG(DEBUG, VERIFIER) << AbsIntInstructionHandler::RegisterName(reg_num);
                }
            }
#endif
            scope_reg_context.insert(std::make_pair(scope, reg_context));
        }

        const auto *exception = exc_handler.CachedException;

        auto &reg_context = scope_reg_context[scope];

#ifndef NDEBUG
        LOG(DEBUG, VERIFIER) << "Exception handler at " << std::hex << "0x" << take_address(exc_handler.Info.Start)
                             << (exception == nullptr ? PandaString {""}
                                                      : PandaString {", for exception '"} + exception->GetName() + "' ")
                             << ", try block scope: [ "
                             << "0x" << take_address(exc_handler.ScopeStart) << ", "
                             << "0x" << take_address(exc_handler.ScopeEnd) << " ]";
#endif

        Type exception_type;

        if (exception != nullptr) {
            exception_type = verif_ctx.Types().TypeOf(*exc_handler.CachedException);
        } else {
            auto lang = verif_ctx.GetJob().JobCachedMethod().klass.get().source_lang;
            if (lang == panda_file::SourceLang::PANDA_ASSEMBLY) {
                exception_type = verif_ctx.Types().PandaObject();
            }
        }

        if (exception_type.IsValid()) {
            const int ACC = -1;
            reg_context[ACC] = AbstractTypedValue {exception_type, verif_ctx.NewVar()};
        }

        exec_ctx.CurrentRegContext() = reg_context;
        exec_ctx.AddEntryPoint(exc_handler.Info.Start, EntryPointType::EXCEPTION_HANDLER);
        exec_ctx.StoreCurrentRegContextForAddr(exc_handler.Info.Start);

        while ((status = exec_ctx.GetEntryPointForChecking(&entry_point, &entry_type)) == ExecContext::Status::OK) {
#ifndef NDEBUG
            const void *method_code_start = cflow_info.InstMap().AddrStart<const void *>();
            LOG_VERIFIER_DEBUG_CODE_BLOCK_VERIFICATION(
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry_point) -
                                      reinterpret_cast<uintptr_t>(method_code_start)),
                (entry_type == EntryPointType::METHOD_BODY ? "method body" : "exception handler"));
#endif
            auto result = AbstractInterpret(v_level, &verif_ctx, entry_point, entry_type);
            if (debug_opts.Allow.ErrorInExceptionHandler && entry_type == EntryPointType::EXCEPTION_HANDLER &&
                result == VerificationStatus::ERROR) {
                result = VerificationStatus::WARNING;
            }
            if (result == VerificationStatus::ERROR) {
                return result;
            }
            was_warnings = was_warnings || (result == VerificationStatus::WARNING);
        }
    }

    // 3. Add marking at Sync() and calc unmarked blocks at the end

    if (status == ExecContext::Status::NO_ENTRY_POINTS_WITH_CONTEXT) {
        return VerificationStatus::WARNING;
    }

    return was_warnings ? VerificationStatus::WARNING : VerificationStatus::OK;
}

}  // namespace panda::verifier
