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

#include "logger.h"
#include "os/thread.h"
#include "string_helpers.h"
#include "generated/base_options.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <string_view>

namespace panda {

Logger *Logger::logger = nullptr;
os::memory::Mutex Logger::mutex;  // NOLINT(fuchsia-statically-constructed-objects)
FUNC_MOBILE_LOG_PRINT mlog_buf_print = nullptr;

void Logger::Initialize(const base_options::Options &options)
{
    panda::Logger::ComponentMask component_mask;
    for (const auto &s : options.GetLogComponents()) {
        component_mask |= Logger::ComponentMaskFromString(s);
    }

    if (options.GetLogStream() == "std") {
        Logger::InitializeStdLogging(Logger::LevelFromString(options.GetLogLevel()), component_mask);
    } else if (options.GetLogStream() == "file" || options.GetLogStream() == "fast-file") {
        const std::string &file_name = options.GetLogFile();
        Logger::InitializeFileLogging(file_name, Logger::LevelFromString(options.GetLogLevel()), component_mask);
    } else if (options.GetLogStream() == "dummy") {
        Logger::InitializeDummyLogging(Logger::LevelFromString(options.GetLogLevel()), component_mask);
    } else {
        UNREACHABLE();
    }
}

Logger::Message::~Message()
{
    if (print_system_error_) {
        stream_ << ": " << os::Error(errno).ToString();
    }

    Logger::Log(level_, component_, stream_.str());

    if (level_ == Level::FATAL) {
        std::abort();
    }
}

static const char *GetComponentTag(Logger::Component component)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str)                         \
    if (component == Logger::Component::e) { \
        return str;                          \
    }
    LOG_COMPONENT_LIST(D)
#undef D

    UNREACHABLE();
}

/* static */
void Logger::Log(Level level, Component component, const std::string &str)
{
    if (!IsLoggingOn(level, component)) {
        return;
    }

    os::memory::LockHolder<os::memory::Mutex> lock(mutex);
    if (!IsLoggingOn(level, component)) {
        return;
    }

    size_t nl = str.find('\n');
    if (nl == std::string::npos) {
        logger->LogLineInternal(level, component, str);
        logger->WriteMobileLog(level, GetComponentTag(component), str.c_str());
    } else {
        size_t i = 0;
        while (nl != std::string::npos) {
            std::string line = str.substr(i, nl - i);
            logger->LogLineInternal(level, component, line);
            logger->WriteMobileLog(level, GetComponentTag(component), line.c_str());
            i = nl + 1;
            nl = str.find('\n', i);
        }

        logger->LogLineInternal(level, component, str.substr(i));
        logger->WriteMobileLog(level, GetComponentTag(component), str.substr(i).c_str());
    }
}

static const char *GetLevelTag(Logger::Level level)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, short_str, str)      \
    if (level == Logger::Level::e) { \
        return short_str;            \
    }
    LOG_LEVEL_LIST(D)
#undef D

    UNREACHABLE();
}

/* static */
std::string GetPrefix(Logger::Level level, Logger::Component component)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return helpers::string::Format("[TID %06x] %s/%s: ", os::thread::GetCurrentThreadId(), GetLevelTag(level),
                                   GetComponentTag(component));
}

/* static */
void Logger::InitializeFileLogging(const std::string &log_file, Level level, ComponentMask component_mask)
{
    if (IsInitialized()) {
        return;
    }

    os::memory::LockHolder<os::memory::Mutex> lock(mutex);

    if (IsInitialized()) {
        return;
    }

    std::ofstream stream(log_file);
    if (stream) {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        logger = new FileLogger(std::move(stream), level, component_mask);
    } else {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        logger = new StderrLogger(level, component_mask);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        std::string msg = helpers::string::Format("Fallback to stderr logging: cannot open log file '%s': %s",
                                                  log_file.c_str(), os::Error(errno).ToString().c_str());
        logger->LogLineInternal(Level::ERROR, Component::COMMON, msg);
    }
#ifdef PANDA_TARGET_UNIX
    if (DfxController::IsInitialized() && DfxController::GetOptionValue(DfxOptionHandler::MOBILE_LOG) == 0) {
        Logger::SetMobileLogOpenFlag(false);
    }
#endif
}

