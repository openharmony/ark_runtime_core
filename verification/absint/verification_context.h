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

#ifndef PANDA_VERIFICATION_ABSINT_VERIFICATION_CONTEXT_H_
#define PANDA_VERIFICATION_ABSINT_VERIFICATION_CONTEXT_H_

#include "exec_context.h"
#include "panda_types.h"

#include "cflow/cflow_info.h"
#include "value/variables.h"

#include "util/lazy.h"
#include "util/callable.h"

#include "verification/job_queue/job.h"
#include "verification/job_queue/cache.h"

#include "verification/type/type_systems.h"

#include "runtime/include/method.h"

#include "macros.h"

#include <functional>

namespace panda::verifier {
using CallIntoRuntimeHandler = callable<void(callable<void()>)>;

class VerificationContext {
public:
    using Var = Variables::Var;

    VerificationContext(PandaTypes &panda_types, const Job &job, Type method_class_type)
        : Types_ {panda_types},
          Job_ {job},
          MethodClass_ {method_class_type},
          ExecCtx_ {CflowInfo().InstMap().AddrStart<const uint8_t *>(),
                    CflowInfo().InstMap().AddrEnd<const uint8_t *>()}
    {
        // set checkpoints for reg_context storage
        // start of method is checkpoint too
        ExecCtx().SetCheckPoints(
            ConstLazyFetch(std::array<const uint8_t *, 1> {CflowInfo().InstMap().AddrStart<const uint8_t *>()}));
        ExecCtx().SetCheckPoints(CflowInfo().JmpsMap().AllTargetsLazy<const uint8_t *>());
        // set checkpoints for entries of exception handlers
        ExecCtx().SetCheckPoints(Transform(ConstLazyFetch(CflowInfo().ExcHandlers()),
                                           [](const auto &exc_handler) { return exc_handler.Info.Start; }));
        ExecCtx().SetCheckPoints(Transform(ConstLazyFetch(CflowInfo().ExcHandlers()),
                                           [](const auto &exc_handler) { return exc_handler.ScopeStart; }));
        ExecCtx().SetCheckPoints(Transform(ConstLazyFetch(CflowInfo().ExcHandlers()),
                                           [](const auto &exc_handler) { return exc_handler.ScopeEnd; }));
    }

    ~VerificationContext() = default;

    const Job &GetJob() const
    {
        return Job_;
    }

    const CflowMethodInfo &CflowInfo() const
    {
        return Job_.JobMethodCflow();
    }

    const CacheOfRuntimeThings::CachedMethod &GetCachedMethod() const
    {
        return Job_.JobCachedMethod();
    }

    Type GetMethodClass() const
    {
        return MethodClass_;
    }

    ExecContext &ExecCtx()
    {
        return ExecCtx_;
    }

    const ExecContext &ExecCtx() const
    {
        return ExecCtx_;
    }

    PandaTypes &Types()
    {
        return Types_;
    }

    Var NewVar()
    {
        return TypeSystems::GetVar(Types_.GetKind());
    }

    const Type &ReturnType() const
    {
        return ReturnType_;
    }

    void SetReturnType(const Type &type)
    {
        ReturnType_ = type;
    }

private:
    PandaTypes &Types_;
    const Job &Job_;
    Type ReturnType_;
    Type MethodClass_;
    ExecContext ExecCtx_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_ABSINT_VERIFICATION_CONTEXT_H_
