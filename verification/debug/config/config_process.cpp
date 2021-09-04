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

#include "config_process.h"

#include "verification/debug/context/context.h"

#include "runtime/include/mem/panda_string.h"

#include <unordered_map>

namespace {

using panda::verifier::config::Section;
using panda::verifier::debug::DebugContext;

bool ProcessConfigSection(const Section &section, const panda::PandaString &path = "")
{
    auto &section_handlers = DebugContext::GetCurrent().Config.SectionHandlers;
    if (section_handlers.count(path) > 0) {
        return section_handlers.at(path)(section);
    }
    for (const auto &s : section.sections) {
        if (!ProcessConfigSection(s, path + "." + s.name)) {
            return false;
        }
    }
    return true;
}

}  // namespace

namespace panda::verifier::config {

void RegisterConfigHandler(const PandaString &path, callable<bool(const Section &)> handler)
{
    auto &section_handlers = DebugContext::GetCurrent().Config.SectionHandlers;
    section_handlers[path] = handler;
}

bool ProcessConfig(const Section &cfg)
{
    return ProcessConfigSection(cfg, cfg.name);
}

}  // namespace panda::verifier::config
