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

#ifndef PANDA_RUNTIME_MEM_GC_BITMAP_H_
#define PANDA_RUNTIME_MEM_GC_BITMAP_H_

// clash with mingw
#ifndef PANDA_TARGET_WINDOWS
#include <securec.h>
#endif
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "libpandabase/macros.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/math_helpers.h"
#include "libpandabase/utils/span.h"

namespace panda::mem {

/**
 * Abstract base class. Constructor/destructor are protected. No virtual function to avoid dynamic polymorphism.
 */
class Bitmap {
public:
    using BitmapWordType = uintptr_t;

    size_t Size() const
    {
        return bitsize_;
    }

    void ClearAllBits()
    {
#ifndef PANDA_TARGET_WINDOWS
        (void)memset_s(bitmap_.Data(), bitmap_.SizeBytes(), 0, bitmap_.SizeBytes());
#endif
    }

    Span<BitmapWordType> GetBitMap()
    {
        return bitmap_;
    }

    static const size_t BITSPERBYTE = 8;
    static const size_t BITSPERWORD = BITSPERBYTE * sizeof(BitmapWordType);
    static constexpr size_t LOG_BITSPERBYTE = panda::helpers::math::GetIntLog2(static_cast<uint64_t>(BITSPERBYTE));
    static constexpr size_t LOG_BITSPERWORD = panda::helpers::math::GetIntLog2(static_cast<uint64_t>(BITSPERWORD));

protected:
    /**
     * \brief Set the bit indexed by bit_offset.
     * @param bit_offset - index of the bit to set.
     */
    void SetBit(size_t bit_offset)
    {
        CheckBitOffset(bit_offset);
        bitmap_[GetWordIdx(bit_offset)] |= GetBitMask(bit_offset);
    }

    /**
     * \brief Clear the bit indexed by bit_offset.
     * @param bit_offset - index of the bit to clear.
     */
    void ClearBit(size_t bit_offset)
    {
        CheckBitOffset(bit_offset);
        bitmap_[GetWordIdx(bit_offset)] &= ~GetBitMask(bit_offset);
    }

    /**
     * \brief Test the bit indexed by bit_offset.
     * @param bit_offset - index of the bit to test.
     * @return Returns value of indexed bit.
     */
    bool TestBit(size_t bit_offset) const
    {
        CheckBitOffset(bit_offset);
        return (bitmap_[GetWordIdx(bit_offset)] & GetBitMask(bit_offset)) != 0;
    }

    /**
     * \brief Atomically set bit indexed by bit_offset. If the bit is not set, set it atomically. Otherwise, do nothing.
     * @param bit_offset - index of the bit to set.
     * @return Returns old value of the bit.
     */
    bool AtomicTestAndSetBit(size_t bit_offset);

    /**
     * \brief Atomically clear bit corresponding to addr. If the bit is set, clear it atomically. Otherwise, do nothing.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     * @return Returns old value of the bit.
     */
    bool AtomicTestAndClearBit(size_t bit_offset);

    /**
     * \brief Atomically test bit corresponding to addr.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     * @return Returns the value of the bit.
     */
    bool AtomicTestBit(size_t bit_offset);

    /**
     * \brief Iterate over marked bits of bitmap sequentially.
     * Finish iteration if the visitor returns false.
     * @tparam VisitorType
     * @param visitor - function pointer or functor.
     */
    template <typename VisitorType>
    void IterateOverSetBits(const VisitorType &visitor)
    {
        IterateOverSetBitsInRange(0, Size(), visitor);
    }

    /**
     * \brief Iterate over all bits of bitmap sequentially.
     * @tparam VisitorType
     * @param visitor - function pointer or functor.
     */
    template <typename VisitorType>
    void IterateOverBits(const VisitorType &visitor)
    {
        IterateOverBitsInRange(0, Size(), visitor);
    }

