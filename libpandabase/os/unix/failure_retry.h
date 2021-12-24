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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_
#define PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_

// Mac Os' libc doesn't have this macro
#ifndef TEMP_FAILURE_RETRY
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

#endif  // PANDA_LIBPANDABASE_OS_UNIX_FAILURE_RETRY_H_
