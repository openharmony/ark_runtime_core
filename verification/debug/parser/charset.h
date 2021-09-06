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

#ifndef PANDA_VERIFICATION_DEBUG_PARSER_CHARSET_H_
#define PANDA_VERIFICATION_DEBUG_PARSER_CHARSET_H_

#include "macros.h"

#include <cstdint>

namespace panda::parser {

template <typename Char>
class charset {
public:
    constexpr bool operator()(Char c_) const
    {
        uint8_t c = static_cast<uint8_t>(c_);
        return (bitmap[(c) >> 0x6U] & (0x1ULL << ((c)&0x3FU))) != 0;
    }

    charset() {}
    charset(const charset &c) = default;
    charset(charset &&c) = default;
    charset &operator=(const charset &c) = default;
    charset &operator=(charset &&c) = default;
    ~charset() = default;

    constexpr charset(Char *s)
    {
        ASSERT(s != nullptr);
        while (*s) {
            uint8_t c = static_cast<uint8_t>(*s);
            // NB!: 1ULL (64bit) is mandatory due to bug in clang optimizations on -O2 and higher
            bitmap[(c) >> 0x6U] = static_cast<uint64_t>(bitmap[(c) >> 0x6U] | (0x1ULL << ((c)&0x3FU)));
            ++s;
        }
    }

    constexpr charset operator+(const charset &c) const
    {
        charset cs;
        cs.bitmap[0x0] = bitmap[0x0] | c.bitmap[0x0];
        cs.bitmap[0x1] = bitmap[0x1] | c.bitmap[0x1];
        cs.bitmap[0x2] = bitmap[0x2] | c.bitmap[0x2];
        cs.bitmap[0x3] = bitmap[0x3] | c.bitmap[0x3];
        return cs;
    }

    constexpr charset operator-(const charset &c) const
    {
        charset cs;
        cs.bitmap[0x0] = bitmap[0x0] & ~c.bitmap[0x0];
        cs.bitmap[0x1] = bitmap[0x1] & ~c.bitmap[0x1];
        cs.bitmap[0x2] = bitmap[0x2] & ~c.bitmap[0x2];
        cs.bitmap[0x3] = bitmap[0x3] & ~c.bitmap[0x3];
        return cs;
    }

    constexpr charset operator!() const
    {
        charset cs;
        cs.bitmap[0x0] = ~bitmap[0x0];
        cs.bitmap[0x1] = ~bitmap[0x1];
        cs.bitmap[0x2] = ~bitmap[0x2];
        cs.bitmap[0x3] = ~bitmap[0x3];
        return cs;
    }

private:
    uint64_t bitmap[4] = {
        0x0ULL,
    };
};

}  // namespace panda::parser

#endif  // PANDA_VERIFICATION_DEBUG_PARSER_CHARSET_H_