    /**
     * \brief Iterate over marked bits in range [begin, end) sequentially.
     * Finish iteration if the visitor returns false.
     * @tparam VisitorType
     * @param begin - beginning index of the range, inclusive.
     * @param end - end index of the range, exclusive.
     * @param visitor - function pointer or functor.
     */
    template <typename VisitorType>
    void IterateOverSetBitsInRange(size_t begin, size_t end, const VisitorType &visitor)
    {
        CheckBitRange(begin, end);
        if (UNLIKELY(begin == end)) {
            return;
        }

        // first word, clear bits before begin
        auto bitmap_word = bitmap_[GetWordIdx(begin)];
        auto offset_within_word = GetBitIdxWithinWord(begin);
        bitmap_word &= GetRangeBitMask(offset_within_word, BITSPERWORD);
        auto offset_word_begin = GetWordIdx(begin) * BITSPERWORD;
        const bool RIGHT_ALIGNED = (GetBitIdxWithinWord(end) == 0);
        const auto OFFSET_LAST_WORD_BEGIN = GetWordIdx(end) * BITSPERWORD;

        do {
            if (offset_word_begin == OFFSET_LAST_WORD_BEGIN && !RIGHT_ALIGNED) {
                // last partial word, clear bits after right boundary
                auto mask = GetRangeBitMask(0, GetBitIdxWithinWord(end));
                bitmap_word &= mask;
            }
            // loop over bits of bitmap_word
            while (offset_within_word < BITSPERWORD) {
                if (bitmap_word == 0) {
                    break;
                }
                offset_within_word = static_cast<size_t>(Ctz(bitmap_word));
                if (!visitor(offset_word_begin + offset_within_word)) {
                    return;
                }
                bitmap_word &= ~GetBitMask(offset_within_word);
            }

            offset_word_begin += BITSPERWORD;
            if (offset_word_begin < end) {
                bitmap_word = bitmap_[GetWordIdx(offset_word_begin)];
                offset_within_word = 0;
            }
        } while (offset_word_begin < end);
    }

    /**
     * \brief Iterate over all bits in range [begin, end) sequentially.
     * @tparam VisitorType
     * @param begin - beginning index of the range, inclusive.
     * @param end - end index of the range, exclusive.
     * @param visitor - function pointer or functor.
     */
    template <typename VisitorType>
    void IterateOverBitsInRange(size_t begin, size_t end, const VisitorType &visitor)
    {
        CheckBitRange(begin, end);
        for (size_t i = begin; i < end; ++i) {
            visitor(i);
        }
    }

    /**
     * \brief Clear all bits in range [begin, end).
     * @param begin - beginning index of the range, inclusive.
     * @param end - end index of the range, exclusive.
     */
    void ClearBitsInRange(size_t begin, size_t end);

    /**
     * \brief Set all bits in range [begin, end). [begin, end) must be within a BitmapWord.
     * @param begin - beginning index of the range, inclusive.
     * @param end - end index of the range, exclusive.
     */
    void SetRangeWithinWord(size_t begin, size_t end)
    {
        if (LIKELY(begin != end)) {
            ModifyRangeWithinWord<true>(begin, end);
        }
    }

    /**
     * \brief Clear all bits in range [begin, end). [begin, end) must be within a BitmapWord.
     * @param begin - beginning index of the range, inclusive.
     * @param end - end index of the range, exclusive.
     */
    void ClearRangeWithinWord(size_t begin, size_t end)
    {
        if (LIKELY(begin != end)) {
            ModifyRangeWithinWord<false>(begin, end);
        }
    }

    /**
     * \brief Set all BitmapWords in index range [begin, end).
     * @param begin - beginning BitmapWord index of the range, inclusive.
     * @param end - end BitmapWord index of the range, exclusive.
     */
    void SetWords([[maybe_unused]] size_t word_begin, [[maybe_unused]] size_t word_end)
    {
        ASSERT(word_begin <= word_end);
#ifndef PANDA_TARGET_WINDOWS
        if (UNLIKELY(word_begin == word_end)) {
            return;
        }
        (void)memset_s(&bitmap_[word_begin], (word_end - word_begin) * sizeof(BitmapWordType),
                       ~static_cast<unsigned char>(0), (word_end - word_begin) * sizeof(BitmapWordType));
#endif
    }

    /**
     * \brief Clear all BitmapWords in index range [begin, end).
     * @param begin - beginning BitmapWord index of the range, inclusive.
     * @param end - end BitmapWord index of the range, exclusive.
     */
    void ClearWords([[maybe_unused]] size_t word_begin, [[maybe_unused]] size_t word_end)
    {
        ASSERT(word_begin <= word_end);
#ifndef PANDA_TARGET_WINDOWS
        if (UNLIKELY(word_begin == word_end)) {
            return;
        }
        (void)memset_s(&bitmap_[word_begin], (word_end - word_begin) * sizeof(BitmapWordType),
                       static_cast<unsigned char>(0), (word_end - word_begin) * sizeof(BitmapWordType));
#endif
    }

