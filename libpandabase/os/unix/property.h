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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_PROPERTY_H_
#define PANDA_LIBPANDABASE_OS_UNIX_PROPERTY_H_

#include <string>
#include <vector>
#include <map>

namespace panda::os::unix::property {

#ifdef PANDA_TARGET_MOBILE
static const char *ark_dfx_prop = "ark.dfx.options";
static const char *ark_trace_prop = "ark.trace.enable";

std::string GetPropertyBuffer(const char *ark_prop);
#endif  // PANDA_TARGET_MOBILE
}  // namespace panda::os::unix::property

#endif  // PANDA_LIBPANDABASE_OS_UNIX_PROPERTY_H_