/* static */
void Logger::InitializeStdLogging(Level level, ComponentMask component_mask)
{
    if (IsInitialized()) {
        return;
    }

    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        if (IsInitialized()) {
            return;
        }

        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        logger = new StderrLogger(level, component_mask);
#ifdef PANDA_TARGET_UNIX
        if (DfxController::IsInitialized() && DfxController::GetOptionValue(DfxOptionHandler::MOBILE_LOG) == 0) {
            Logger::SetMobileLogOpenFlag(false);
        }
#endif
    }
}

/* static */
void Logger::InitializeDummyLogging(Level level, ComponentMask component_mask)
{
    if (IsInitialized()) {
        return;
    }

    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        if (IsInitialized()) {
            return;
        }

        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        logger = new DummyLogger(level, component_mask);
    }
}

/* static */
void Logger::Destroy()
{
    if (!IsInitialized()) {
        return;
    }

    Logger *l = nullptr;

    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        if (!IsInitialized()) {
            return;
        }

        l = logger;
        logger = nullptr;
    }

    delete l;
}

/* static */
Logger::Level Logger::LevelFromString(std::string_view s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, short_str, str)  \
    if (s == str) {              \
        return Logger::Level::e; \
    }
    LOG_LEVEL_LIST(D)
#undef D

    UNREACHABLE();
}

/* static */
Logger::ComponentMask Logger::ComponentMaskFromString(std::string_view s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str)                                                     \
    if (s == str) {                                                      \
        return panda::Logger::ComponentMask().set(Logger::Component::e); \
    }
    LOG_COMPONENT_LIST(D)
#undef D

    if (s == "all") {
        return panda::LoggerComponentMaskAll;
    }

    UNREACHABLE();
}

/* static */
std::string Logger::StringfromDfxComponent(LogDfxComponent dfx_component)
{
    switch (dfx_component) {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str)                 \
    case Logger::LogDfxComponent::e: \
        return str;
        LOG_DFX_COMPONENT_LIST(D)  // CODECHECK-NOLINT(C_RULE_ID_SWITCH_INDENTATION)
#undef D
        default:
            break;
    }
    UNREACHABLE();
}

/* static */
bool Logger::IsInLevelList(std::string_view s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, short_str, str) \
    if (s == str) {             \
        return true;            \
    }
    LOG_LEVEL_LIST(D)
#undef D
    return false;
}

/* static */
bool Logger::IsInComponentList(std::string_view s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str) \
    if (s == str) {  \
        return true; \
    }
    LOG_COMPONENT_LIST(D)
#undef D
    if (s == "all") {
        return true;
    }
    return false;
}

/* static */
void Logger::ProcessLogLevelFromString(std::string_view s)
{
    if (Logger::IsInLevelList(s)) {
        Logger::SetLevel(Logger::LevelFromString(s));
    } else {
        LOG(ERROR, RUNTIME) << "Unknown level " << s;
    }
}

/* static */
void Logger::ProcessLogComponentsFromString(std::string_view s)
{
    Logger::ResetComponentMask();
    size_t last_pos = s.find_first_not_of(',', 0);
    size_t pos = s.find(',', last_pos);
    while (last_pos != std::string_view::npos) {
        std::string_view component_str = s.substr(last_pos, pos - last_pos);
        last_pos = s.find_first_not_of(',', pos);
        pos = s.find(',', last_pos);
        if (Logger::IsInComponentList(component_str)) {
            Logger::EnableComponent(Logger::ComponentMaskFromString(component_str));
        } else {
            LOG(ERROR, RUNTIME) << "Unknown component " << component_str;
        }
    }
}

void FileLogger::LogLineInternal(Level level, Component component, const std::string &str)
{
    std::string prefix = GetPrefix(level, component);
    stream_ << prefix << str << std::endl << std::flush;
}

void StderrLogger::LogLineInternal(Level level, Component component, const std::string &str)
{
    std::string prefix = GetPrefix(level, component);
    std::cerr << prefix << str << std::endl << std::flush;
}

}  // namespace panda
