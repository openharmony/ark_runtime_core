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

#include "runtime/include/mem/panda_string.h"

#include <cmath>
#include <cstdlib>

#include "libpandabase/macros.h"
#include "runtime/include/coretypes/string.h"

namespace panda {

static constexpr int BASE = 10;

int64_t PandaStringToLL(const PandaString &str)
{
    [[maybe_unused]] char *end_ptr = nullptr;
    int64_t result = std::strtoll(str.c_str(), &end_ptr, BASE);
    ASSERT(!(result == 0 && str.c_str() == end_ptr) && "PandaString argument is not long long int");
    return result;
}

uint64_t PandaStringToULL(const PandaString &str)
{
    [[maybe_unused]] char *end_ptr = nullptr;
    uint64_t result = std::strtoull(str.c_str(), &end_ptr, BASE);
    ASSERT(!(result == 0 && str.c_str() == end_ptr) && "PandaString argument is not unsigned long long int");
    return result;
}

float PandaStringToF(const PandaString &str)
{
    [[maybe_unused]] char *end_ptr = nullptr;
    float result = std::strtof(str.c_str(), &end_ptr);
    ASSERT(result != HUGE_VALF && "PandaString argument is not float");
    ASSERT(!(result == 0 && str.c_str() == end_ptr) && "PandaString argument is not float");
    return result;
}

double PandaStringToD(const PandaString &str)
{
    [[maybe_unused]] char *end_ptr = nullptr;
    double result = std::strtod(str.c_str(), &end_ptr);
    ASSERT(result != HUGE_VALF && "PandaString argument is not double");
    ASSERT(!(result == 0 && str.c_str() == end_ptr) && "PandaString argument is not double");
    return result;
}

PandaString ConvertToString(Span<const uint8_t> sp)
{
    PandaString res;
    res.reserve(sp.size());
    for (auto c : sp) {
        res.push_back(c);
    }
    return res;
}

// NB! the following function need additional mem allocation, donnot use when unnecessary!
PandaString ConvertToString(const std::string &str)
{
    PandaString res;
    res.reserve(str.size());
    for (auto c : str) {
        res.push_back(c);
    }
    return res;
}

PandaString ConvertToString(coretypes::String *s)
{
    ASSERT(s != nullptr);
    if (s->IsUtf16()) {
        // Should convert utf-16 to utf-8, because uint16_t likely greater than MAX_CHAR, will convert fail
        size_t len = utf::Utf16ToMUtf8Size(s->GetDataUtf16(), s->GetUtf16Length()) - 1;
        PandaVector<uint8_t> buf(len);
        len = utf::ConvertRegionUtf16ToMUtf8(s->GetDataUtf16(), buf.data(), s->GetUtf16Length(), len, 0);
        Span<const uint8_t> sp(buf.data(), len);
        return ConvertToString(sp);
    }

    Span<const uint8_t> sp(s->GetDataMUtf8(), s->GetLength());
    return ConvertToString(sp);
}

}  // namespace panda
