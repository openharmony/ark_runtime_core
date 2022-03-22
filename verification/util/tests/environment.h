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

#ifndef PANDA_VERIFICATION_UTIL_TESTS_ENVIRONMENT_H_
#define PANDA_VERIFICATION_UTIL_TESTS_ENVIRONMENT_H_

#include <unordered_map>
#include <string>
#include <variant>
#include <optional>

namespace panda::verifier::test {
using OptionValue = std::variant<std::string, int, bool>;

class EnvOptions {
public:
    std::optional<OptionValue> operator[](const std::string &name) const;
    template <typename T>
    T Get(const std::string &name, T deflt = T {}) const
    {
        auto val = (*this)[name];
        if (!val) {
            return deflt;
        }
        if (auto pntr = std::get_if<T>(&*val)) {
            return *pntr;
        }
        return deflt;
    }
    EnvOptions(const char *);
    EnvOptions(const EnvOptions &) = default;
    EnvOptions(EnvOptions &&) = default;
    EnvOptions &operator=(const EnvOptions &) = default;
    EnvOptions &operator=(EnvOptions &&) = default;
    ~EnvOptions() = default;

private:
    std::unordered_map<std::string, OptionValue> Options_;
};
}  // namespace panda::verifier::test

#endif  // PANDA_VERIFICATION_UTIL_TESTS_ENVIRONMENT_H_
