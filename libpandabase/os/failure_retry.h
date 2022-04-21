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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_
#define PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_

#ifdef PANDA_TARGET_UNIX
// Mac Os' libc doesn't have this macro
#ifndef TEMP_FAILURE_RETRY
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEMP_FAILURE_RETRY(exp)                    \
    (__extension__({                               \
        decltype(exp) _result;                     \
        do {                                       \
            _result = (exp);                       \
        } while (_result == -1 && errno == EINTR); \
        _result;                                   \
    }))
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PANDA_FAILURE_RETRY(exp) (__extension__ TEMP_FAILURE_RETRY(exp))
#elif PANDA_TARGET_WINDOWS
// Windows Os does not support TEMP_FAILURE_RETRY macro
#define PANDA_FAILURE_RETRY(exp) (exp)
#else
#error "Unsupported platform"
#endif  // PANDA_TARGET_UNIX

#endif  // PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_
