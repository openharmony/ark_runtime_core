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

#include "os/unix/property.h"

#ifdef PANDA_TARGET_MOBILE
#include <sys/system_properties.h>
#endif  // PANDA_TARGET_MOBILE

namespace panda::os::unix::property {
#ifdef PANDA_TARGET_MOBILE
std::string GetPropertyBuffer(const char *ark_prop)
{
    char buf[PROP_VALUE_MAX] = {};
    if (__system_property_get(ark_prop, buf) > 0) {
        std::string prop = buf;
        return prop;
    }
    return "Not Found";
}
#endif  // PANDA_TARGET_MOBILE
}  // namespace panda::os::unix::property
