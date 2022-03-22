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

#ifndef PANDA_VERIFICATION_UTIL_BIT_VECTOR_H_
#define PANDA_VERIFICATION_UTIL_BIT_VECTOR_H_

#include "utils/bit_utils.h"
#include "function_traits.h"
#include "panda_or_std.h"
#include "macros.h"
#include "index.h"

#include <utility>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <tuple>
#include <iostream>

namespace panda::verifier {

#ifdef PANDA_TARGET_64
using Word = uint64_t;
#else
using Word = uint32_t;
#endif

template <typename GetFunc>
class ConstBits {
public:
    ConstBits(GetFunc &&f) : GetF_ {std::move(f)} {}
    ~ConstBits() = default;
    ConstBits() = delete;
    ConstBits(const ConstBits &) = delete;
    ConstBits(ConstBits &&) = default;
    ConstBits &operator=(const ConstBits &) = delete;
    ConstBits &operator=(ConstBits &&) = default;

    operator Word() const
    {
        return GetF_();
    }

    template <typename Rhs>
    bool operator==(const Rhs &rhs) const
    {
        return GetF_() == rhs.GetF_();
    }

private:
    GetFunc GetF_;
    template <typename A>
    friend class ConstBits;
};

template <typename GetFunc, typename SetFunc>
class Bits : public ConstBits<GetFunc> {
public:
    Bits(GetFunc &&get, SetFunc &&set) : ConstBits<GetFunc>(std::move(get)), SetF_ {std::move(set)} {}
    ~Bits() = default;
    Bits() = delete;
    Bits(const Bits &) = delete;
    Bits(Bits &&) = default;
    Bits &operator=(const Bits &rhs)
    {
        SetF_(rhs);
        return *this;
    }
    Bits &operator=(Bits &&) = default;

    Bits &operator=(Word val)
    {
        SetF_(val);
        return *this;
    }

private:
    SetFunc SetF_;
};

class BitVector {
    using Allocator = MPandaAllocator<Word>;
    static constexpr size_t BITS_IN_WORD = sizeof(Word) * 8;
    static constexpr size_t BITS_IN_INT = sizeof(int) * 8;
    size_t MAX_BIT_IDX() const
    {
        return size_ - 1;
    }
    static constexpr size_t POS_SHIFT = panda::Ctz(BITS_IN_WORD);
    static constexpr size_t POS_MASK = BITS_IN_WORD - 1;
    static constexpr Word MAX_WORD = std::numeric_limits<Word>::max();

    static Word MaskForIndex(size_t idx)
    {
        return static_cast<Word>(1) << idx;
    }

    static Word MaskUpToIndex(size_t idx)
    {
        return idx >= BITS_IN_WORD ? MAX_WORD : ((static_cast<Word>(1) << idx) - 1);
    }

    class Bit {
    public:
        Bit(BitVector &bit_vector, size_t index) : bit_vector_ {bit_vector}, index_ {index} {};
        ~Bit() = default;
        NO_MOVE_SEMANTIC(Bit);
        NO_COPY_SEMANTIC(Bit);

        operator bool() const
        {
            return const_cast<const BitVector &>(bit_vector_)[index_];
        }

        Bit &operator=(bool value)
        {
            if (value) {
                bit_vector_.Set(index_);
            } else {
                bit_vector_.Clr(index_);
            }
            return *this;
        }

    private:
        BitVector &bit_vector_;
        size_t index_;
    };

    static constexpr size_t SizeInBitsFromSizeInWords(size_t size)
    {
        return size << POS_SHIFT;
    }

    static constexpr size_t SizeInWordsFromSizeInBits(size_t size)
    {
        return (size + POS_MASK) >> POS_SHIFT;
    }

    void Deallocate()
    {
        if (data_ != nullptr) {
            Allocator allocator;
            allocator.deallocate(data_, size_in_words());
        }
        size_ = 0;
        data_ = nullptr;
    }

public:
    BitVector(size_t sz) : size_ {sz}, data_ {Allocator().allocate(size_in_words())}
    {
        clr();
    }
    ~BitVector()
    {
        Deallocate();
    }

    BitVector(const BitVector &other)
    {
        CopyFrom(other);
    }

    BitVector(BitVector &&other) noexcept
    {
        MoveFrom(std::move(other));
    }

    BitVector &operator=(const BitVector &rhs)
    {
        Deallocate();
        CopyFrom(rhs);
        return *this;
    }

    BitVector &operator=(BitVector &&rhs) noexcept
    {
        Deallocate();
        MoveFrom(std::move(rhs));
        return *this;
    }

