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

#ifndef PANDA_LIBPANDABASE_OS_PROPERTY_H_
#define PANDA_LIBPANDABASE_OS_PROPERTY_H_

#if defined(PANDA_TARGET_UNIX)
#include "os/unix/property.h"
#endif  // PANDA_TARGET_UNIX

#include <string>

namespace panda::os::property {

#if defined(PANDA_TARGET_UNIX)
#ifdef PANDA_TARGET_MOBILE
const auto ark_dfx_prop = panda::os::unix::property::ark_dfx_prop;
const auto ark_trace_prop = panda::os::unix::property::ark_trace_prop;

const auto GetPropertyBuffer = panda::os::unix::property::GetPropertyBuffer;
#endif  // PANDA_TARGET_MOBILE
#else
std::string GetPropertyBuffer([[maybe_unused]] const char *ark_prop);
#endif  // PANDA_TARGET_UNIX
}  // namespace panda::os::property

#endif  // PANDA_LIBPANDABASE_OS_PROPERTY_H_
