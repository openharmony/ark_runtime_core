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

#ifndef PANDA_VERIFICATION_UTIL_ADDR_MAP_H_
#define PANDA_VERIFICATION_UTIL_ADDR_MAP_H_

#include "macros.h"
#include "range.h"
#include "bit_vector.h"

#include <cstdint>

namespace panda::verifier {
class AddrMap {
public:
    AddrMap() = delete;
    AddrMap(const void *start_ptr, const void *end_ptr)
        : AddrMap(reinterpret_cast<uintptr_t>(start_ptr), reinterpret_cast<uintptr_t>(end_ptr))
    {
    }

    AddrMap(const void *start_ptr, size_t size)
        : AddrMap(reinterpret_cast<uintptr_t>(start_ptr), reinterpret_cast<uintptr_t>(start_ptr) + size - 1)
    {
    }

    ~AddrMap() = default;

    bool IsInAddressSpace(const void *ptr) const
    {
        return IsInAddressSpace(reinterpret_cast<uintptr_t>(ptr));
    }

    template <typename PtrType>
    PtrType AddrStart() const
    {
        return reinterpret_cast<PtrType>(AddrRange_.Start());
    }

    template <typename PtrType>
    PtrType AddrEnd() const
    {
        return reinterpret_cast<PtrType>(AddrRange_.End());
    }

    bool Mark(const void *addr_ptr)
    {
        return Mark(addr_ptr, addr_ptr);
    }

    bool Mark(const void *addr_start_ptr, const void *addr_end_ptr)
    {
        return Mark(reinterpret_cast<uintptr_t>(addr_start_ptr), reinterpret_cast<uintptr_t>(addr_end_ptr));
    }

    void Clear()
    {
        Clear(AddrRange_.Start(), AddrRange_.End());
    }

    bool Clear(const void *addr_ptr)
    {
        return Clear(addr_ptr, addr_ptr);
    }

    bool Clear(const void *addr_start_ptr, const void *addr_end_ptr)
    {
        return Clear(reinterpret_cast<uintptr_t>(addr_start_ptr), reinterpret_cast<uintptr_t>(addr_end_ptr));
    }

    bool HasMark(const void *addr_ptr) const
    {
        return HasMarks(addr_ptr, addr_ptr);
    }

    bool HasMarks(const void *addr_start_ptr, const void *addr_end_ptr) const
    {
        return HasMarks(reinterpret_cast<uintptr_t>(addr_start_ptr), reinterpret_cast<uintptr_t>(addr_end_ptr));
    }

    bool HasCommonMarks(const AddrMap &rhs) const
    {
        ASSERT(AddrRange_ == rhs.AddrRange_);
        return BitVector::lazy_and_then_indices_of<true>(BitMap_, rhs.BitMap_)().IsValid();
    }

    template <typename PtrType>
    bool GetFirstCommonMark(const AddrMap &rhs, PtrType *ptr) const
    {
        ASSERT(AddrRange_ == rhs.AddrRange_);
        Index<size_t> idx = BitVector::lazy_and_then_indices_of<true>(BitMap_, rhs.BitMap_)();
        if (idx.IsValid()) {
            size_t offset = idx;
            *ptr = reinterpret_cast<PtrType>(AddrRange_.IndexOf(offset));
            return true;
        }
        return false;
    }

    // and refactor it to work with words and ctlz like intrinsics
    template <typename PtrType, typename Callback>
    void EnumerateMarkedBlocks(Callback cb) const
    {
        PtrType start = nullptr;
        PtrType end = nullptr;
        for (const auto addr : AddrRange_) {
            uintptr_t bit_offset = AddrRange_.OffsetOf(addr);
            if (start == nullptr) {
                if (BitMap_[bit_offset]) {
                    start = reinterpret_cast<PtrType>(addr);
                }
            } else {
                if (!BitMap_[bit_offset]) {
                    end = reinterpret_cast<PtrType>(addr - 1);
                    if (!cb(start, end)) {
                        return;
                    }
                    start = nullptr;
                }
            }
        }
        if (start != nullptr) {
            end = reinterpret_cast<PtrType>(AddrRange_.End());
            cb(start, end);
        }
    }

    void InvertMarks()
    {
        BitMap_.invert();
    }

    template <typename PtrType, typename Handler>
    void EnumerateMarksInScope(const void *addr_start_ptr, const void *addr_end_ptr, Handler handler) const
    {
        EnumerateMarksInScope<PtrType>(reinterpret_cast<uintptr_t>(addr_start_ptr),
                                       reinterpret_cast<uintptr_t>(addr_end_ptr), std::move(handler));
    }

private:
    AddrMap(uintptr_t addr_from, uintptr_t addr_to) : AddrRange_ {addr_from, addr_to}, BitMap_ {AddrRange_.Length()} {}

    bool IsInAddressSpace(uintptr_t addr) const
    {
        return AddrRange_.Contains(addr);
    }

    bool Mark(uintptr_t addr_from, uintptr_t addr_to)
    {
        if (!AddrRange_.Contains(addr_from) || !AddrRange_.Contains(addr_to)) {
            return false;
        }
        ASSERT(addr_from <= addr_to);
        BitMap_.Set(AddrRange_.OffsetOf(addr_from), AddrRange_.OffsetOf(addr_to));
        return true;
    }

    bool Clear(uintptr_t addr_from, uintptr_t addr_to)
    {
        if (!AddrRange_.Contains(addr_from) || !AddrRange_.Contains(addr_to)) {
            return false;
        }
        ASSERT(addr_from <= addr_to);
        BitMap_.Clr(AddrRange_.OffsetOf(addr_from), AddrRange_.OffsetOf(addr_to));
        return true;
    }

    bool HasMarks(uintptr_t addr_from, uintptr_t addr_to) const
    {
        if (!AddrRange_.Contains(addr_from) || !AddrRange_.Contains(addr_to)) {
            return false;
        }
        ASSERT(addr_from <= addr_to);
        Index<size_t> first_mark_idx =
            BitMap_.LazyIndicesOf<1>(AddrRange_.OffsetOf(addr_from), AddrRange_.OffsetOf(addr_to))();
        return first_mark_idx.IsValid();
    }

    template <typename PtrType, typename Handler>
    void EnumerateMarksInScope(uintptr_t addr_from, uintptr_t addr_to, Handler handler) const
    {
        addr_from = AddrRange_.PutInBounds(addr_from);
        addr_to = AddrRange_.PutInBounds(addr_to);
        ASSERT(addr_from <= addr_to);
        auto mark_idxs = BitMap_.LazyIndicesOf<1>(AddrRange_.OffsetOf(addr_from), AddrRange_.OffsetOf(addr_to));
        Index<size_t> idx = mark_idxs();
        while (idx.IsValid()) {
            auto addr = AddrRange_.IndexOf(idx);
            PtrType ptr = reinterpret_cast<PtrType>(addr);
            if (!handler(ptr)) {
                return;
            }
            idx = mark_idxs();
        }
    }

private:
    Range<uintptr_t> AddrRange_;
    BitVector BitMap_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_ADDR_MAP_H_