    auto bits(size_t from, size_t to) const
    {
        ASSERT(data_ != nullptr);
        ASSERT(from <= to);
        ASSERT(to <= MAX_BIT_IDX());
        ASSERT(to - from <= BITS_IN_WORD - 1);
        const Word MASK = MaskUpToIndex(to - from + 1);
        const size_t POS_FROM = from >> POS_SHIFT;
        const size_t POS_TO = to >> POS_SHIFT;
        const size_t IDX_FROM = from & POS_MASK;
        return ConstBits([this, MASK, POS_FROM, POS_TO, IDX_FROM]() -> Word {
            if (POS_FROM == POS_TO) {
                return (data_[POS_FROM] >> IDX_FROM) & MASK;
            } else {
                Word data = (data_[POS_FROM] >> IDX_FROM) | (data_[POS_TO] << (BITS_IN_WORD - IDX_FROM));
                return data & MASK;
            }
        });
    }

    auto bits(size_t from, size_t to)
    {
        ASSERT(data_ != nullptr);
        ASSERT(from <= to);
        ASSERT(to <= MAX_BIT_IDX());
        ASSERT(to - from <= BITS_IN_WORD - 1);
        const Word MASK = MaskUpToIndex(to - from + 1);
        const size_t POS_FROM = from >> POS_SHIFT;
        const size_t POS_TO = to >> POS_SHIFT;
        const size_t IDX_FROM = from & POS_MASK;
        return Bits(
            [this, MASK, POS_FROM, POS_TO, IDX_FROM]() -> Word {
                if (POS_FROM == POS_TO) {
                    return (data_[POS_FROM] >> IDX_FROM) & MASK;
                } else {
                    Word data = (data_[POS_FROM] >> IDX_FROM) | (data_[POS_TO] << (BITS_IN_WORD - IDX_FROM));
                    return data & MASK;
                }
            },
            [this, MASK, POS_FROM, POS_TO, IDX_FROM](Word val) {
                const Word VAL = val & MASK;
                const auto LOW_MASK = MASK << IDX_FROM;
                const auto LOW_VAL = VAL << IDX_FROM;
                if (POS_FROM == POS_TO) {
                    data_[POS_FROM] &= ~LOW_MASK;
                    data_[POS_FROM] |= LOW_VAL;
                } else {
                    const auto HIGH_SHIFT = BITS_IN_WORD - IDX_FROM;
                    const auto HIGH_MASK = MASK >> HIGH_SHIFT;
                    const auto HIGH_VAL = VAL >> HIGH_SHIFT;
                    data_[POS_FROM] &= ~LOW_MASK;
                    data_[POS_FROM] |= LOW_VAL;
                    data_[POS_TO] &= ~HIGH_MASK;
                    data_[POS_TO] |= HIGH_VAL;
                }
            });
    }

    bool operator[](size_t idx) const
    {
        ASSERT(idx < size());
        return data_[idx >> POS_SHIFT] & MaskForIndex(idx % BITS_IN_WORD);
    }
    Bit operator[](size_t idx)
    {
        return {*this, idx};
    }

    void clr()
    {
        for (size_t pos = 0; pos < size_in_words(); ++pos) {
            data_[pos] = 0;
        }
    }
    void set()
    {
        for (size_t pos = 0; pos < size_in_words(); ++pos) {
            data_[pos] = MAX_WORD;
        }
    }
    void invert()
    {
        for (size_t pos = 0; pos < size_in_words(); ++pos) {
            data_[pos] = ~data_[pos];
        }
    }
    void Clr(size_t idx)
    {
        ASSERT(idx < size());
        data_[idx >> POS_SHIFT] &= ~MaskForIndex(idx % BITS_IN_WORD);
    }
    void Set(size_t idx)
    {
        ASSERT(idx < size());
        data_[idx >> POS_SHIFT] |= MaskForIndex(idx % BITS_IN_WORD);
    }
    void invert(size_t idx)
    {
        operator[](idx) = !operator[](idx);
    }

    BitVector operator~() const
    {
        BitVector result {*this};
        result.invert();
        return result;
    }

    bool operator==(const BitVector &rhs) const
    {
        if (size() != rhs.size()) {
            return false;
        }
        size_t last_word_partial_bits = size() % BITS_IN_WORD;
        size_t num_full_words = size_in_words() - (last_word_partial_bits ? 1 : 0);
        for (size_t pos = 0; pos < num_full_words; pos++) {
            if (data_[pos] != rhs.data_[pos]) {
                return false;
            }
        }
        if (last_word_partial_bits) {
            size_t last_word_start = size() - last_word_partial_bits;
            return bits(last_word_start, size() - 1) == rhs.bits(last_word_start, size() - 1);
        }
        return true;
    }

