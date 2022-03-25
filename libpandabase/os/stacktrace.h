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

#ifndef PANDA_LIBPANDABASE_OS_STACKTRACE_H_
#define PANDA_LIBPANDABASE_OS_STACKTRACE_H_

#include <vector>
#include <iostream>
#include "macros.h"

namespace panda {

class StackPrinterImpl;

/*
 * Return stack trace as a vector of PCs
 *
 * Use std::vector instead of PandaVector to have ability
 * to print stack traces in internal allocator.
 * Since PandaVector uses internal allocator it leads to
 * infinite recursion.
 */
std::vector<uintptr_t> GetStacktrace();

/*
 * Print stack trace provided into 'Print' function.
 * The class caches information. So it is aimed to be used
 * to print multiple stack traces.
 */
std::ostream &PrintStack(const std::vector<uintptr_t> &stacktrace, std::ostream &out);

/*
 * Print stack trace
 */
// NOLINTNEXTLINE(misc-definitions-in-headers)
inline std::ostream &PrintStack(std::ostream &out)
{
    return PrintStack(GetStacktrace(), out);
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_OS_STACKTRACE_H_
