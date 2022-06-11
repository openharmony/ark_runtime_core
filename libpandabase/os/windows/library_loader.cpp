/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <windows.h>

namespace panda::os::library_loader {
Expected<LibraryHandle, Error> Load(std::string_view filename)
{
    HMODULE module = LoadLibrary(filename.data());
    void *handle = reinterpret_cast<void *>(module);
    if (handle != nullptr) {
        return LibraryHandle(handle);
    }
    return Unexpected(Error(std::string("Failed to load library ") + filename.data() + std::string(", error code ") +
                            std::to_string(GetLastError())));
}

Expected<void *, Error> ResolveSymbol(const LibraryHandle &handle, std::string_view name)
{
    HMODULE module = reinterpret_cast<HMODULE>(handle.GetNativeHandle());
    void *p = reinterpret_cast<void *>(GetProcAddress(module, name.data()));
    if (p != nullptr) {
        return p;
    }
    return Unexpected(Error(std::string("Failed to resolve symbol ") + name.data() + std::string(", error code ") +
                            std::to_string(GetLastError())));
}

void CloseHandle(void *handle)
{
    if (handle != nullptr) {
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
    }
}
}  // namespace panda::os::library_loader
