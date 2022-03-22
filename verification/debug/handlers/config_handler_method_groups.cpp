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

#include "verification/debug/config/config_process.h"
#include "verification/debug/parser/parser.h"
#include "verification/debug/options/method_group_parser.h"

#include "verifier_messages.h"

#include "runtime/include/mem/panda_string.h"

#include "literal_parser.h"

#include "runtime/include/method.h"
#include "runtime/include/runtime.h"

#include "utils/logger.h"

#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_map>

#include <type_traits>

namespace panda::verifier::debug {

using panda::parser::parser;
using panda::verifier::config::Section;

void RegisterConfigHandlerMethodGroups()
{
    static const auto CONFIG_DEBUG_METHOD_GROUPS_VERIFIER_OPTIONS = [](const Section &section) {
        auto &runtime = *Runtime::GetCurrent();
        auto &verif_options = runtime.GetVerificationOptions();

        for (const auto &item : section.items) {
            struct Context {
                PandaString group;
                PandaString options;
            };

            using p = parser<Context, const char, const char *>;
            const auto WS = p::of_charset(" \t");

            const auto GROUP_HANDLER = [](Context &c, PandaString &&group) {
                c.group = std::move(group);
                return true;
            };

            const auto OPTIONS_HANDLER = [](Context &c, PandaString &&options) {
                c.options = std::move(options);
                return true;
            };

            const auto LINE = ~WS >> MethodGroupParser<p, PandaString>(GROUP_HANDLER) >> WS >>
                              LiteralParser<p, PandaString>(OPTIONS_HANDLER) >> ~WS >> p::end();

            const char *start = item.c_str();
            const char *end = item.c_str() + item.length();  // NOLINT

            Context ctx;
            if (!LINE(ctx, start, end)) {
                LOG(DEBUG, VERIFIER) << "  Error: cannot parse config line '" << item << "'";
                return false;
            }

            if (!verif_options.Debug.GetMethodOptions().AddOptionsForGroup(ctx.group, ctx.options)) {
                LOG(DEBUG, VERIFIER) << "  Error: cannot set options for method group '" << ctx.group << "', options '"
                                     << ctx.options << "'";
                return false;
            }

            LOG(DEBUG, VERIFIER) << "  Set options for method group '" << ctx.group << "' : '" << ctx.options << "'";
        }

        return true;
    };

    config::RegisterConfigHandler("config.debug.method_groups.verifier.options",
                                  CONFIG_DEBUG_METHOD_GROUPS_VERIFIER_OPTIONS);
}

}  // namespace panda::verifier::debug
