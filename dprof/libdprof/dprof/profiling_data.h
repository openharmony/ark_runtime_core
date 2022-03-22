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

#ifndef PANDA_DPROF_LIBDPROF_DPROF_PROFILING_DATA_H_
#define PANDA_DPROF_LIBDPROF_DPROF_PROFILING_DATA_H_

#include <vector>
#include <string>
#include <unordered_map>

namespace panda::dprof {
class ProfilingData {
public:
    ProfilingData(const std::string &appName, uint64_t hash, uint32_t pid) : app_name_(appName), hash_(hash), pid_(pid)
    {
    }
    ~ProfilingData() = default;

    bool SetFeatureDate(const std::string &featureName, std::vector<uint8_t> &&data);
    bool DumpAndResetFeatures();

private:
    std::string app_name_;
    uint64_t hash_;
    uint32_t pid_;

    using FeaturesDataMap = std::unordered_map<std::string, std::vector<uint8_t>>;
    FeaturesDataMap features_data_map_;
};

}  // namespace panda::dprof

#endif  // PANDA_DPROF_LIBDPROF_DPROF_PROFILING_DATA_H_