    bool operator!=(const BitVector &rhs) const
    {
        return !(*this == rhs);
    }

    template <typename Handler>
    void process(size_t from, size_t to, Handler handler)
    {
        ASSERT(data_ != nullptr);
        ASSERT(from <= to);
        ASSERT(to <= MAX_BIT_IDX());
        const size_t POS_FROM = from >> POS_SHIFT;
        const size_t POS_TO = to >> POS_SHIFT;
        const size_t IDX_FROM = from & POS_MASK;
        const size_t IDX_TO = to & POS_MASK;
        auto process_word = [this, &handler](size_t pos) {
            const Word VAL = handler(data_[pos], BITS_IN_WORD);
            data_[pos] = VAL;
        };
        auto process_part = [this, &handler, &process_word](size_t pos, size_t idx_from, size_t idx_to) {
            const auto LEN = idx_to - idx_from + 1;
            if (LEN == BITS_IN_WORD) {
                process_word(pos);
            } else {
                const Word MASK = MaskUpToIndex(LEN);
                const Word VAL = handler((data_[pos] >> idx_from) & MASK, LEN) & MASK;
                data_[pos] &= ~(MASK << idx_from);
                data_[pos] |= VAL << idx_from;
            }
        };
        if (POS_FROM == POS_TO) {
            process_part(POS_FROM, IDX_FROM, IDX_TO);
        } else {
            process_part(POS_FROM, IDX_FROM, BITS_IN_WORD - 1);
            for (size_t pos = POS_FROM + 1; pos < POS_TO; ++pos) {
                process_word(pos);
            }
            process_part(POS_TO, 0, IDX_TO);
        }
    }

    void Clr(size_t from, size_t to)
    {
        process(from, to, [](auto, auto) { return static_cast<Word>(0); });
    }

    void Set(size_t from, size_t to)
    {
        process(from, to, [](auto, auto) { return MAX_WORD; });
    }

    void invert(size_t from, size_t to)
    {
        process(from, to, [](auto val, auto) { return ~val; });
    }

    template <typename Handler>
    void process(const BitVector &rhs, Handler &&handler)
    {
        size_t sz = std::min(size(), rhs.size());
        size_t words = SizeInWordsFromSizeInBits(sz);
        size_t pos = 0;
        bool last_word_partially_filled = ((sz & POS_MASK) == 0) ? false : true;
        if (words > 0) {
            for (; pos < (words - (last_word_partially_filled ? 1 : 0)); ++pos) {
                data_[pos] = handler(data_[pos], rhs.data_[pos]);
            }
        }
        if (last_word_partially_filled) {
            const Word MASK = MaskUpToIndex(sz & POS_MASK);
            data_[pos] = (data_[pos] & ~MASK) | (handler(data_[pos] & MASK, rhs.data_[pos] & MASK) & MASK);
        }
    }

    BitVector &operator&=(const BitVector &rhs)
    {
        process(rhs, [](const auto l, const auto r) { return l & r; });
        return *this;
    }

    BitVector &operator|=(const BitVector &rhs)
    {
        process(rhs, [](const auto l, const auto r) { return l | r; });
        return *this;
    }

    BitVector &operator^=(const BitVector &rhs)
    {
        process(rhs, [](const auto l, const auto r) { return l ^ r; });
        return *this;
    }

    BitVector operator&(const BitVector &rhs) const
    {
        if (size() > rhs.size()) {
            return rhs & *this;
        }
        BitVector result {*this};
        result &= rhs;
        return result;
    }

    BitVector operator|(const BitVector &rhs) const
    {
        if (size() < rhs.size()) {
            return rhs | *this;
        }
        BitVector result {*this};
        result |= rhs;
        return result;
    }

    BitVector operator^(const BitVector &rhs) const
    {
        if (size() < rhs.size()) {
            return rhs ^ *this;
        }
        BitVector result {*this};
        result ^= rhs;
        return result;
    }

    template <typename Handler>
    void for_all_idx_val(Handler handler) const
    {
        size_t last_word_partial_bits = size() % BITS_IN_WORD;
        size_t num_full_words = size_in_words() - (last_word_partial_bits ? 1 : 0);
        for (size_t pos = 0; pos < num_full_words; pos++) {
            Word val = data_[pos];
            if (!handler(pos * BITS_IN_WORD, val)) {
                return;
            }
        }
        if (last_word_partial_bits) {
            size_t last_word_start = size() - last_word_partial_bits;
            Word val = bits(last_word_start, size() - 1);
            handler(last_word_start, val);
        }
    }

