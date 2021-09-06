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

#include "os/library_loader.h"

#include <dlfcn.h>

namespace panda::os::library_loader {

Expected<LibraryHandle, Error> Load(std::string_view filename)
{
    void *handle = dlopen(filename.data(), RTLD_LAZY);
    if (handle != nullptr) {
        return LibraryHandle(handle);
    }
    char *msg = dlerror();
    return msg != nullptr ? Unexpected(Error(msg)) : Unexpected(Error("no error message"));
}

Expected<void *, Error> ResolveSymbol(const LibraryHandle &handle, std::string_view name)
{
    void *p = dlsym(handle.GetNativeHandle(), name.data());
    if (p != nullptr) {
        return p;
    }
    char *msg = dlerror();
    return msg != nullptr ? Unexpected(Error(msg)) : Unexpected(Error("no error message"));
}

}  // namespace panda::os::library_loader
