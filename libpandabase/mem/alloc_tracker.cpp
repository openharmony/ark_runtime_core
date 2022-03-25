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

#include "mem/alloc_tracker.h"

#include <cstring>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <fstream>
#include <functional>
#include <unordered_map>
#include "utils/logger.h"
#include "os/stacktrace.h"

namespace panda {

static constexpr size_t NUM_SKIP_FRAMES = 1;
static constexpr size_t ARENA_SIZE = 4096;
static constexpr size_t ENTRY_HDR_SIZE = sizeof(int32_t);

static const char *GetDumpFilePath()
{
#if defined(PANDA_TARGET_MOBILE)
    return "/data/local/tmp/memdump.bin";
#else
    return "memdump.bin";
#endif
}

static void Write(uint32_t val, std::ostream &out)
{
    out.write(reinterpret_cast<char *>(&val), sizeof(val));
}

static void Write(const std::string &str, std::ostream &out)
{
    Write(static_cast<uint32_t>(str.size()), out);
    out.write(str.c_str(), str.length());
}

static size_t CalcHash(const std::vector<uintptr_t> &st)
{
    size_t hash = 0;
    std::hash<uintptr_t> addr_hash;
    for (uintptr_t addr : st) {
        hash |= addr_hash(addr);
    }
    return hash;
}

// On mobile target getting a stacktrace is expensive operation.
// An application doesn't launch in timeout and get killed.
// This function is aimed to skip getting stacktraces for some allocations.
#if defined(PANDA_TARGET_MOBILE)
static bool SkipStacktrace(size_t num)
{
    static constexpr size_t FREQUENCY = 5;
    return num % FREQUENCY != 0;
}
#else
static bool SkipStacktrace([[maybe_unused]] size_t num)
{
    return false;
}
#endif

void DetailAllocTracker::TrackAlloc(void *addr, size_t size, SpaceType space)
{
    if (addr == nullptr) {
        return;
    }
    Stacktrace stacktrace = SkipStacktrace(++alloc_counter_) ? Stacktrace() : GetStacktrace();
    os::memory::LockHolder lock(mutex_);

    uint32_t stacktrace_id = stacktraces_.size();
    if (stacktrace.size() > NUM_SKIP_FRAMES) {
        stacktraces_.emplace_back(stacktrace.begin() + NUM_SKIP_FRAMES, stacktrace.end());
    } else {
        stacktraces_.emplace_back(stacktrace);
    }
    if (cur_arena_.size() < sizeof(AllocInfo)) {
        AllocArena();
    }
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto info = new (cur_arena_.data()) AllocInfo(cur_id_++, size, static_cast<uint32_t>(space), stacktrace_id);
    cur_arena_ = cur_arena_.SubSpan(sizeof(AllocInfo));
    cur_allocs_.insert({addr, info});
}

void DetailAllocTracker::TrackFree(void *addr)
{
    if (addr == nullptr) {
        return;
    }
    os::memory::LockHolder lock(mutex_);
    auto it = cur_allocs_.find(addr);
    ASSERT(it != cur_allocs_.end());
    AllocInfo *alloc = it->second;
    cur_allocs_.erase(it);
    if (cur_arena_.size() < sizeof(FreeInfo)) {
        AllocArena();
    }
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    new (cur_arena_.data()) FreeInfo(alloc->GetId());
    cur_arena_ = cur_arena_.SubSpan(sizeof(FreeInfo));
}

void DetailAllocTracker::AllocArena()
{
    if (cur_arena_.size() >= ENTRY_HDR_SIZE) {
        *reinterpret_cast<uint32_t *>(cur_arena_.data()) = 0;
    }
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    arenas_.emplace_back(new uint8_t[ARENA_SIZE]);
    cur_arena_ = Span<uint8_t>(arenas_.back().get(), arenas_.back().get() + ARENA_SIZE);
}

void DetailAllocTracker::Dump()
{
    LOG(ERROR, RUNTIME) << "DetailAllocTracker::Dump to " << GetDumpFilePath();
    std::ofstream out(GetDumpFilePath(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out) {
        LOG(ERROR, RUNTIME) << "DetailAllocTracker: Cannot open " << GetDumpFilePath()
                            << " for writing: " << strerror(errno) << "."
                            << "\nCheck if the directory has write permissions or"
                            << " selinux is disabled.";
    }
    Dump(out);
    LOG(ERROR, RUNTIME) << "DetailAllocTracker: dump file has been written";
}

void DetailAllocTracker::Dump(std::ostream &out)
{
    os::memory::LockHolder lock(mutex_);

    Write(0, out);  // nuber of items, will be updated later
    Write(0, out);  // number of stacktraces, will be updated later

    std::map<uint32_t, uint32_t> id_map;
    uint32_t num_stacks = WriteStacks(out, &id_map);

    // Write end marker to the current arena
    if (cur_arena_.size() >= ENTRY_HDR_SIZE) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        *reinterpret_cast<uint32_t *>(cur_arena_.data()) = 0;
    }
    uint32_t num_items = 0;
    for (auto &arena : arenas_) {
        uint8_t *ptr = arena.get();
        size_t pos = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        while (pos < ARENA_SIZE && *reinterpret_cast<uint32_t *>(ptr + pos) != 0) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            uint32_t tag = *reinterpret_cast<uint32_t *>(ptr + pos);
            if (tag == ALLOC_TAG) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto alloc = reinterpret_cast<AllocInfo *>(ptr + pos);
                Write(alloc->GetTag(), out);
                Write(alloc->GetId(), out);
                Write(alloc->GetSize(), out);
                Write(alloc->GetSpace(), out);
                Write(id_map[alloc->GetStacktraceId()], out);
                pos += sizeof(AllocInfo);
            } else if (tag == FREE_TAG) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto info = reinterpret_cast<FreeInfo *>(ptr + pos);
                Write(info->GetTag(), out);
                Write(info->GetAllocId(), out);
                pos += sizeof(FreeInfo);
            } else {
                UNREACHABLE();
            }
            ++num_items;
        }
    }

    out.seekp(0);
    Write(num_items, out);
    Write(num_stacks, out);
}