    template <const int Val, typename Handler>
    bool for_all_idx_of(Handler handler) const
    {
        for (size_t pos = 0; pos < size_in_words(); ++pos) {
            auto val = data_[pos];
            val = Val ? val : ~val;
            size_t idx = pos << POS_SHIFT;
            while (val) {
                size_t i = panda::Ctz(val);
                idx += i;
                if (idx >= size()) {
                    return true;
                }
                if (!handler(idx)) {
                    return false;
                }
                ++idx;
                val >>= i;
                val >>= 1;
            }
        }
        return true;
    }

    template <const int Val>
    auto LazyIndicesOf(size_t from = 0, size_t to = std::numeric_limits<size_t>::max()) const
    {
        size_t idx = from;
        size_t pos = from >> POS_SHIFT;
        auto val = (Val ? data_[pos] : ~data_[pos]) >> (idx % BITS_IN_WORD);
        ++pos;
        to = std::min(size_ - 1, to);
        auto sz = SizeInWordsFromSizeInBits(to + 1);
        return [this, sz, to, pos, val, idx]() mutable -> Index<size_t> {
            while (true) {
                if (idx > to) {
                    return {};
                }
                if (val) {
                    size_t i = panda::Ctz(val);
                    idx += i;
                    if (idx > to) {
                        return {};
                    }
                    val >>= i;
                    val >>= 1;
                    return idx++;
                }
                while (val == 0 && pos < sz) {
                    val = Val ? data_[pos] : ~data_[pos];
                    ++pos;
                }
                idx = (pos - 1) << POS_SHIFT;
                if (pos >= sz && val == 0) {
                    return {};
                }
            }
        };
    }

    size_t SetBitsCount() const
    {
        size_t result = 0;

        size_t pos = 0;
        bool last_word_partially_filled = ((size() & POS_MASK) == 0) ? false : true;
        if (size_in_words() > 0) {
            for (; pos < (size_in_words() - (last_word_partially_filled ? 1 : 0)); ++pos) {
                result += panda::Popcount(data_[pos]);
            }
        }
        if (last_word_partially_filled) {
            const Word MASK = MaskUpToIndex(size() & POS_MASK);
            result += panda::Popcount(data_[pos] & MASK);
        }
        return result;
    }

    template <typename Op, typename Binop, typename... Args>
    static size_t power_of_op_then_fold(Op op, Binop binop, const Args &... args)
    {
        size_t result = 0;

        size_t sz = n_ary {[](size_t a, size_t b) { return std::min(a, b); }}(args.size_in_words()...);
        size_t size = n_ary {[](size_t a, size_t b) { return std::min(a, b); }}(args.size()...);
        size_t num_args = sizeof...(Args);
        auto get_processed_word = [&op, &binop, num_args, &args...](size_t idx) {
            size_t n = 0;
            auto unop = [&n, num_args, &op](Word val) { return op(val, n++, num_args); };
            return n_ary {binop}(std::tuple<std::decay_t<decltype(args.data_[idx])>...> {unop(args.data_[idx])...});
        };

        size_t pos = 0;
        bool last_word_partially_filled = ((size & POS_MASK) == 0) ? false : true;
        if (sz > 0) {
            for (; pos < (sz - (last_word_partially_filled ? 1 : 0)); ++pos) {
                auto val = get_processed_word(pos);
                result += panda::Popcount(val);
            }
        }
        if (last_word_partially_filled) {
            const Word MASK = MaskUpToIndex(size & POS_MASK);
            result += panda::Popcount(get_processed_word(pos) & MASK);
        }
        return result;
    }

    template <typename... Args>
    static size_t power_of_and(const Args &... args)
    {
        return power_of_op_then_fold([](Word val, size_t, size_t) { return val; },
                                     [](Word lhs, Word rhs) { return lhs & rhs; }, args...);
    }

    template <typename... Args>
    static size_t power_of_or(const Args &... args)
    {
        return power_of_op_then_fold([](Word val, size_t, size_t) { return val; },
                                     [](Word lhs, Word rhs) { return lhs | rhs; }, args...);
    }

    template <typename... Args>
    static size_t power_of_xor(const Args &... args)
    {
        return power_of_op_then_fold([](Word val, size_t, size_t) { return val; },
                                     [](Word lhs, Word rhs) { return lhs ^ rhs; }, args...);
    }

