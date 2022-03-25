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

#ifndef PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_H_

#include <functional>
#include <memory>
#include <vector>

#include "libpandabase/macros.h"
#include "libpandabase/mem/arena.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/mem_range.h"
#include "runtime/mem/runslots_allocator.h"
#include "runtime/mem/gc/bitmap.h"

namespace panda {
class ObjectHeader;
}  // namespace panda

namespace panda::mem {

enum PygoteSpaceState {
    STATE_PYGOTE_INIT,     // before pygote fork, used for small non-movable objects
    STATE_PYGOTE_FORKING,  // at first pygote fork, allocate for copied objects
    STATE_PYGOTE_FORKED,   // after fork, can't allocate/free objects in it
};

using BitmapList = std::vector<MarkBitmap *>;
template <typename AllocConfigT>
class PygoteSpaceAllocator final {
public:
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(PygoteSpaceAllocator);
    NO_COPY_SEMANTIC(PygoteSpaceAllocator);
    explicit PygoteSpaceAllocator(MemStatsType *mem_stats);
    ~PygoteSpaceAllocator();
    void *Alloc(size_t size, Alignment alignment = panda::DEFAULT_ALIGNMENT);

    void Free(void *mem);

    void SetState(PygoteSpaceState new_state);

    PygoteSpaceState GetState() const
    {
        return state_;
    }

    static constexpr size_t GetMaxSize()
    {
        return RunSlotsAllocator<AllocConfigT>::GetMaxSize();
    }

    bool CanAllocNonMovable(size_t size, Alignment align)
    {
        return state_ == STATE_PYGOTE_INIT && AlignUp(size, GetAlignmentInBytes(align)) <= GetMaxSize();
    }

    bool ContainObject(const ObjectHeader *object);

    bool IsLive(const ObjectHeader *object);

    void ClearLiveBitmaps();

    BitmapList &GetLiveBitmaps()
    {
        // return live_bitmaps as mark bitmap for gc,
        // gc will update it at end of gc process
        return live_bitmaps_;
    }

    template <typename Visitor>
    void IterateOverObjectsInRange(const Visitor &visitor, void *start, void *end);

    void IterateOverObjects(const ObjectVisitor &object_visitor);

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor);

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor);

    void Collect(const GCObjectVisitor &gc_visitor);

private:
    void CreateLiveBitmap(void *heap_begin, size_t heap_size);
    RunSlotsAllocator<AllocConfigT> runslots_alloc_;
    Arena *arena_ = nullptr;
    SpaceType space_type_ = SpaceType::SPACE_TYPE_OBJECT;
    PygoteSpaceState state_ = STATE_PYGOTE_INIT;
    BitmapList live_bitmaps_;
    MemStatsType *mem_stats_;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_H_
