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

#ifndef PANDA_LIBPANDABASE_UTILS_BIT_MEMORY_REGION_H_
#define PANDA_LIBPANDABASE_UTILS_BIT_MEMORY_REGION_H_

#include "globals.h"
#include "mem/mem.h"
#include "utils/bit_utils.h"
#include "utils/span.h"
#include "utils/type_helpers.h"

namespace panda {

template <typename Base = uint8_t>
class BitMemoryRegion {
public:
    using ValueType = std::conditional_t<std::is_const_v<Base>, const uint8_t, uint8_t>;

    BitMemoryRegion() = default;
    BitMemoryRegion(Base *data, size_t size) : BitMemoryRegion(data, 0, size) {}
    BitMemoryRegion(Base *data, size_t start, size_t size)
        : data_(reinterpret_cast<ValueType *>(reinterpret_cast<uintptr_t>(
              AlignDown(reinterpret_cast<uintptr_t>(data) + (start >> BITS_PER_BYTE_LOG2), alignof(uint64_t))))),
          start_(start + BITS_PER_BYTE * (reinterpret_cast<ValueType *>(data) - data_)),
          size_(size)
    {
    }

    template <typename T, typename = typename T::value_type>
    explicit BitMemoryRegion(T &data)
        : BitMemoryRegion(reinterpret_cast<ValueType *>(data.data()), 0,
                          data.size() * sizeof(typename T::value_type) * BITS_PER_BYTE)
    {
    }

    class Iterator : public std::iterator<std::forward_iterator_tag, uint32_t, ptrdiff_t, void, uint32_t> {
    public:
        static constexpr uint32_t INVAILD_OFFSET = std::numeric_limits<uint32_t>::max();

        Iterator(const BitMemoryRegion &region, uint32_t offset) : region_(region), bit_(offset)
        {
            if (bit_ != region_.Size() && !region_.ReadBit(bit_)) {
                Next(1);
            }
        }

        Iterator &operator++()
        {
            Next(1);
            return *this;
        }

        bool operator==(const Iterator &rhs) const
        {
            return bit_ == rhs.bit_;
        }

        bool operator!=(const Iterator &rhs) const
        {
            return !(*this == rhs);
        }

        Iterator operator+(int32_t n) const
        {
            ASSERT(bit_ + n < region_.Size());
            Iterator it(*this);
            it.Next(n);
            return it;
        }

        Iterator operator-(int32_t n) const
        {
            ASSERT(helpers::ToUnsigned(n) <= bit_);
            Iterator it(*this);
            it.Next(-n);
            return it;
        }

        uint32_t operator*()
        {
            return bit_;
        }

        void Next(uint32_t val)
        {
            ASSERT(val != 0);
            int step = val > 0 ? 1 : -1;
            for (; val != 0; val--) {
                for (bit_ += step; bit_ > 0 && bit_ != region_.Size() && !region_.ReadBit(bit_); bit_ += step) {
                }
                if (bit_ == 0 && !region_.ReadBit(bit_)) {
                    bit_ = region_.Size();
                }
            }
        }

        ~Iterator() = default;

        DEFAULT_COPY_SEMANTIC(Iterator);
        DEFAULT_NOEXCEPT_MOVE_SEMANTIC(Iterator);

    private:
        const BitMemoryRegion &region_;
        uint32_t bit_ {INVAILD_OFFSET};
    };

    Iterator begin() const
    {
        return Iterator(*this, 0);
    }

    Iterator end() const
    {
        return Iterator(*this, Size());
    }

    bool Read(size_t offset)
    {
        ASSERT(offset < size_);
        size_t index = (start_ + offset) / BITS_PER_BYTE;
        size_t shift = (start_ + offset) % BITS_PER_BYTE;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return (data_[index] & (1U << shift)) != 0;
    }

    void Write(bool value, size_t offset)
    {
        ASSERT(offset < size_);
        size_t index = (start_ + offset) / BITS_PER_BYTE;
        size_t shift = (start_ + offset) % BITS_PER_BYTE;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        data_[index] &= ~(1U << shift);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        data_[index] |= ((value ? 1U : 0U) << shift);
    }