    template <typename... Args>
    static size_t power_of_and_not(const Args &... args)
    {
        return power_of_op_then_fold(
            [](Word val, size_t idx, size_t num_args) { return (idx < num_args - 1) ? val : ~val; },
            [](Word lhs, Word rhs) { return lhs & rhs; }, args...);
    }

    size_t size() const
    {
        return size_;
    }

    size_t size_in_words() const
    {
        return SizeInWordsFromSizeInBits(size_);
    }

    void resize(size_t sz)
    {
        if (sz == 0) {
            Deallocate();
        } else {
            size_t new_size_in_words = SizeInWordsFromSizeInBits(sz);
            size_t old_size_in_words = SizeInWordsFromSizeInBits(size_);
            if (old_size_in_words != new_size_in_words) {
                Allocator allocator;
                Word *new_data = allocator.allocate(sz);
                ASSERT(new_data != nullptr);
                size_t pos = 0;
                for (; pos < std::min(old_size_in_words, new_size_in_words); ++pos) {
                    new_data[pos] = data_[pos];
                }
                for (; pos < new_size_in_words; ++pos) {
                    new_data[pos] = 0;
                }
                Deallocate();
                data_ = new_data;
            }
            size_ = sz;
        }
    }

    template <const int V, typename Op, typename BinOp, typename... Args>
    static auto lazy_op_then_fold_then_indices_of(Op op, BinOp binop, const Args &... args)
    {
        using namespace panda::verifier;
        size_t sz = n_ary {[](size_t a, size_t b) { return std::min(a, b); }}(args.size_in_words()...);
        size_t size = n_ary {[](size_t a, size_t b) { return std::min(a, b); }}(args.size()...);
        size_t num_args = sizeof...(Args);
        auto get_processed_word = [op, binop, num_args, &args...](size_t idx) {
            size_t n = 0;
            auto unop = [&n, num_args, &op](Word val) { return op(val, n++, num_args); };
            Word val = n_ary {binop}(std::tuple<std::decay_t<decltype(args.data_[idx])>...> {unop(args.data_[idx])...});
            return V ? val : ~val;
        };
        size_t pos = 0;
        auto val = get_processed_word(pos++);
        size_t idx = 0;
        return [sz, size, pos, val, idx, get_processed_word]() mutable -> Index<size_t> {
            do {
                if (idx >= size) {
                    return {};
                }
                if (val) {
                    size_t i = panda::Ctz(val);
                    idx += i;
                    if (idx >= size) {
                        return {};
                    }
                    val >>= i;
                    val >>= 1;
                    return idx++;
                }
                while (val == 0 && pos < sz) {
                    val = get_processed_word(pos++);
                }
                idx = (pos - 1) << POS_SHIFT;
                if (pos >= sz && val == 0) {
                    return {};
                }
            } while (true);
        };
    }

    template <const int V, typename... Args>
    static auto lazy_and_then_indices_of(const Args &... args)
    {
        return lazy_op_then_fold_then_indices_of<V>([](Word val, size_t, size_t) { return val; },
                                                    [](Word lhs, Word rhs) { return lhs & rhs; }, args...);
    }

    template <const int V, typename... Args>
    static auto lazy_or_then_indices_of(const Args &... args)
    {
        return lazy_op_then_fold_then_indices_of<V>([](Word val, size_t, size_t) { return val; },
                                                    [](Word lhs, Word rhs) { return lhs | rhs; }, args...);
    }

    template <const int V, typename... Args>
    static auto lazy_xor_then_indices_of(const Args &... args)
    {
        return lazy_op_then_fold_then_indices_of<V>([](Word val, size_t, size_t) { return val; },
                                                    [](Word lhs, Word rhs) { return lhs ^ rhs; }, args...);
    }

    template <const int V, typename... Args>
    static auto lazy_and_not_then_indices_of(const Args &... args)
    {
        return lazy_op_then_fold_then_indices_of<V>(
            [](Word val, size_t idx, size_t num_args) {
                val = (idx < num_args - 1) ? val : ~val;
                return val;
            },
            [](Word lhs, Word rhs) { return lhs & rhs; }, args...);
    }

private:
    size_t size_;
    Word *data_ = nullptr;

    void CopyFrom(const BitVector &other)
    {
        size_ = other.size_;
        size_t size_in_words = other.size_in_words();
        data_ = Allocator().allocate(size_in_words);
        std::copy_n(other.data_, size_in_words, data_);
    }

    void MoveFrom(BitVector &&other) noexcept
    {
        size_ = other.size_;
        data_ = other.data_;
        // don't rhs.Deallocate() as we stole its data_!
        other.size_ = 0;
        other.data_ = nullptr;
    }
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_BIT_VECTOR_H_