    explicit Bitmap(BitmapWordType *bitmap, size_t bitsize)
        : bitmap_(bitmap, (AlignUp(bitsize, BITSPERWORD) >> LOG_BITSPERWORD)), bitsize_(bitsize)
    {
    }
    ~Bitmap() = default;
    // do we need special copy/move constructor?
    NO_COPY_SEMANTIC(Bitmap);
    NO_MOVE_SEMANTIC(Bitmap);

private:
    Span<BitmapWordType> bitmap_;
    size_t bitsize_ = 0;

    /**
     * \brief Compute word index from bit index.
     * @param bit_offset - bit index.
     * @return Returns BitmapWord Index of bit_offset.
     */
    static size_t GetWordIdx(size_t bit_offset)
    {
        return bit_offset >> LOG_BITSPERWORD;
    }

    /**
     * \brief Compute bit index within a BitmapWord from bit index.
     * @param bit_offset - bit index.
     * @return Returns bit index within a BitmapWord.
     */
    size_t GetBitIdxWithinWord(size_t bit_offset) const
    {
        CheckBitOffset(bit_offset);
        constexpr auto BIT_INDEX_MASK = static_cast<size_t>((1UL << LOG_BITSPERWORD) - 1);
        return bit_offset & BIT_INDEX_MASK;
    }

    /**
     * \brief Compute bit mask from bit index.
     * @param bit_offset - bit index.
     * @return Returns bit mask of bit_offset.
     */
    BitmapWordType GetBitMask(size_t bit_offset) const
    {
        return 1UL << GetBitIdxWithinWord(bit_offset);
    }

    /**
     * \brief Compute bit mask of range [begin_within_word, end_within_word).
     * @param begin_within_word - beginning index within word, in range [0, BITSPERWORD).
     * @param end_within_word - end index within word, in range [0, BITSPERWORD]. Make sure end_within_word is
     * BITSPERWORD(instead of 0) if you want to cover from certain bit to last. [0, 0) is the only valid case when
     * end_within_word is 0.
     * @return Returns bit mask.
     */
    BitmapWordType GetRangeBitMask(size_t begin_within_word, size_t end_within_word) const
    {
        ASSERT(begin_within_word < BITSPERWORD);
        ASSERT(end_within_word <= BITSPERWORD);
        ASSERT(begin_within_word <= end_within_word);
        auto end_mask =
            (end_within_word == BITSPERWORD) ? ~static_cast<BitmapWordType>(0) : GetBitMask(end_within_word) - 1;
        return end_mask - (GetBitMask(begin_within_word) - 1);
    }

    /**
     * \brief Check if bit_offset is valid.
     */
    void CheckBitOffset([[maybe_unused]] size_t bit_offset) const
    {
        ASSERT(bit_offset <= Size());
    }

    /**
     * \brief According to SET, set or clear range [begin, end) within a BitmapWord.
     * @param begin - beginning global bit index.
     * @param end - end global bit index.
     */
    template <bool SET>
    ALWAYS_INLINE void ModifyRangeWithinWord(size_t begin, size_t end)
    {
        CheckBitRange(begin, end);

        if (UNLIKELY(begin == end)) {
            return;
        }

        BitmapWordType mask;
        if (end % BITSPERWORD == 0) {
            ASSERT(GetWordIdx(end) - GetWordIdx(begin) == 1);
            mask = GetRangeBitMask(GetBitIdxWithinWord(begin), BITSPERWORD);
        } else {
            ASSERT(GetWordIdx(end) == GetWordIdx(begin));
            mask = GetRangeBitMask(GetBitIdxWithinWord(begin), GetBitIdxWithinWord(end));
        }

        if (SET) {
            bitmap_[GetWordIdx(begin)] |= mask;
        } else {
            bitmap_[GetWordIdx(begin)] &= ~mask;
        }
    }

