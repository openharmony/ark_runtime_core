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

#include "include/tooling/debug_inf.h"

//
// Debuge interface for native tools(simpleperf, libunwind).
//

namespace panda::tooling {
#ifdef __cplusplus
extern "C" {
#endif

enum CodeAction {
    CODE_NOACTION = 0,
    CODE_ADDED,
    CODE_REMOVE,
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct PCodeItem {
    std::atomic<PCodeItem *> next_;
    PCodeItem *prev_;
    const uint8_t *code_base_;
    uint64_t code_size_;
    uint64_t timestamp_;
};

struct PCodeMetaInfo {
    uint32_t version_ = 1;
    uint32_t action_ = CODE_NOACTION;
    PCodeItem *relevent_item_ = nullptr;
    std::atomic<PCodeItem *> head_ {nullptr};

    // Panda-specific fields
    static constexpr size_t MAGIC_SIZE = 8;
    std::array<uint8_t, MAGIC_SIZE> magic_ {'P', 'a', 'n', 'd', 'a', 'r', 't', '1'};
    uint32_t flags_ = 0;
    uint32_t size_meta_info_ = sizeof(PCodeMetaInfo);
    uint32_t size_codeitem_ = sizeof(PCodeItem);
    std::atomic_uint32_t update_lock_ {0};
    uint64_t timestamp_ = 1;
};

// simpleperf currently use g_jitDebugDescriptor and g_dexDebugDescriptor
// to find the jit code item and dexfiles.
// for using the variable interface, we doesn't change the name in panda
// NOLINTNEXTLINE(readability-identifier-naming, fuchsia-statically-constructed-objects)
PCodeMetaInfo g_jitDebugDescriptor;
// NOLINTNEXTLINE(readability-identifier-naming, fuchsia-statically-constructed-objects)
PCodeMetaInfo g_dexDebugDescriptor;

#ifdef __cplusplus
}  // extern "C"
#endif

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
std::map<const std::string, PCodeItem *> DebugInf::aex_item_map;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
panda::os::memory::Mutex DebugInf::jit_item_lock;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
panda::os::memory::Mutex DebugInf::aex_item_lock;

void DebugInf::AddCodeMetaInfo(const panda_file::File *file)
{
    panda::os::memory::LockHolder lock(aex_item_lock);
    ASSERT(file != nullptr);
    auto it = aex_item_map.find(file->GetFilename());
    if (it != aex_item_map.end()) {
        return;
    }

    PCodeItem *item = AddCodeMetaInfoImpl(&g_dexDebugDescriptor, {file->GetBase(), file->GetHeader()->file_size});
    aex_item_map.emplace(file->GetFilename(), item);
}

void DebugInf::DelCodeMetaInfo(const panda_file::File *file)
{
    panda::os::memory::LockHolder lock(aex_item_lock);
    ASSERT(file != nullptr);
    auto it = aex_item_map.find(file->GetFilename());
    if (it == aex_item_map.end()) {
        return;
    }

    DelCodeMetaInfoImpl(&g_dexDebugDescriptor, file);
}

void DebugInf::Lock(PCodeMetaInfo *mi)
{
    mi->update_lock_.fetch_add(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
}

void DebugInf::UnLock(PCodeMetaInfo *mi)
{
    std::atomic_thread_fence(std::memory_order_release);
    mi->update_lock_.fetch_add(1, std::memory_order_relaxed);
}

PCodeItem *DebugInf::AddCodeMetaInfoImpl(PCodeMetaInfo *metaInfo, [[maybe_unused]] Span<const uint8_t> inss)
{
    uint64_t timestamp = std::max(metaInfo->timestamp_ + 1, panda::time::GetCurrentTimeInNanos());

    auto *head = metaInfo->head_.load(std::memory_order_relaxed);

    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto *codeItem = new PCodeItem;
    codeItem->code_base_ = inss.begin();
    codeItem->code_size_ = inss.Size();
    codeItem->prev_ = nullptr;
    codeItem->next_.store(head, std::memory_order_relaxed);
    codeItem->timestamp_ = timestamp;

    // lock
    Lock(metaInfo);
    if (head != nullptr) {
        head->prev_ = codeItem;
    }

    metaInfo->head_.store(codeItem, std::memory_order_relaxed);
    metaInfo->relevent_item_ = codeItem;
    metaInfo->action_ = CODE_ADDED;

    // unlock
    UnLock(metaInfo);

    return codeItem;
}

void DebugInf::DelCodeMetaInfoImpl(PCodeMetaInfo *metaInfo, const panda_file::File *file)
{
    PCodeItem *codeItem = aex_item_map[file->GetFilename()];
    ASSERT(codeItem != nullptr);
    uint64_t timestamp = std::max(metaInfo->timestamp_ + 1, panda::time::GetCurrentTimeInNanos());
    // lock
    Lock(metaInfo);

    auto next = codeItem->next_.load(std::memory_order_relaxed);
    if (codeItem->prev_ != nullptr) {
        codeItem->prev_->next_.store(next, std::memory_order_relaxed);
    } else {
        metaInfo->head_.store(next, std::memory_order_relaxed);
    }

    if (next != nullptr) {
        next->prev_ = codeItem->prev_;
    }

    metaInfo->relevent_item_ = codeItem;
    metaInfo->action_ = CODE_REMOVE;
    metaInfo->timestamp_ = timestamp;

    // unlock
    UnLock(metaInfo);
}
}  // namespace panda::tooling
