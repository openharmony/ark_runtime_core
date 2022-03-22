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
#include "verification/debug/config/config.h"
#include "verification/debug/options/msg_set_parser.h"
#include "verification/debug/config/config_parse.h"
#include "verification/debug/default_config.h"

#include "verifier_messages.h"

#include "literal_parser.h"

#include "runtime/include/method.h"
#include "runtime/include/runtime.h"

#include "runtime/include/mem/panda_string.h"
#include "runtime/include/mem/panda_containers.h"

#include "utils/logger.h"

#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_map>

#include <type_traits>

namespace {

struct Context {
    panda::PandaVector<std::pair<size_t, size_t>> stack;
    panda::PandaUnorderedSet<size_t> nums;
};

}  // namespace

namespace panda::verifier::debug {

using panda::verifier::config::Section;
using MethodOptions = VerificationOptions::MethodOptionsConfig::MethodOptions;

template <class F>
static bool ProcessSectionMsg(const Section &s, MethodOptions *options, const F &joiner)
{
    auto lines = joiner(s.items);
    const char *start = lines.c_str();
    const char *end = lines.c_str() + lines.length();  // NOLINT
    Context c;

    if (!MessageSetParser<Context, PandaString>()(c, start, end)) {
        LOG(DEBUG, VERIFIER) << "Wrong set of messages: '" << lines << "'";
        return false;
    }

    bool error = s.name == "error";
    bool warning = s.name == "warning";

    const MethodOption::MsgClass klass =
        error ? MethodOption::MsgClass::ERROR
              : warning ? MethodOption::MsgClass::WARNING : MethodOption::MsgClass::HIDDEN;
    for (const auto msg_num : c.nums) {
        options->SetMsgClass(VerifierMessageIsValid<>, msg_num, klass);
    }

    return true;
}

template <class F>
static bool ProcessSectionShow(const Section &s, MethodOptions *options, const F &joiner)
{
    auto lines = joiner(s.items);
    const char *start = lines.c_str();
    const char *end = lines.c_str() + lines.length();  // NOLINT
    PandaVector<PandaString> literals;
    if (!LiteralsParser<PandaVector, PandaString>()(literals, start, end)) {
        LOG(DEBUG, VERIFIER) << "Wrong options: '" << lines << "'";
        return false;
    }
    for (const auto &option : literals) {
        if (option == "context") {
            options->SetShow(MethodOption::InfoType::CONTEXT);
        } else if (option == "reg-changes") {
            options->SetShow(MethodOption::InfoType::REG_CHANGES);
        } else if (option == "cflow") {
            options->SetShow(MethodOption::InfoType::CFLOW);
        } else if (option == "jobfill") {
            options->SetShow(MethodOption::InfoType::JOBFILL);
        } else {
            LOG(DEBUG, VERIFIER) << "Wrong option: '" << option << "'";
            return false;
        }
    }

    return true;
}

template <class F>
static bool ProcessSectionUplevel(const Section &s, MethodOptions *options, const F &joiner)
{
    auto &runtime = *Runtime::GetCurrent();
    auto &verif_options = runtime.GetVerificationOptions();

    auto lines = joiner(s.items);
    const char *start = lines.c_str();
    const char *end = lines.c_str() + lines.length();  // NOLINT
    PandaVector<PandaString> uplevel_options;
    if (!LiteralsParser<PandaVector, PandaString>()(uplevel_options, start, end)) {
        LOG(DEBUG, VERIFIER) << "Wrong uplevel options: '" << lines << "'";
        return false;
    }
    for (const auto &uplevel : uplevel_options) {
        if (!verif_options.Debug.GetMethodOptions().IsOptionsPresent(uplevel)) {
            LOG(DEBUG, VERIFIER) << "Cannot find uplevel options: '" << uplevel << "'";
            return false;
        }
        options->AddUpLevel(verif_options.Debug.GetMethodOptions().GetOptions(uplevel));
    }

    return true;
}

template <class F>
static bool ProcessSectionCheck(const Section &s, MethodOptions *options, const F &joiner)
{
    auto lines = joiner(s.items);
    const char *start = lines.c_str();
    const char *end = lines.c_str() + lines.length();  // NOLINT
    PandaVector<PandaString> checks;
    if (!LiteralsParser<PandaVector, PandaString>()(checks, start, end)) {
        LOG(DEBUG, VERIFIER) << "Wrong checks section: '" << lines << "'";
        return false;
    }
    for (const auto &c : checks) {
        if (c == "cflow") {
            options->Check() |= MethodOption::CheckType::CFLOW;
        } else if (c == "reg-usage") {
            options->Check() |= MethodOption::CheckType::REG_USAGE;
        } else if (c == "resolve-id") {
            options->Check() |= MethodOption::CheckType::RESOLVE_ID;
        } else if (c == "typing") {
            options->Check() |= MethodOption::CheckType::TYPING;
        } else if (c == "absint") {
            options->Check() |= MethodOption::CheckType::ABSINT;
        } else {
            LOG(DEBUG, VERIFIER) << "Wrong check type: '" << c << "'";
            return false;
        }
    }

    return true;
}

const auto &MethodOptionsProcessor()
{
    static const auto PROCESS_METHOD_OPTIONS = [](const Section &section) {
        const auto &name = section.name;
        auto &runtime = *Runtime::GetCurrent();
        auto &verif_options = runtime.GetVerificationOptions();
        auto &options = verif_options.Debug.GetMethodOptions().NewOptions(name);

        for (const auto &s : section.sections) {
            bool error = s.name == "error";
            bool warning = s.name == "warning";
            bool hidden = s.name == "hidden";

            const auto joiner = [](const auto &lines) {
                std::decay_t<decltype(lines[0])> result;
                for (const auto &l : lines) {
                    result += l;
                    result += " ";
                }
                return result;
            };

            if (warning || error || hidden) {
                if (!ProcessSectionMsg(s, &options, joiner)) {
                    return false;
                }
            } else if (s.name == "show") {
                if (!ProcessSectionShow(s, &options, joiner)) {
                    return false;
                }
            } else if (s.name == "uplevel") {
                if (!ProcessSectionUplevel(s, &options, joiner)) {
                    return false;
                }
            } else if (s.name == "check") {
                if (!ProcessSectionCheck(s, &options, joiner)) {
                    return false;
                }
            } else {
                LOG(DEBUG, VERIFIER) << "Wrong section: '" << s.name << "'";
                return false;
            }
        }

        LOG(DEBUG, VERIFIER) << options.Image(VerifierMessageToString<PandaString>);

        return true;
    };

    return PROCESS_METHOD_OPTIONS;
}

void RegisterConfigHandlerMethodOptions()
{
    static const auto CONFIG_DEBUG_METHOD_OPTIONS_VERIFIER = [](const Section &section) {
        bool default_present = false;
        for (const auto &s : section.sections) {
            if (s.name == "default") {
                default_present = true;
                break;
            }
        }
        if (!default_present) {
            // take default section from inlined config
            Section cfg;
            if (!ParseConfig(panda::verifier::config::G_VERIFIER_DEBUG_DEFAULT_CONFIG, cfg)) {
                LOG(DEBUG, VERIFIER) << "Cannot parse default internal config. Internal error.";
                return false;
            }
            if (!MethodOptionsProcessor()(cfg["debug"]["method_options"]["verifier"]["default"])) {
                LOG(DEBUG, VERIFIER) << "Cannot parse default section";
                return false;
            }
        }
        for (const auto &s : section.sections) {
            if (!MethodOptionsProcessor()(s)) {
                LOG(DEBUG, VERIFIER) << "Cannot parse section '" << s.name << "'";
                return false;
            }
        }
        return true;
    };

    config::RegisterConfigHandler("config.debug.method_options.verifier", CONFIG_DEBUG_METHOD_OPTIONS_VERIFIER);
}

void SetDefaultMethodOptions()
{
    auto &runtime = *Runtime::GetCurrent();
    auto &verif_options = runtime.GetVerificationOptions();
    auto &options = verif_options.Debug.GetMethodOptions();
    if (!options.IsOptionsPresent("default")) {
        // take default section from inlined config
        Section cfg;
        if (!ParseConfig(panda::verifier::config::G_VERIFIER_DEBUG_DEFAULT_CONFIG, cfg)) {
            LOG(FATAL, VERIFIER) << "Cannot parse default internal config. Internal error.";
        }
        if (!MethodOptionsProcessor()(cfg["debug"]["method_options"]["verifier"]["default"])) {
            LOG(FATAL, VERIFIER) << "Cannot parse default section";
        }
    }
}

}  // namespace panda::verifier::debug
