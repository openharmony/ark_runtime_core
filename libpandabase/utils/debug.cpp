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

#include "debug.h"
#include "terminate.h"
#include "os/thread.h"
#include "os/stacktrace.h"
#include <iostream>
#include <iomanip>

namespace panda::debug {

[[noreturn]] void AssertionFail(const char *expr, const char *file, unsigned line, const char *function)
{
    std::cerr << "ASSERTION FAILED: " << expr << std::endl;
    std::cerr << "IN " << file << ":" << line << ": " << function << std::endl;
    std::cerr << "Backtrace [tid=" << os::thread::GetCurrentThreadId() << "]:\n";
    PrintStack(std::cerr);
#ifdef FUZZING_EXIT_ON_FAILED_ASSERT
    panda::terminate::Terminate(file);
#else
    std::abort();
#endif
}

}  // namespace panda::debug