void DetailAllocTracker::DumpMemLeaks(std::ostream &out)
{
    static constexpr size_t MAX_ENTRIES_TO_REPORT = 10;

    os::memory::LockHolder lock(mutex_);
    size_t num = 0;
    for (auto &entry : cur_allocs_) {
        out << "Allocation of " << entry.second->GetSize() << " is allocated at\n";
        uint32_t stacktrace_id = entry.second->GetStacktraceId();
        auto it = stacktraces_.begin();
        std::advance(it, stacktrace_id);
        PrintStack(*it, out);
        if (++num > MAX_ENTRIES_TO_REPORT) {
            break;
        }
    }
}

uint32_t DetailAllocTracker::WriteStacks(std::ostream &out, std::map<uint32_t, uint32_t> *id_map)
{
    class Key {
    public:
        explicit Key(const Stacktrace *stacktrace) : stacktrace_(stacktrace), hash_(CalcHash(*stacktrace)) {}
        ~Key() = default;
        DEFAULT_COPY_SEMANTIC(Key);
        DEFAULT_MOVE_SEMANTIC(Key);

        bool operator==(const Key &k) const
        {
            return *stacktrace_ == *k.stacktrace_;
        }

        size_t GetHash() const
        {
            return hash_;
        }

    private:
        const Stacktrace *stacktrace_;
        size_t hash_;
    };

    struct KeyHash {
        size_t operator()(const Key &k) const
        {
            return k.GetHash();
        }
    };

    std::unordered_map<Key, uint32_t, KeyHash> alloc_stacks;
    uint32_t stacktrace_id = 0;
    uint32_t deduplicated_id = 0;
    for (Stacktrace &stacktrace : stacktraces_) {
        Key akey(&stacktrace);
        auto res = alloc_stacks.insert({akey, deduplicated_id});
        if (res.second) {
            std::stringstream str;
            PrintStack(stacktrace, str);
            Write(str.str(), out);
            id_map->insert({stacktrace_id, deduplicated_id});
            ++deduplicated_id;
        } else {
            uint32_t id = res.first->second;
            id_map->insert({stacktrace_id, id});
        }
        ++stacktrace_id;
    }
    return deduplicated_id;
}

}  // namespace panda
