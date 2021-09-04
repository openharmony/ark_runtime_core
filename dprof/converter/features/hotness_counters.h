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

#ifndef PANDA_DPROF_CONVERTER_FEATURES_HOTNESS_COUNTERS_H_
#define PANDA_DPROF_CONVERTER_FEATURES_HOTNESS_COUNTERS_H_

#include "macros.h"
#include "features_manager.h"
#include "dprof/storage.h"
#include "utils/logger.h"
#include "serializer/serializer.h"

#include <list>

namespace panda::dprof {
static const char HCOUNTERS_FEATURE_NAME[] = "hotness_counters.v1";

class HCountersFunctor : public FeaturesManager::Functor {
    struct HCountersInfo {
        struct MethodInfo {
            std::string name;
            uint32_t value;
        };
        std::string app_name;
        uint64_t hash;
        uint32_t pid;
        std::list<MethodInfo> methods_list;
    };

public:
    explicit HCountersFunctor(std::ostream &out) : out_(out) {}
    ~HCountersFunctor() = default;

    bool operator()(const AppData &appData, const std::vector<uint8_t> &data) override
    {
        std::unordered_map<std::string, uint32_t> methodInfoMap;
        if (!serializer::BufferToType(data.data(), data.size(), methodInfoMap)) {
            LOG(ERROR, DPROF) << "Cannot deserialize methodInfoMap";
            return false;
        }

        std::list<HCountersInfo::MethodInfo> methodsList;
        for (auto &it : methodInfoMap) {
            methodsList.emplace_back(HCountersInfo::MethodInfo {std::move(it.first), it.second});
        }

        hcounters_info_list_.emplace_back(
            HCountersInfo {appData.GetName(), appData.GetHash(), appData.GetPid(), std::move(methodsList)});

        return true;
    }

    bool ShowInfo(const std::string &format)
    {
        if (hcounters_info_list_.empty()) {
            return false;
        }

        if (format == "text") {
            ShowText();
        } else if (format == "json") {
            ShowJson();
        } else {
            LOG(ERROR, DPROF) << "Unknown format: " << format << std::endl;
            return false;
        }
        return true;
    }

private:
    void ShowText()
    {
        out_ << "Feature: " << HCOUNTERS_FEATURE_NAME << std::endl;
        for (auto &hcounters_info : hcounters_info_list_) {
            out_ << "  app: name=" << hcounters_info.app_name << " pid=" << hcounters_info.pid
                 << " hash=" << hcounters_info.hash << std::endl;

            for (auto &method_info : hcounters_info.methods_list) {
                out_ << "    " << method_info.name << ":" << method_info.value << std::endl;
            }
        }
    }

    void ShowJson()
    {
        out_ << "{" << std::endl;
        out_ << "  \"" << HCOUNTERS_FEATURE_NAME << "\": [" << std::endl;
        for (auto &hcounters_info : hcounters_info_list_) {
            out_ << "    {" << std::endl;
            out_ << "      \"app_name\": \"" << hcounters_info.app_name << "\"," << std::endl;
            out_ << "      \"pid\": \"" << hcounters_info.pid << "\"," << std::endl;
            out_ << "      \"hash\": \"" << hcounters_info.hash << "\"," << std::endl;
            out_ << "      \"counters\": [" << std::endl;
            for (auto &method_info : hcounters_info.methods_list) {
                out_ << "        {" << std::endl;
                out_ << "          \"name\": \"" << method_info.name << "\"," << std::endl;
                out_ << "          \"value\": \"" << method_info.value << "\"" << std::endl;
                out_ << "        }";
                if (&method_info != &hcounters_info.methods_list.back()) {
                    out_ << ",";
                }
                out_ << std::endl;
            }
            out_ << "      ]" << std::endl;
            out_ << "    }";
            if (&hcounters_info != &hcounters_info_list_.back()) {
                out_ << ",";
            }
            out_ << std::endl;
        }
        out_ << "  ]" << std::endl;
        out_ << "}" << std::endl;
    }

    std::list<HCountersInfo> hcounters_info_list_;
    std::ostream &out_;

    NO_COPY_SEMANTIC(HCountersFunctor);
    NO_MOVE_SEMANTIC(HCountersFunctor);
};
}  // namespace panda::dprof

#endif  // PANDA_DPROF_CONVERTER_FEATURES_HOTNESS_COUNTERS_H_
