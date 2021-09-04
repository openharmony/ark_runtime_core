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

#ifndef PANDA_VERIFICATION_JOB_QUEUE_JOB_H_
#define PANDA_VERIFICATION_JOB_QUEUE_JOB_H_

#include "verification/job_queue/cache.h"
#include "verification/cflow/cflow_info.h"
#include "verification/verification_options.h"

#include "runtime/include/method.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace panda::verifier {
class Job {
public:
    using CachedField = CacheOfRuntimeThings::CachedField;
    using CachedMethod = CacheOfRuntimeThings::CachedMethod;
    using CachedClass = CacheOfRuntimeThings::CachedClass;

    Job(Method &method, const CachedMethod &ch_method,
        const VerificationOptions::MethodOptionsConfig::MethodOptions &options)
        : method_to_be_verified {method}, cached_method {std::cref(ch_method)}, method_options {options}
    {
    }

    ~Job() = default;

    void AddField(uint32_t offset, const CachedField &cached_field)
    {
        fields.emplace(offset, std::cref(cached_field));
    }

    void AddMethod(uint32_t offset, const CachedMethod &ch_method)
    {
        methods.emplace(offset, std::cref(ch_method));
    }

    void AddClass(uint32_t offset, const CachedClass &ch_method)
    {
        classes.emplace(offset, std::cref(ch_method));
    }

    bool IsFieldPresentForOffset(uint32_t offset) const
    {
        return fields.count(offset) != 0;
    }

    bool IsMethodPresentForOffset(uint32_t offset) const
    {
        return methods.count(offset) != 0;
    }

    bool IsClassPresentForOffset(uint32_t offset) const
    {
        return classes.count(offset) != 0;
    }

    const CachedField &GetField(uint32_t offset) const
    {
        return fields.at(offset);
    }

    const CachedMethod &GetMethod(uint32_t offset) const
    {
        return methods.at(offset);
    }

    const CachedClass &GetClass(uint32_t offset) const
    {
        return classes.at(offset);
    }

    Job *TakeNext()
    {
        auto nxt = next;
        next = nullptr;
        return nxt;
    }

    void SetNext(Job *nxt)
    {
        next = nxt;
    }

    const CachedMethod &JobCachedMethod() const
    {
        return cached_method;
    }

    Method &JobMethod() const
    {
        return method_to_be_verified;
    }

    const CflowMethodInfo &JobMethodCflow() const
    {
        return *cflow_info;
    }

    void SetMethodCflowInfo(PandaUniquePtr<CflowMethodInfo> &&cflow)
    {
        cflow_info = std::move(cflow);
    }

    template <typename Handler>
    void ForAllCachedClasses(Handler &&handler) const
    {
        for (const auto &item : classes) {
            handler(item.second.get());
        }
    }

    template <typename Handler>
    void ForAllCachedMethods(Handler &&handler) const
    {
        for (const auto &item : methods) {
            handler(item.second.get());
        }
    }

    template <typename Handler>
    void ForAllCachedFields(Handler &&handler) const
    {
        for (const auto &item : fields) {
            handler(item.second.get());
        }
    }

    const auto &Options() const
    {
        return method_options;
    }

private:
    Job *next {nullptr};
    Method &method_to_be_verified;
    std::reference_wrapper<const CachedMethod> cached_method;
    const VerificationOptions::MethodOptionsConfig::MethodOptions &method_options;
    PandaUniquePtr<CflowMethodInfo> cflow_info;

    // offset -> cache item
    PandaUnorderedMap<uint32_t, std::reference_wrapper<const CachedField>> fields;
    PandaUnorderedMap<uint32_t, std::reference_wrapper<const CachedMethod>> methods;
    PandaUnorderedMap<uint32_t, std::reference_wrapper<const CachedClass>> classes;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_JOB_QUEUE_JOB_H_
