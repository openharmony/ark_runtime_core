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

#ifndef PANDA_VERIFICATION_DEBUG_CONFIG_CONFIG_H_
#define PANDA_VERIFICATION_DEBUG_CONFIG_CONFIG_H_

#include "libpandabase/utils/logger.h"

#include "runtime/include/mem/panda_string.h"

#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

namespace panda::verifier::config {
struct Section {
    PandaString name;
    std::vector<struct Section> sections;
    std::vector<PandaString> items;
    PandaString Image(size_t align = 0) const
    {
        PandaString spaces(align, ' ');
        PandaString result = spaces + name + " {\n";
        for (const auto &s : sections) {
            result += s.Image(align + 2);
        }
        for (const auto &i : items) {
            result += spaces + "  " + i + "\n";
        }
        result += spaces + "}\n";
        return result;
    }
    const Section &operator[](const PandaString &title) const
    {
        for (const auto &s : sections) {
            if (s.name == title) {
                return s;
            }
        }
        LOG(FATAL, VERIFIER) << "config section '" << title << "' not found";
        UNREACHABLE();
    }
};
}  // namespace panda::verifier::config

#endif  // PANDA_VERIFICATION_DEBUG_CONFIG_CONFIG_H_
