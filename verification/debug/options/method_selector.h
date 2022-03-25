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

#ifndef PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_SELECTOR_H_
#define PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_SELECTOR_H_

#include <regex>

template <typename Info, template <typename...> class Vector, typename Regex, typename String>
class VerifierMethodSelector {
public:
    using InfoRef = std::reference_wrapper<const Info>;

    void Add(const String &regex_str, const Info &info)
    {
        groups.emplace_back(Regex {regex_str, std::regex_constants::basic | std::regex_constants::optimize |
                                                  std::regex_constants::nosubs | std::regex_constants::icase},
                            std::cref(info));
    }

    std::optional<InfoRef> operator[](const String &name) const
    {
        for (const auto &g : groups) {
            const auto &regex = g.first;
            const auto &info_ref = g.second;
            if (std::regex_match(name, regex)) {
                return info_ref;
            }
        }
        return std::nullopt;
    }

private:
    Vector<std::pair<Regex, InfoRef>> groups;
};

#endif  // PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_SELECTOR_H_
