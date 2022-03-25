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

#include <cstring>
#include <string>

namespace panda::terminate {

#ifndef FUZZING_EXIT_ON_FAILED_ASSERT_FOR
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FUZZING_EXIT_ON_FAILED_ASSERT_FOR ""
#endif

[[noreturn]] void Terminate(const char *file)
{
    auto filepath = std::string(file);
    std::string libsTmp;

    char *replace = std::getenv("FUZZING_EXIT_ON_FAILED_ASSERT");
    if ((replace != nullptr) && (std::string(replace) == "false")) {
        std::abort();
    }

    char *libs = std::getenv("FUZZING_EXIT_ON_FAILED_ASSERT_FOR");
    if (libs == nullptr) {
        libsTmp = std::string(FUZZING_EXIT_ON_FAILED_ASSERT_FOR);
        if (libsTmp.empty()) {
            std::abort();
        }
        libs = libsTmp.data();
    }

    char *lib = strtok(libs, ",");
    while (lib != nullptr) {
        if (filepath.find(lib) != std::string::npos) {
            std::exit(1);
        }
        lib = strtok(nullptr, ",");
    }
    std::abort();
}

}  // namespace panda::terminate
