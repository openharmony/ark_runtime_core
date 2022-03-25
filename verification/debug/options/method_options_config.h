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

#ifndef PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_OPTIONS_CONFIG_H_
#define PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_OPTIONS_CONFIG_H_

#include "method_options.h"
#include "method_selector.h"

#include <regex>

namespace panda::verifier {

template <typename String, typename VerifierMessagesEnum, template <typename...> class UnorderedMap,
          template <typename...> class Vector>
class VerifierMethodOptionsConfig {
public:
    using MethodOptions = VerifierMethodOptions<String, VerifierMessagesEnum, UnorderedMap, Vector>;
    using MethodSelector = VerifierMethodSelector<MethodOptions, Vector, std::regex, String>;
    MethodOptions &NewOptions(const String &name)
    {
        return Config.emplace(name, name).first->second;
    }
    const MethodOptions &GetOptions(const String &name) const
    {
        return Config.at(name);
    }
    bool IsOptionsPresent(const String &name) const
    {
        return Config.count(name) > 0;
    }
    auto operator[](const String &method_name) const
    {
        return MethodGroups[method_name];
    }
    bool AddOptionsForGroup(const String &group_regex, const String &options_name)
    {
        if (!IsOptionsPresent(options_name)) {
            return false;
        }
        MethodGroups.Add(group_regex, GetOptions(options_name));
        return true;
    }

private:
    UnorderedMap<String, MethodOptions> Config;
    MethodSelector MethodGroups;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_OPTIONS_CONFIG_H_