    /**
     * \brief Check if bit range [begin, end) is valid.
     */
    void CheckBitRange([[maybe_unused]] size_t begin, [[maybe_unused]] size_t end) const
    {
        ASSERT(begin < Size());
        ASSERT(end <= Size());
        ASSERT(begin <= end);
    }
};

/**
 * Memory bitmap, binding a continuous range of memory to a bitmap.
 * One bit represents BYTESPERCHUNK bytes of memory.
 */
template <size_t BYTESPERCHUNK = 1, typename pointer_type = object_pointer_type>
class MemBitmap : public Bitmap {
public:
    explicit MemBitmap(void *mem_addr, size_t heap_size, void *bitmap_addr)
        : Bitmap(static_cast<BitmapWordType *>(bitmap_addr), heap_size / BYTESPERCHUNK),
          begin_addr_(ToPointerType(mem_addr)),
          end_addr_(begin_addr_ + heap_size)
    {
    }
    NO_COPY_SEMANTIC(MemBitmap);
    NO_MOVE_SEMANTIC(MemBitmap);

    /**
     * \brief Reinitialize the MemBitmap for new memory range.
     * The size of range will be the same as the initial
     * because we reuse the same bitmap storage.
     * @param mem_addr - start addr of the new range.
     */
    void ReInitializeMemoryRange(void *mem_addr)
    {
        begin_addr_ = ToPointerType(mem_addr);
        end_addr_ = begin_addr_ + MemSizeInBytes();
        Bitmap::ClearAllBits();
    }

    inline static constexpr size_t GetBitMapSizeInByte(size_t heap_size)
    {
        ASSERT(heap_size % BYTESPERCHUNK == 0);
        size_t bit_size = heap_size / BYTESPERCHUNK;
        return (AlignUp(bit_size, BITSPERWORD) >> Bitmap::LOG_BITSPERWORD) * sizeof(BitmapWordType);
    }

    ~MemBitmap() = default;

    size_t MemSizeInBytes() const
    {
        return Size() * BYTESPERCHUNK;
    }

    inline std::pair<uintptr_t, uintptr_t> GetHeapRange()
    {
        return {begin_addr_, end_addr_};
    }

