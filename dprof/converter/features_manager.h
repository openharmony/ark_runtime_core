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

#ifndef PANDA_DPROF_CONVERTER_FEATURES_MANAGER_H_
#define PANDA_DPROF_CONVERTER_FEATURES_MANAGER_H_

#include "dprof/storage.h"
#include "utils/logger.h"

#include <vector>
#include <unordered_map>
#include <functional>

namespace panda::dprof {
class FeaturesManager {
public:
    struct Functor {
        virtual ~Functor() = default;
        virtual bool operator()(const AppData &appData, const std::vector<uint8_t> &data) = 0;
    };

    bool RegisterFeature(const std::string &featureName, Functor &functor)
    {
        auto it = map_.find(featureName);
        if (it != map_.end()) {
            LOG(ERROR, DPROF) << "Feature already exists, featureName=" << featureName;
            return false;
        }
        map_.insert({featureName, functor});
        return true;
    }

    bool UnregisterFeature(const std::string &featureName)
    {
        if (map_.erase(featureName) != 1) {
            LOG(ERROR, DPROF) << "Feature does not exist, featureName=" << featureName;
            return false;
        }
        return true;
    }

    bool ProcessingFeature(const AppData &appData, const std::string &featureName,
                           const std::vector<uint8_t> &data) const
    {
        auto it = map_.find(featureName);
        if (it == map_.end()) {
            LOG(ERROR, DPROF) << "Feature is not supported, featureName=" << featureName;
            return false;
        }

        return it->second(appData, data);
    }

    bool ProcessingFeatures(const AppData &appData) const
    {
        for (const auto &it : appData.GetFeaturesMap()) {
            if (!ProcessingFeature(appData, it.first, it.second)) {
                LOG(ERROR, DPROF) << "Cannot processing feature: " << it.first << ", app: " << appData.GetName();
                return false;
            }
        }
        return true;
    }

private:
    std::unordered_map<std::string, Functor &> map_;
};
}  // namespace panda::dprof

#endif  // PANDA_DPROF_CONVERTER_FEATURES_MANAGER_H_
