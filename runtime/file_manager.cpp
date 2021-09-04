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

#include "runtime/include/file_manager.h"
#include "runtime/include/runtime.h"
#include "libpandabase/os/filesystem.h"

namespace panda {

bool FileManager::LoadAbcFile(const PandaString &location, panda_file::File::OpenMode open_mode)
{
    auto pf = panda_file::OpenPandaFile(location, "", open_mode);
    if (pf == nullptr) {
        LOG(ERROR, PANDAFILE) << "Load panda file failed: " << location;
        return false;
    }
    auto runtime = Runtime::GetCurrent();
    runtime->GetClassLinker()->AddPandaFile(std::move(pf));
    if (Runtime::GetOptions().IsEnableAn()) {
        auto an_location = FileManager::ResolveAnFilePath(location);
        auto res = FileManager::LoadAnFile(an_location);
        if (res && res.Value()) {
            LOG(INFO, PANDAFILE) << "Found .an file for '" << location << "': '" << an_location << "'";
        } else if (!res) {
            LOG(INFO, PANDAFILE) << "Failed to load AOT file: '" << an_location << "': " << res.Error();
        } else {
            LOG(INFO, PANDAFILE) << "Failed to load '" << an_location << "' with unknown reason";
        }
    }

    return true;
}

Expected<bool, std::string> FileManager::LoadAnFile(const PandaString &an_location)
{
    return Unexpected(std::string("Cannot load file: ") + std::string(an_location) + ": AOT files unsupported");
}

PandaString FileManager::ResolveAnFilePath([[maybe_unused]] const PandaString &abc_path)
{
    return "";
}

}  // namespace panda
