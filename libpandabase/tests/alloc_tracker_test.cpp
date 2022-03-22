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

#include <thread>
#include <sstream>
#include <gtest/gtest.h>
#include "mem/alloc_tracker.h"

namespace panda {

struct Header {
    uint32_t num_items = 0;
    uint32_t num_stacktraces = 0;
};

struct AllocInfo {
    uint32_t tag = 0;
    uint32_t id = 0;
    uint32_t size = 0;
    uint32_t space = 0;
    uint32_t stacktrace_id = 0;
};

struct FreeInfo {
    uint32_t tag = 0;
    uint32_t alloc_id = 0;
};

static void SkipString(std::istream *in)
{
    uint32_t len = 0;
    in->read(reinterpret_cast<char *>(&len), sizeof(len));
    if (!(*in)) {
        return;
    }
    in->seekg(len, std::ios_base::cur);
}

TEST(DetailAllocTrackerTest, NoAllocs)
{
    DetailAllocTracker tracker;
    std::stringstream out;
    tracker.Dump(out);
    out.seekg(0);

    Header hdr;
    out.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(0, hdr.num_items);
    ASSERT_EQ(0, hdr.num_stacktraces);
}

TEST(DetailAllocTrackerTest, OneAlloc)
{
    DetailAllocTracker tracker;
    std::stringstream out;
    tracker.TrackAlloc(reinterpret_cast<void *>(0x15), 20, SpaceType::SPACE_TYPE_INTERNAL);
    tracker.Dump(out);
    out.seekg(0);

    Header hdr;
    out.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(1, hdr.num_items);
    ASSERT_EQ(1, hdr.num_stacktraces);

    // skip stacktrace
    SkipString(&out);
    ASSERT_FALSE(out.eof());
    AllocInfo info;
    out.read(reinterpret_cast<char *>(&info), sizeof(info));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(DetailAllocTracker::ALLOC_TAG, info.tag);
    ASSERT_EQ(0, info.id);
    ASSERT_EQ(20, info.size);
    ASSERT_EQ(static_cast<uint32_t>(SpaceType::SPACE_TYPE_INTERNAL), info.space);
    ASSERT_EQ(0, info.stacktrace_id);
}

TEST(DetailAllocTrackerTest, AllocAndFree)
{
    DetailAllocTracker tracker;
    std::stringstream out;
    tracker.TrackAlloc(reinterpret_cast<void *>(0x15), 20, SpaceType::SPACE_TYPE_INTERNAL);
    tracker.TrackFree(reinterpret_cast<void *>(0x15));
    tracker.Dump(out);
    out.seekg(0);

    Header hdr;
    out.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(2, hdr.num_items);
    ASSERT_EQ(1, hdr.num_stacktraces);

    // skip stacktrace
    SkipString(&out);
    ASSERT_FALSE(out.eof());
    AllocInfo alloc;
    FreeInfo free;
    out.read(reinterpret_cast<char *>(&alloc), sizeof(alloc));
    out.read(reinterpret_cast<char *>(&free), sizeof(free));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(DetailAllocTracker::ALLOC_TAG, alloc.tag);
    ASSERT_EQ(0, alloc.id);
    ASSERT_EQ(20, alloc.size);
    ASSERT_EQ(static_cast<uint32_t>(SpaceType::SPACE_TYPE_INTERNAL), alloc.space);
    ASSERT_EQ(0, alloc.stacktrace_id);
    ASSERT_EQ(DetailAllocTracker::FREE_TAG, free.tag);
    ASSERT_EQ(0, free.alloc_id);
}

TEST(DetailAllocTrackerTest, MultithreadedAlloc)
{
    static constexpr size_t NUM_THREADS = 10;
    static constexpr size_t NUM_ITERS = 100;

    DetailAllocTracker tracker;
    std::vector<std::thread> threads;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(
            [&tracker](size_t thread_num) {
                for (size_t iter = 0; iter < NUM_ITERS; ++iter) {
                    auto addr = reinterpret_cast<void *>(thread_num * NUM_THREADS + iter + 1);
                    tracker.TrackAlloc(addr, 10, SpaceType::SPACE_TYPE_INTERNAL);
                }
            },
            i);
    }

    for (auto &thread : threads) {
        thread.join();
    }

    std::stringstream out;
    tracker.Dump(out);
    out.seekg(0);

    Header hdr;
    out.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    ASSERT_FALSE(out.eof());
    ASSERT_EQ(NUM_THREADS * NUM_ITERS, hdr.num_items);
    ASSERT_EQ(1, hdr.num_stacktraces);
}

}  // namespace panda
