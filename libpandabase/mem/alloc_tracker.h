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

#ifndef PANDA_LIBPANDABASE_MEM_ALLOC_TRACKER_H_
#define PANDA_LIBPANDABASE_MEM_ALLOC_TRACKER_H_

#include <list>
#include <map>
#include <atomic>
#include <memory>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "space.h"
#include "os/mutex.h"
#include "utils/span.h"

namespace panda {

class AllocTracker {
public:
    AllocTracker() = default;
    virtual ~AllocTracker() = default;

    virtual void TrackAlloc(void *addr, size_t size, SpaceType space) = 0;
    virtual void TrackFree(void *addr) = 0;

    virtual void Dump() {}
    virtual void Dump([[maybe_unused]] std::ostream &out) {}
    virtual void DumpMemLeaks([[maybe_unused]] std::ostream &out) {}

    NO_COPY_SEMANTIC(AllocTracker);
    NO_MOVE_SEMANTIC(AllocTracker);
};

class SimpleAllocTracker final : public AllocTracker {
public:
    void TrackAlloc(void *addr, size_t size, [[maybe_unused]] SpaceType space) override
    {
        os::memory::LockHolder lock(lock_);
        internal_alloc_counter_++;
        total_allocated_ += size;
        current_allocated_ += size;
        peak_allocated_ = std::max(peak_allocated_, current_allocated_);
        auto ins_result = allocated_addresses_.insert({addr, AllocInfo(internal_alloc_counter_, size)});
        ASSERT(ins_result.second);
        static_cast<void>(ins_result);  // Fix compilation in release
    }

    void TrackFree(void *addr) override
    {
        os::memory::LockHolder lock(lock_);
        internal_free_counter_++;
        auto it = allocated_addresses_.find(addr);
        ASSERT(it != allocated_addresses_.end());
        size_t size = it->second.GetSize();
        allocated_addresses_.erase(it);
        current_allocated_ -= size;
    }

    void Dump() override
    {
        Dump(std::cout);
    }

    void Dump(std::ostream &out) override
    {
        out << "Internal memory allocations:\n";
        out << "allocations count: " << internal_alloc_counter_ << "\n";
        out << "  total allocated: " << total_allocated_ << "\n";
        out << "   peak allocated: " << peak_allocated_ << "\n";
    }

    void DumpMemLeaks(std::ostream &out) override
    {
        out << "=== Allocated Internal Memory: ===" << std::endl;
        for (auto it : allocated_addresses_) {
            out << std::hex << it.first << ", allocation #" << std::dec << it.second.GetAllocNumber() << std::endl;
        }
        out << "==================================" << std::endl;
    }

private:
    class AllocInfo {
    public:
        AllocInfo(size_t alloc_number, size_t size) : alloc_number_(alloc_number), size_(size) {}
        ~AllocInfo() = default;
        DEFAULT_COPY_SEMANTIC(AllocInfo);
        DEFAULT_MOVE_SEMANTIC(AllocInfo);

        size_t GetAllocNumber() const
        {
            return alloc_number_;
        }

        size_t GetSize() const
        {
            return size_;
        }

    private:
        size_t alloc_number_;
        size_t size_;
    };

private:
    size_t internal_alloc_counter_ = 0;
    size_t internal_free_counter_ = 0;
    size_t total_allocated_ = 0;
    size_t current_allocated_ = 0;
    size_t peak_allocated_ = 0;
    std::unordered_map<void *, AllocInfo> allocated_addresses_;
    os::memory::Mutex lock_;
};

class DetailAllocTracker final : public AllocTracker {
public:
    static constexpr uint32_t ALLOC_TAG = 1;
    static constexpr uint32_t FREE_TAG = 2;

    void TrackAlloc(void *addr, size_t size, SpaceType space) override;
    void TrackFree(void *addr) override;

    void Dump() override;
    void Dump(std::ostream &out) override;
    void DumpMemLeaks(std::ostream &out) override;

private:
    using Stacktrace = std::vector<uintptr_t>;

    class AllocInfo {
    public:
        AllocInfo(uint32_t id, uint32_t size, uint32_t space, uint32_t stacktrace_id)
            : id_(id), size_(size), space_(space), stacktrace_id_(stacktrace_id)
        {
        }

        uint32_t GetTag() const
        {
            return tag_;
        }

        uint32_t GetId() const
        {
            return id_;
        }

        uint32_t GetSize() const
        {
            return size_;
        }

        uint32_t GetSpace() const
        {
            return space_;
        }

        uint32_t GetStacktraceId() const
        {
            return stacktrace_id_;
        }

    private:
        const uint32_t tag_ = ALLOC_TAG;
        uint32_t id_;
        uint32_t size_;
        uint32_t space_;
        uint32_t stacktrace_id_;
    };

    class FreeInfo {
    public:
        explicit FreeInfo(uint32_t alloc_id) : alloc_id_(alloc_id) {}

        uint32_t GetTag() const
        {
            return tag_;
        }

        uint32_t GetAllocId() const
        {
            return alloc_id_;
        }

    private:
        const uint32_t tag_ = FREE_TAG;
        uint32_t alloc_id_;
    };

    void AllocArena() REQUIRES(mutex_);
    uint32_t WriteStacks(std::ostream &out, std::map<uint32_t, uint32_t> *id_map) REQUIRES(mutex_);

private:
    std::atomic<size_t> alloc_counter_ = 0;
    uint32_t cur_id_ GUARDED_BY(mutex_) = 0;
    Span<uint8_t> cur_arena_ GUARDED_BY(mutex_);
    std::list<std::unique_ptr<uint8_t[]>> arenas_ GUARDED_BY(mutex_);  // NOLINT(modernize-avoid-c-arrays)
    std::list<Stacktrace> stacktraces_ GUARDED_BY(mutex_);
    std::map<void *, AllocInfo *> cur_allocs_ GUARDED_BY(mutex_);
    os::memory::Mutex mutex_;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_ALLOC_TRACKER_H_
