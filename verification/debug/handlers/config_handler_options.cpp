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

#include "verification/debug/config/config_process.h"
#include "verification/debug/breakpoint/breakpoint_private.h"
#include "verification/debug/allowlist/allowlist_private.h"
#include "verification/debug/parser/parser.h"
#include "verification/util/struct_field.h"
#include "runtime/include/mem/panda_containers.h"

#include "literal_parser.h"

#include "verifier_messages.h"

#include "runtime/include/method.h"
#include "runtime/include/runtime.h"

#include "runtime/include/mem/panda_string.h"

#include "utils/logger.h"

#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_map>

namespace panda::verifier::debug {

namespace {

template <typename M>
PandaString GetKeys(const M &map)
{
    PandaString keys;
    for (const auto &p : map) {
        if (keys.empty()) {
            keys += "'";
            keys += p.first;
            keys += "'";
        } else {
            keys += ", '";
            keys += p.first;
            keys += "'";
        }
    }
    return keys;
}

}  // namespace

using panda::verifier::config::Section;
using BoolField = struct_field<VerificationOptions, bool>;
using Flags = PandaUnorderedMap<PandaString, BoolField>;
using FlagsSection = PandaUnorderedMap<PandaString, Flags>;

static bool Verify(const Section &section, const FlagsSection &flags)
{
    auto &verif_opts = Runtime::GetCurrent()->GetVerificationOptions();
    for (const auto &s : section.sections) {
        if (flags.count(s.name) == 0) {
            LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTIONS_SECTION(s.name, GetKeys(flags));
            return false;
        }
        const auto &section_flags = flags.at(s.name);
        for (const auto &i : s.items) {
            std::vector<PandaString> c;
            const char *start = i.c_str();
            const char *end = i.c_str() + i.length();  // NOLINT

            if (!LiteralsParser<std::vector, PandaString>()(c, start, end)) {
                LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTIONS_LINE(i);
                return false;
            }

            if (c.empty()) {
                continue;
            }

            for (const auto &l : c) {
                if (section_flags.count(l) == 0) {
                    LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTION_FOR_SECTION(l, s.name, GetKeys(section_flags));
                    return false;
                }
                section_flags.at(l).of(verif_opts) = true;
                LOG_VERIFIER_DEBUG_CONFIG_OPTION_IS_ACTIVE_INFO(s.name, l);
            }
        }
    }
    return true;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
void RegisterConfigHandlerOptions()
{
    static const auto CONFIG_DEBUG_OPTIONS_VERIFIER = [](const Section &section) {
        const FlagsSection FLAGS = {
            {"show",
             {{"context", BoolField {offsetof(VerificationOptions, Debug.Show.Context)}},
              {"reg-changes", BoolField {offsetof(VerificationOptions, Debug.Show.RegChanges)}},
              {"typesystem", BoolField {offsetof(VerificationOptions, Debug.Show.TypeSystem)}}}},
            {"allow",
             {{"undefined-class", BoolField {offsetof(VerificationOptions, Debug.Allow.UndefinedClass)}},
              {"undefined-method", BoolField {offsetof(VerificationOptions, Debug.Allow.UndefinedMethod)}},
              {"undefined-field", BoolField {offsetof(VerificationOptions, Debug.Allow.UndefinedField)}},
              {"undefined-type", BoolField {offsetof(VerificationOptions, Debug.Allow.UndefinedType)}},
              {"undefined-string", BoolField {offsetof(VerificationOptions, Debug.Allow.UndefinedString)}},
              {"method-access-violation", BoolField {offsetof(VerificationOptions, Debug.Allow.MethodAccessViolation)}},
              {"field-access-violation", BoolField {offsetof(VerificationOptions, Debug.Allow.FieldAccessViolation)}},
              {"wrong-subclassing-in-method-args",
               BoolField {offsetof(VerificationOptions, Debug.Allow.WrongSubclassingInMethodArgs)}},
              {"error-in-exception-handler",
               BoolField {offsetof(VerificationOptions, Debug.Allow.ErrorInExceptionHandler)}},
              {"permanent-runtime-exception",
               BoolField {offsetof(VerificationOptions, Debug.Allow.PermanentRuntimeException)}}}}};

        return Verify(section, FLAGS);
    };

    config::RegisterConfigHandler("config.debug.options.verifier", CONFIG_DEBUG_OPTIONS_VERIFIER);
}

}  // namespace panda::verifier::debug