    template <typename T = size_t>
    NO_ADDRESS_SANITIZE  // Suppress asan since we can read extra bytes
        T
        Read(size_t offset, size_t length) const
    {
        static_assert(std::is_integral_v<T>, "T must be integral");
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");

        if (length == 0) {
            return 0;
        }

        ASSERT(offset + length <= size_);
        ASSERT(offset < size_);

        const T *data = reinterpret_cast<const T *>(data_);
        size_t width = std::numeric_limits<std::make_unsigned_t<T>>::digits;
        size_t index = (start_ + offset) / width;
        size_t shift = (start_ + offset) % width;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        T value = data[index] >> shift;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        T extra = data[index + (shift + (length - 1)) / width];
        T clear = (std::numeric_limits<T>::max() << 1U) << (length - 1);
        return (value | (extra << ((width - shift) & (width - 1)))) & ~clear;
    }

    template <typename T = size_t>
    NO_ADDRESS_SANITIZE T ReadAll() const
    {
        ASSERT(sizeof(T) * BITS_PER_BYTE >= Size());
        return Read(0, Size());
    }

    bool ReadBit(size_t offset) const
    {
        ASSERT(offset < size_);
        offset += start_;
        size_t index = offset / BITS_PER_BYTE;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return (data_[index] & (1U << (offset & (BITS_PER_BYTE - 1)))) != 0;
    }

    template <typename T = size_t>
    T Pop(size_t length)
    {
        T res = Read(0, length);
        start_ += length;
        return res;
    }

    NO_ADDRESS_SANITIZE
    void Write(uint32_t value, size_t offset, size_t length)
    {
        if (length == 0) {
            return;
        }

        ASSERT(offset + length <= size_);
        ASSERT(offset < size_);

        uint32_t mask = std::numeric_limits<uint32_t>::max() >> (std::numeric_limits<uint32_t>::digits - length);
        size_t index = (start_ + offset) / BITS_PER_BYTE;
        size_t shift = (start_ + offset) % BITS_PER_BYTE;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        data_[index] &= ~(mask << shift);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        data_[index] |= (value << shift);
        size_t end_bits = BITS_PER_BYTE - shift;
        for (int i = 1; end_bits < length; i++, end_bits += BITS_PER_BYTE) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            data_[index + i] &= ~(mask >> end_bits);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            data_[index + i] |= (value >> end_bits);
        }
    }

    BitMemoryRegion Subregion(size_t offset, size_t length)
    {
        ASSERT(offset <= size_);
        ASSERT(offset + length <= size_);
        return BitMemoryRegion(data_, start_ + offset, length);
    }

    BitMemoryRegion Subregion(size_t offset, size_t length) const
    {
        ASSERT(offset <= size_);
        ASSERT(offset + length <= size_);
        return BitMemoryRegion(data_, start_ + offset, length);
    }

    size_t Size() const
    {
        return size_;
    }

    size_t Popcount(size_t first, size_t length) const
    {
        ASSERT(first < Size());
        ASSERT((first + length) <= Size());
        size_t res = 0;
        size_t i = 0;
        for (; (i + BITS_PER_UINT32) < length; i += BITS_PER_UINT32) {
            res += panda::Popcount(Read(first + i, BITS_PER_UINT32));
        }
        return res + panda::Popcount(Read(first + i, length - i));
    }

    size_t Popcount() const
    {
        return Popcount(0, Size());
    }

    void Dump(std::ostream &os) const;

    virtual ~BitMemoryRegion() = default;

    DEFAULT_COPY_SEMANTIC(BitMemoryRegion);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(BitMemoryRegion);

protected:
    void Advance(size_t val)
    {
        ASSERT(val <= size_);
        start_ += val;
        size_ -= val;
    }

private:
    ValueType *data_ {nullptr};
    size_t start_ {0};
    size_t size_ {0};
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_BIT_MEMORY_REGION_H_
