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

#include "profiling_data.h"
#include "utils/logger.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_unix_socket.h"
#include "ipc/ipc_message_protocol.h"
#include "serializer/serializer.h"

namespace panda::dprof {
bool ProfilingData::SetFeatureDate(const std::string &featureName, std::vector<uint8_t> &&data)
{
    auto it = features_data_map_.find(featureName);
    if (it != features_data_map_.end()) {
        LOG(ERROR, DPROF) << "Feature already exists, featureName=" << featureName;
        return false;
    }

    features_data_map_.emplace(std::pair(featureName, std::move(data)));
    return false;
}

bool ProfilingData::DumpAndResetFeatures()
{
    os::unique_fd::UniqueFd sock(ipc::CreateUnixClientSocket());
    if (!sock.IsValid()) {
        LOG(ERROR, DPROF) << "Cannot create client socket";
        return false;
    }

    ipc::protocol::Version tmp {ipc::protocol::VERSION};

    std::vector<uint8_t> versionData;
    serializer::StructToBuffer<ipc::protocol::VERSION_FCOUNT>(tmp, versionData);

    ipc::Message msgVersion(ipc::Message::Id::VERSION, std::move(versionData));
    if (!SendMessage(sock.Get(), msgVersion)) {
        LOG(ERROR, DPROF) << "Cannot send version";
        return false;
    }

    ipc::protocol::AppInfo tmp2 {app_name_, hash_, pid_};
    std::vector<uint8_t> appInfoData;
    serializer::StructToBuffer<ipc::protocol::APP_INFO_FCOUNT>(tmp2, appInfoData);

    ipc::Message msgAppInfo(ipc::Message::Id::APP_INFO, std::move(appInfoData));
    if (!SendMessage(sock.Get(), msgAppInfo)) {
        LOG(ERROR, DPROF) << "Cannot send app info";
        return false;
    }

    // Send features data
    for (auto &kv : features_data_map_) {
        ipc::protocol::FeatureData tmp_data;
        tmp_data.name = kv.first;
        tmp_data.data = std::move(kv.second);

        std::vector<uint8_t> featureData;
        serializer::StructToBuffer<ipc::protocol::FEATURE_DATA_FCOUNT>(tmp_data, featureData);

        ipc::Message msgFeatureData(ipc::Message::Id::FEATURE_DATA, std::move(featureData));
        if (!SendMessage(sock.Get(), msgFeatureData)) {
            LOG(ERROR, DPROF) << "Cannot send feature data, featureName=" << tmp_data.name;
            return false;
        }
    }

    features_data_map_.clear();
    return true;
}
}  // namespace panda::dprof
