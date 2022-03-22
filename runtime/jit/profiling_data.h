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

#ifndef PANDA_RUNTIME_JIT_PROFILING_DATA_H_
#define PANDA_RUNTIME_JIT_PROFILING_DATA_H_

#include "macros.h"
#include <array>
#include <numeric>

#include <cstdint>

namespace panda {

class Class;

class CallSiteInlineCache {
public:
    static constexpr size_t CLASSES_COUNT = 4;
    static constexpr uintptr_t MEGAMORPHIC_FLAG = static_cast<uintptr_t>(-1);

    explicit CallSiteInlineCache(uintptr_t pc) : bytecode_pc_(pc) {}
    ~CallSiteInlineCache() = default;
    NO_MOVE_SEMANTIC(CallSiteInlineCache);
    NO_COPY_SEMANTIC(CallSiteInlineCache);

    void Init(uintptr_t pc)
    {
        SetBytecodePc(pc);
        std::fill(classes_.begin(), classes_.end(), nullptr);
    }

    void UpdateInlineCaches(Class *cls)
    {
        for (uint32_t i = 0; i < classes_.size();) {
            auto *class_atomic = reinterpret_cast<std::atomic<Class *> *>(&(classes_[i]));
            auto stored_class = class_atomic->load(std::memory_order_acquire);
            // Check that the call is already megamorphic
            if (i == 0 && stored_class == reinterpret_cast<Class *>(MEGAMORPHIC_FLAG)) {
                return;
            }
            if (stored_class == cls) {
                return;
            }
            if (stored_class == nullptr) {
                if (!class_atomic->compare_exchange_weak(stored_class, cls, std::memory_order_acq_rel)) {
                    continue;
                }
                return;
            }
            i++;
        }
        // Megamorphic call, disable devirtualization for this call site.
        auto *class_atomic = reinterpret_cast<std::atomic<Class *> *>(&(classes_[0]));
        class_atomic->store(reinterpret_cast<Class *>(MEGAMORPHIC_FLAG), std::memory_order_release);
    }

    auto GetBytecodePc() const
    {
        return bytecode_pc_.load(std::memory_order_acquire);
    }

    void SetBytecodePc(uintptr_t pc)
    {
        bytecode_pc_.store(pc, std::memory_order_release);
    }

    auto GetClasses()
    {
        return Span<Class *>(classes_.data(), GetClassesCount());
    }

    size_t GetClassesCount() const
    {
        size_t classes_count = 0;
        for (uint32_t i = 0; i < classes_.size();) {
            auto *class_atomic = reinterpret_cast<std::atomic<Class *> const *>(&(classes_[i]));
            auto stored_class = class_atomic->load(std::memory_order_acquire);
            if (stored_class != nullptr) {
                classes_count++;
            }
            i++;
        }
        return classes_count;
    }

    static bool IsMegamorphic(Class *cls)
    {
        auto *class_atomic = reinterpret_cast<std::atomic<Class *> *>(&cls);
        return class_atomic->load(std::memory_order_acquire) == reinterpret_cast<Class *>(MEGAMORPHIC_FLAG);
    }

private:
    std::atomic_uintptr_t bytecode_pc_;
    std::array<Class *, CLASSES_COUNT> classes_ {};
};

class ProfilingData {
public:
    explicit ProfilingData(size_t inline_caches_num) : inline_caches_num_(inline_caches_num)
    {
        auto data = GetInlineCaches().SubSpan<uint8_t>(0, GetInlineCaches().size());
        std::fill(data.begin(), data.end(), 0);
    }
    ~ProfilingData() = default;
    NO_MOVE_SEMANTIC(ProfilingData);
    NO_COPY_SEMANTIC(ProfilingData);

    Span<CallSiteInlineCache> GetInlineCaches()
    {
        return Span<CallSiteInlineCache>(inline_caches_, inline_caches_num_);
    }

    CallSiteInlineCache *FindInlineCache(uintptr_t pc)
    {
        auto ics = GetInlineCaches();
        auto ic = std::lower_bound(ics.begin(), ics.end(), pc,
                                   [](const auto &a, uintptr_t counter) { return a.GetBytecodePc() < counter; });
        return (ic == ics.end() || ic->GetBytecodePc() != pc) ? nullptr : &*ic;
    }

    void UpdateInlineCaches(uintptr_t pc, Class *cls)
    {
        auto ic = FindInlineCache(pc);
        ASSERT(ic != nullptr);
        if (ic != nullptr) {
            ic->UpdateInlineCaches(cls);
        }
    }

private:
    size_t inline_caches_num_ {};
    __extension__ CallSiteInlineCache inline_caches_[0];  // NOLINT(modernize-avoid-c-arrays)
};

}  // namespace panda

#endif  // PANDA_RUNTIME_JIT_PROFILING_DATA_H_
