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

#ifndef PANDA_LIBPANDAFILE_SHORTY_ITERATOR_H_
#define PANDA_LIBPANDAFILE_SHORTY_ITERATOR_H_

#include <cstdint>
#include "file_items.h"
#include "libpandabase/macros.h"

namespace panda::panda_file {
class ShortyIterator {
public:
    ShortyIterator() = default;

    explicit ShortyIterator(const uint16_t *shorty_ptr) : shorty_ptr_(shorty_ptr)
    {
        shorty_ = *shorty_ptr_++;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        element_ = shorty_ & ELEMENT_MASK;
        elem_idx_ = 0;
        ASSERT(element_ != 0);
    }

    ~ShortyIterator() = default;

    DEFAULT_COPY_SEMANTIC(ShortyIterator);
    DEFAULT_MOVE_SEMANTIC(ShortyIterator);

    Type operator*() const
    {
        return Type(static_cast<Type::TypeId>(element_));
    }

    bool operator==(const ShortyIterator &it) const
    {
        return shorty_ptr_ == it.shorty_ptr_ && elem_idx_ == it.elem_idx_;
    }

    bool operator!=(const ShortyIterator &it) const
    {
        return !(*this == it);
    }

    ShortyIterator &operator++()
    {
        if (element_ == 0) {
            return *this;
        }
        ++elem_idx_;
        if (elem_idx_ == NUM_ELEMENTS_PER_16BIT) {
            shorty_ = *shorty_ptr_++;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            elem_idx_ = 0;
        } else {
            shorty_ >>= NUM_BITS_PER_ELEMENT;
        }
        element_ = shorty_ & ELEMENT_MASK;
        if (element_ == 0) {
            *this = ShortyIterator();
        }
        return *this;
    }

    ShortyIterator operator++(int)  // NOLINT(cert-dcl21-cpp)
    {
        ShortyIterator prev = *this;
        ++(*this);
        return prev;
    }

private:
    static constexpr uint32_t NUM_ELEMENTS_PER_16BIT = 4;
    static constexpr uint32_t NUM_BITS_PER_ELEMENT = 4;
    static constexpr uint32_t ELEMENT_MASK = 0xF;

    const uint16_t *shorty_ptr_ {nullptr};
    uint16_t shorty_ {0};
    uint16_t element_ {0};
    uint16_t elem_idx_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_SHORTY_ITERATOR_H_
