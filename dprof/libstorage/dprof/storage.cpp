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

#include "storage.h"
#include "serializer/serializer.h"
#include "utils/logger.h"

#include <dirent.h>
#include <fstream>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace panda::dprof {
/* static */
std::unique_ptr<AppData> AppData::CreateByParams(const std::string &name, uint64_t hash, uint32_t pid,
                                                 FeaturesMap &&featuresMap)
{
    std::unique_ptr<AppData> appData(new AppData);

    appData->common_info_.name = name;
    appData->common_info_.hash = hash;
    appData->common_info_.pid = pid;
    appData->features_map_ = std::move(featuresMap);

    return appData;
}

/* static */
std::unique_ptr<AppData> AppData::CreateByBuffer(const std::vector<uint8_t> &buffer)
{
    std::unique_ptr<AppData> appData(new AppData);

    const uint8_t *data = buffer.data();
    size_t size = buffer.size();
    auto r = serializer::RawBufferToStruct<3>(data, size, appData->common_info_);  // 3
    if (!r) {
        LOG(ERROR, DPROF) << "Cannot deserialize buffer to common_info. Error: " << r.Error();
        return nullptr;
    }
    ASSERT(r.Value() <= size);
    data = serializer::ToUint8tPtr(serializer::ToUintPtr(data) + r.Value());
    size -= r.Value();

    r = serializer::BufferToType(data, size, appData->features_map_);
    if (!r) {
        LOG(ERROR, DPROF) << "Cannot deserialize features_map. Error: " << r.Error();
        return nullptr;
    }
    ASSERT(r.Value() <= size);
    size -= r.Value();
    if (size != 0) {
        LOG(ERROR, DPROF) << "Cannot deserialize all buffers, unused buffer size: " << size;
        return nullptr;
    }

    return appData;
}

bool AppData::ToBuffer(std::vector<uint8_t> &buffer) const
{
    if (!serializer::StructToBuffer<3>(common_info_, buffer)) {
        LOG(ERROR, DPROF) << "Cannot serialize common_info";
        return false;
    }
    auto ret = serializer::TypeToBuffer(features_map_, buffer);
    if (!ret) {
        LOG(ERROR, DPROF) << "Cannot serialize features_map. Error: " << ret.Error();
        return false;
    }
    return true;
}

/* static */
std::unique_ptr<AppDataStorage> AppDataStorage::Create(const std::string &storageDir, bool createDir)
{
    if (storageDir.empty()) {
        LOG(ERROR, DPROF) << "Storage directory is not set";
        return nullptr;
    }

    struct stat statBuffer {
    };
    if (::stat(storageDir.c_str(), &statBuffer) == 0) {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        if (S_ISDIR(statBuffer.st_mode)) {
            return std::unique_ptr<AppDataStorage>(new AppDataStorage(storageDir));
        }

        LOG(ERROR, DPROF) << storageDir << " is already exists and it is neither directory";
        return nullptr;
    }

    if (createDir) {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        if (::mkdir(storageDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            PLOG(ERROR, DPROF) << "mkdir() failed";
            return nullptr;
        }
        return std::unique_ptr<AppDataStorage>(new AppDataStorage(storageDir));
    }

    return nullptr;
}

bool AppDataStorage::SaveAppData(const AppData &appData)
{
    std::vector<uint8_t> buffer;
    if (!appData.ToBuffer(buffer)) {
        LOG(ERROR, DPROF) << "Cannot serialize AppData to buffer";
        return false;
    }

    // Save buffer to file
    std::string fileName = MakeAppPath(appData.GetName(), appData.GetHash(), appData.GetPid());
    std::ofstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        LOG(ERROR, DPROF) << "Cannot open file: " << fileName;
        return false;
    }
    file.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
    if (file.bad()) {
        LOG(ERROR, DPROF) << "Cannot write AppData to file: " << fileName;
        return false;
    }

    LOG(DEBUG, DPROF) << "Save AppData to file: " << fileName;
    return true;
}

struct dirent *DoReadDir(DIR *dirp)
{
    errno = 0;
    return ::readdir(dirp);
}

void AppDataStorage::ForEachApps(const std::function<bool(std::unique_ptr<AppData> &&)> &callback) const
{
    using UniqueDir = std::unique_ptr<DIR, void (*)(DIR *)>;
    UniqueDir dir(::opendir(storage_dir_.c_str()), [](DIR *directory) {
        if (::closedir(directory) == -1) {
            PLOG(FATAL, DPROF) << "closedir() failed";
        }
    });
    if (dir.get() == nullptr) {
        PLOG(FATAL, DPROF) << "opendir() failed, dir=" << storage_dir_;
        return;
    }

    struct dirent *ent;
    while ((ent = DoReadDir(dir.get())) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            // Skip a valid name
            continue;
        }

        if (ent->d_type != DT_REG) {
            LOG(ERROR, DPROF) << "Not a regular file: " << ent->d_name;
            continue;
        }

        std::string path = storage_dir_ + "/" + ent->d_name;
        struct stat statbuf {
        };
        if (stat(path.c_str(), &statbuf) == -1) {
            PLOG(ERROR, DPROF) << "stat() failed, path=" << path;
            continue;
        }

        size_t fileSize = statbuf.st_size;
        if (fileSize > MAX_BUFFER_SIZE) {
            LOG(ERROR, DPROF) << "File is too large: " << path;
            continue;
        }

        // Read file
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            LOG(ERROR, DPROF) << "Cannot open file: " << path;
            continue;
        }
        std::vector<uint8_t> buffer;
        buffer.reserve(fileSize);
        buffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        auto appData = AppData::CreateByBuffer(buffer);
        if (!appData) {
            LOG(ERROR, DPROF) << "Cannot deserialize file: " << path;
            continue;
        }

        if (!callback(std::move(appData))) {
            break;
        }
    }
}

std::string AppDataStorage::MakeAppPath(const std::string &name, uint64_t hash, uint32_t pid) const
{
    ASSERT(!storage_dir_.empty());
    ASSERT(!name.empty());

    std::stringstream str;
    str << storage_dir_ << "/" << name << "@" << pid << "@" << hash;
    return str.str();
}
}  // namespace panda::dprof