    /**
     * \brief Set bit corresponding to addr.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     */
    void Set(void *addr)
    {
        CheckAddrValidity(addr);
        SetBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Clear bit corresponding to addr.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     */
    void Clear(void *addr)
    {
        CheckAddrValidity(addr);
        ClearBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Clear bits corresponding to addr range [begin, end).
     */
    ALWAYS_INLINE void ClearRange(void *begin, void *end)
    {
        CheckHalfClosedHalfOpenAddressRange(begin, end);
        ClearBitsInRange(AddrToBitOffset(ToPointerType(begin)), EndAddrToBitOffset(ToPointerType(end)));
    }

    /**
     * \brief Test bit corresponding to addr.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     */
    bool Test(const void *addr) const
    {
        CheckAddrValidity(addr);
        return TestBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Test bit corresponding to addr if addr is valid.
     * @return value of indexed bit if addr is valid. If addr is invalid then false
     */
    bool TestIfAddrValid(const void *addr) const
    {
        if (IsAddrValid(addr)) {
            return TestBit(AddrToBitOffset(ToPointerType(addr)));
        }
        return false;
    }

    /**
     * \brief Atomically set bit corresponding to addr. If the bit is not set, set it atomically. Otherwise, do nothing.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     * @return Returns old value of the bit.
     */
    bool AtomicTestAndSet(void *addr)
    {
        CheckAddrValidity(addr);
        return AtomicTestAndSetBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Atomically clear bit corresponding to addr. If the bit is set, clear it atomically. Otherwise, do nothing.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     * @return Returns old value of the bit.
     */
    bool AtomicTestAndClear(void *addr)
    {
        CheckAddrValidity(addr);
        return AtomicTestAndClearBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Atomically test bit corresponding to addr.
     * @param addr - addr must be aligned to BYTESPERCHUNK.
     * @return Returns the value of the bit.
     */
    bool AtomicTest(void *addr)
    {
        CheckAddrValidity(addr);
        return AtomicTestBit(AddrToBitOffset(ToPointerType(addr)));
    }

    /**
     * \brief Find first marked chunk.
     */
    void *FindFirstMarkedChunks()
    {
        void *first_marked = nullptr;
        IterateOverSetBits([&first_marked, this](size_t bit_offset) {
            first_marked = BitOffsetToAddr(bit_offset);
            return false;
        });
        return first_marked;
    }

    /**
     * \brief Iterate over marked chunks of memory sequentially.
     */
    template <typename MemVisitor>
    void IterateOverMarkedChunks(const MemVisitor &visitor)
    {
        IterateOverSetBits([&visitor, this](size_t bit_offset) {
            visitor(BitOffsetToAddr(bit_offset));
            return true;
        });
    }

    /**
     * \brief Iterate over all chunks of memory sequentially.
     */
    template <typename MemVisitor>
    void IterateOverChunks(const MemVisitor &visitor)
    {
        IterateOverBits([&visitor, this](size_t bit_offset) { visitor(BitOffsetToAddr(bit_offset)); });
    }

    /**
     * \brief Iterate over marked chunks of memory in range [begin, end) sequentially.
     */
    template <typename MemVisitor>
    void IterateOverMarkedChunkInRange(void *begin, void *end, const MemVisitor &visitor)
    {
        CheckHalfClosedHalfOpenAddressRange(begin, end);
        IterateOverSetBitsInRange(AddrToBitOffset(ToPointerType(begin)), EndAddrToBitOffset(ToPointerType(end)),
                                  [&visitor, this](size_t bit_offset) {
                                      visitor(BitOffsetToAddr(bit_offset));
                                      return true;
                                  });
    }

    /**
     * \brief Iterate over all chunks of memory in range [begin, end) sequentially.
     */
    template <typename MemVisitor>
    void IterateOverChunkInRange(void *begin, void *end, const MemVisitor &visitor)
    {
        CheckHalfClosedHalfOpenAddressRange(begin, end);
        IterateOverBitsInRange(AddrToBitOffset(ToPointerType(begin)), EndAddrToBitOffset(ToPointerType(end)),
                               [&visitor, this](size_t bit_offset) { visitor(BitOffsetToAddr(bit_offset)); });
    }

    bool IsAddrInRange(const void *addr) const
    {
        return addr >= ToVoidPtr(begin_addr_) && addr < ToVoidPtr(end_addr_);
    }

    template <class T>
    static constexpr pointer_type ToPointerType(T *val)
    {
        return static_cast<pointer_type>(ToUintPtr(val));
    }

private:
    /**
     * \brief Compute bit offset from addr.
     */
    size_t AddrToBitOffset(pointer_type addr) const
    {
        return (addr - begin_addr_) / BYTESPERCHUNK;
    }

    size_t EndAddrToBitOffset(pointer_type addr) const
    {
        return (AlignUp(addr, BYTESPERCHUNK) - begin_addr_) / BYTESPERCHUNK;
    }

    /**
     * \brief Compute address from bit offset.
     */
    void *BitOffsetToAddr(size_t bit_offset) const
    {
        return ToVoidPtr(begin_addr_ + bit_offset * BYTESPERCHUNK);
    }

    /**
     * \brief Check if addr is valid.
     */
    void CheckAddrValidity([[maybe_unused]] const void *addr) const
    {
        ASSERT(IsAddrInRange(addr));
        ASSERT((ToPointerType(addr) - begin_addr_) % BYTESPERCHUNK == 0);
    }

    /**
     * \brief Check if addr is valid with returned value.
     * @return true if addr is valid
     */
    bool IsAddrValid(const void *addr) const
    {
        return IsAddrInRange(addr) && (ToPointerType(addr) - begin_addr_) % BYTESPERCHUNK == 0;
    }

    /**
     * \brief Check if [begin, end) is a valid address range.
     */
    void CheckHalfClosedHalfOpenAddressRange([[maybe_unused]] void *begin, [[maybe_unused]] void *end) const
    {
        CheckAddrValidity(begin);
        ASSERT(ToPointerType(end) >= begin_addr_);
        ASSERT(ToPointerType(end) <= end_addr_);
        ASSERT(ToPointerType(begin) <= ToPointerType(end));
    }

    pointer_type begin_addr_ {0};
    pointer_type end_addr_ {0};
};

using MarkBitmap = MemBitmap<DEFAULT_ALIGNMENT_IN_BYTES>;

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_BITMAP_H_
