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

#ifndef PANDA_RUNTIME_MEM_GC_GENERATIONAL_GC_BASE_H_
#define PANDA_RUNTIME_MEM_GC_GENERATIONAL_GC_BASE_H_

#include "runtime/mem/gc/lang/gc_lang.h"
#include "runtime/include/mem/allocator.h"

namespace panda::mem {

/**
 * Base class for generational GC
 */
template <class LanguageConfig>
class GenerationalGC : public GCLang<LanguageConfig> {
public:

protected:
    GenerationalGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
        : GCLang<LanguageConfig>(object_allocator, settings)
    {
    }
    ~GenerationalGC() override = default;
    DEFAULT_MOVE_SEMANTIC(GenerationalGC);
    DEFAULT_COPY_SEMANTIC(GenerationalGC);
    virtual bool ShouldRunTenuredGC(const GCTask &task);

    void DisableTenuredGC()
    {
        major_period_ = DISABLED_MAJOR_PERIOD;  // Disable tenured GC temporarily.
    }

    void RestoreTenuredGC()
    {
        major_period_ = DEFAULT_MAJOR_PERIOD;
    }

    ALWAYS_INLINE size_t GetMajorPeriod() const
    {
        return major_period_;
    }

    void PostForkCallback() override
    {
        GenerationalGC<LanguageConfig>::RestoreTenuredGC();
    }

    void WaitForGC(const GCTask &task) override;

protected:
    class MemStats {
    public:
        ALWAYS_INLINE void RecordCountFreedYoung(size_t count)
        {
            young_free_object_count_ += count;
        }

        ALWAYS_INLINE void RecordSizeFreedYoung(size_t size)
        {
            young_free_object_size_ += size;
        }

        ALWAYS_INLINE void RecordCountMovedYoung(size_t count)
        {
            young_move_object_count_ += count;
        }

        ALWAYS_INLINE void RecordSizeMovedYoung(size_t size)
        {
            young_move_object_size_ += size;
        }

        ALWAYS_INLINE void RecordCountFreedTenured(size_t count)
        {
            young_free_object_count_ += count;
        }

        ALWAYS_INLINE void RecordSizeFreedTenured(size_t size)
        {
            young_free_object_size_ += size;
        }

        void Reset()
        {
            young_free_object_count_ = 0U;
            young_free_object_size_ = 0U;
            young_move_object_count_ = 0U;
            young_move_object_size_ = 0U;
            tenured_free_object_count_ = 0U;
            tenured_free_object_size_ = 0U;
        }

        PandaString Dump();

    private:
        uint32_t young_free_object_count_ {0U};
        uint64_t young_free_object_size_ {0U};
        uint32_t young_move_object_count_ {0U};
        uint64_t young_move_object_size_ {0U};
        uint32_t tenured_free_object_count_ {0U};
        uint64_t tenured_free_object_size_ {0U};

        friend class GenerationalGC;
    };

    MemStats mem_stats_;  // NOLINT(misc-non-private-member-variables-in-classes)

private:
    static constexpr size_t DEFAULT_MAJOR_PERIOD = 3;
    static constexpr size_t DISABLED_MAJOR_PERIOD = 65535;
    size_t major_period_ {DEFAULT_MAJOR_PERIOD};
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GENERATIONAL_GC_BASE_H_
