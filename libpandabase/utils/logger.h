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

#ifndef PANDA_LIBPANDABASE_UTILS_LOGGER_H_
#define PANDA_LIBPANDABASE_UTILS_LOGGER_H_

#include "macros.h"
#include "os/error.h"
#include "os/mutex.h"
#include "utils/dfx.h"

#include <cstdint>

#include <fstream>
#include <string>
#include <sstream>
#include <bitset>

#include <atomic>

namespace panda {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_COMPONENT_ELEM(D, NAME, STR) D(NAME, NAME, STR)

using FUNC_MOBILE_LOG_PRINT = int (*)(int, int, const char *, const char *, const char *);
static constexpr int LOG_ID_MAIN = 0;
extern FUNC_MOBILE_LOG_PRINT mlog_buf_print;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_COMPONENT_LIST(D)                              \
    LOG_COMPONENT_ELEM(D, ALLOC, "alloc")                  \
    LOG_COMPONENT_ELEM(D, ALLOC_OBJECT, "alloc-obj")       \
    LOG_COMPONENT_ELEM(D, ASSEMBLER, "assembler")          \
    LOG_COMPONENT_ELEM(D, CLASS_LINKER, "classlinker")     \
    LOG_COMPONENT_ELEM(D, COMMON, "common")                \
    LOG_COMPONENT_ELEM(D, GC, "gc")                        \
    LOG_COMPONENT_ELEM(D, GC_TRIGGER, "gc_trigger")        \
    LOG_COMPONENT_ELEM(D, REF_PROC, "reference_processor") \
    LOG_COMPONENT_ELEM(D, INTERPRETER, "interpreter")      \
    LOG_COMPONENT_ELEM(D, FUZZER, "fuzzer")                \
    LOG_COMPONENT_ELEM(D, PANDAFILE, "pandafile")          \
    LOG_COMPONENT_ELEM(D, MEMORYPOOL, "memorypool")        \
    LOG_COMPONENT_ELEM(D, RUNTIME, "runtime")              \
    LOG_COMPONENT_ELEM(D, TRACE, "trace")                  \
    LOG_COMPONENT_ELEM(D, DPROF, "dprof")                  \
    LOG_COMPONENT_ELEM(D, ECMASCRIPT, "ecmascript")        \
    LOG_COMPONENT_ELEM(D, DEBUGGER, "debugger")            \
    LOG_COMPONENT_ELEM(D, TOOLING, "tooling")              \
    LOG_COMPONENT_ELEM(D, INTEROP, "interop")              \
    LOG_COMPONENT_ELEM(D, VERIFIER, "verifier")            \
    LOG_COMPONENT_ELEM(D, DISASSEMBLER, "disassembler")    \
    LOG_COMPONENT_ELEM(D, ZIPARCHIVE, "ziparchive")        \
    LOG_COMPONENT_ELEM(D, EVENTS, "events")                \
    LOG_COMPONENT_ELEM(D, DFX, "dfx")                      \
    LOG_COMPONENT_ELEM(D, SCHEDULER, "scheduler")

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_LEVEL_LIST(D)            \
    D(FATAL, 0x00, "F", "fatal")     \
    D(ERROR, 0x01, "E", "error")     \
    D(WARNING, 0x02, "W", "warning") \
    D(INFO, 0x03, "I", "info")       \
    D(DEBUG, 0x04, "D", "debug")

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_DFX_COMPONENT_LIST(D) \
    D(COMMON, 0x00, "common")     \
    D(SIGNAL, 0x01, "signal")

namespace base_options {
class Options;
}  // namespace base_options

class Logger {
public:
    enum Component : uint32_t {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str) e,
        LOG_COMPONENT_LIST(D)
#undef D
            LAST
    };

    using ComponentMask = std::bitset<Component::LAST>;

    enum class Level : uint8_t {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, short_str, str) e = v,
        LOG_LEVEL_LIST(D)
#undef D
    };

    enum class LogDfxComponent : uint8_t {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str) e = v,
        LOG_DFX_COMPONENT_LIST(D)
#undef D
    };

    enum PandaLog2MobileLog : int {
        UNKNOWN = 0,
        DEFAULT,
        VERBOSE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        SILENT,
    };

    class Message {
    public:
        Message(Level level, Component component, bool print_system_error)
            : level_(level), component_(component), print_system_error_(print_system_error)
        {
        }

        ~Message();

        std::ostream &GetStream()
        {
            return stream_;
        }

    private:
        Level level_;
        Component component_;
        bool print_system_error_;
        std::ostringstream stream_;

        NO_COPY_SEMANTIC(Message);
        NO_MOVE_SEMANTIC(Message);
    };

    static void Initialize(const base_options::Options &options);

    static void InitializeFileLogging(const std::string &log_file, Level level, ComponentMask component_mask);

    static void InitializeStdLogging(Level level, ComponentMask component_mask);

    static void InitializeDummyLogging(Level level = Level::DEBUG, ComponentMask component_mask = 0);

    static void Destroy();

    static void SetMobileLogPrintEntryPointByPtr(void *mlog_buf_print_ptr)
    {
        mlog_buf_print = reinterpret_cast<FUNC_MOBILE_LOG_PRINT>(mlog_buf_print_ptr);
    }

    static uint32_t GetLevelNumber(Logger::Level level)
    {
        switch (level) {
            // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, short_str, str) \
    case Logger::Level::e:      \
        return v;
            LOG_LEVEL_LIST(D)
#undef D
            default:
                break;
        };

        UNREACHABLE();
        return 0;
    }

    void WriteMobileLog(Level level, const char *component, const char *message)
    {
        if (mlog_buf_print == nullptr || !is_mlog_opened_) {
            return;
        }
        PandaLog2MobileLog mlog_level = PandaLog2MobileLog::UNKNOWN;
        switch (level) {
            case Level::DEBUG:
                mlog_level = PandaLog2MobileLog::DEBUG;
                break;
            case Level::INFO:
                mlog_level = PandaLog2MobileLog::INFO;
                break;
            case Level::ERROR:
                mlog_level = PandaLog2MobileLog::ERROR;
                break;
            case Level::FATAL:
                mlog_level = PandaLog2MobileLog::FATAL;
                break;
            case Level::WARNING:
                mlog_level = PandaLog2MobileLog::WARN;
                break;
            default:
                UNREACHABLE();
        }
        std::string panda_component = "Ark " + std::string(component);
        mlog_buf_print(LOG_ID_MAIN, mlog_level, panda_component.c_str(), "%s", message);
    }

    static bool IsLoggingOn(Level level, Component component)
    {
        return IsInitialized() && level <= logger->level_ && (logger->component_mask_.test(component));
    }

    static bool IsLoggingOnOrAbort(Level level, Component component)
    {
        if (IsLoggingOn(level, component)) {
            return true;
        }

        if (level == Level::FATAL) {
            std::abort();
        }

        return false;
    }

    static bool IsLoggingDfxOn()
    {
        if (!DfxController::IsInitialized() || !IsInitialized()) {
            return false;
        }
        return (DfxController::GetOptionValue(DfxOptionHandler::DFXLOG) == 1);
    }

    static void Log(Level level, Component component, const std::string &str);

    static Level LevelFromString(std::string_view s);

    static ComponentMask ComponentMaskFromString(std::string_view s);

    static std::string StringfromDfxComponent(LogDfxComponent dfx_component);

    static void SetLevel(Level level)
    {
        ASSERT(IsInitialized());
        logger->level_ = level;
    }

    static Level GetLevel()
    {
        ASSERT(IsInitialized());
        return logger->level_;
    }

    static void EnableComponent(Component component)
    {
        ASSERT(IsInitialized());
        logger->component_mask_.set(component);
    }

    static void EnableComponent(ComponentMask component)
    {
        ASSERT(IsInitialized());
        logger->component_mask_ |= component;
    }

    static void DisableComponent(Component component)
    {
        ASSERT(IsInitialized());
        logger->component_mask_.reset(component);
    }

    static void ResetComponentMask()
    {
        ASSERT(IsInitialized());
        logger->component_mask_.reset();
    }

    static void SetMobileLogOpenFlag(bool is_mlog_opened)
    {
        ASSERT(IsInitialized());
        logger->is_mlog_opened_ = is_mlog_opened;
    }

    static bool IsInLevelList(std::string_view s);

    static bool IsInComponentList(std::string_view s);

    static void ProcessLogLevelFromString(std::string_view s);

    static void ProcessLogComponentsFromString(std::string_view s);

    static bool IsInitialized()
    {
        return logger != nullptr;
    }

protected:
    Logger(Level level, ComponentMask component_mask) : level_(level), component_mask_(component_mask) {}

    virtual void LogLineInternal(Level level, Component component, const std::string &str) = 0;

    virtual ~Logger() = default;

    static Logger *logger;

    static os::memory::Mutex mutex;

private:
    Level level_;
    ComponentMask component_mask_;
    bool is_mlog_opened_ {true};

    NO_COPY_SEMANTIC(Logger);
    NO_MOVE_SEMANTIC(Logger);
};

static Logger::ComponentMask LoggerComponentMaskAll = ~Logger::ComponentMask();

class FileLogger : public Logger {
protected:
    FileLogger(std::ofstream &&stream, Level level, ComponentMask component_mask)
        : Logger(level, component_mask), stream_(std::forward<std::ofstream>(stream))
    {
    }

    void LogLineInternal(Level level, Component component, const std::string &str) override;

    ~FileLogger() override = default;

    NO_COPY_SEMANTIC(FileLogger);
    NO_MOVE_SEMANTIC(FileLogger);

private:
    std::ofstream stream_;

    friend Logger;
};

class StderrLogger : public Logger {
private:
    StderrLogger(Level level, ComponentMask component_mask) : Logger(level, component_mask) {}

    void LogLineInternal(Level level, Component component, const std::string &str) override;

    friend Logger;

    ~StderrLogger() override = default;

    NO_COPY_SEMANTIC(StderrLogger);
    NO_MOVE_SEMANTIC(StderrLogger);
};

class DummyLogger : public Logger {
private:
    DummyLogger(Level level, ComponentMask component_mask) : Logger(level, component_mask) {}

    void LogLineInternal([[maybe_unused]] Level level, [[maybe_unused]] Component component,
                         [[maybe_unused]] const std::string &str) override
    {
    }

    friend Logger;

    ~DummyLogger() override = default;

    NO_COPY_SEMANTIC(DummyLogger);
    NO_MOVE_SEMANTIC(DummyLogger);
};

class DummyStream {
public:
    explicit operator bool() const
    {
        return true;
    }
};

template <class T>
DummyStream operator<<(DummyStream s, [[maybe_unused]] const T &v)
{
    return s;
}

class LogOnceHelper {
public:
    bool IsFirstCall()
    {
        flag_ >>= 1U;
        return flag_ != 0;
    }

private:
    uint8_t flag_ = 0x03;
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_ONCE_HELPER() static LogOnceHelper MERGE_WORDS(log_once_helper, __LINE__);

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_ONCE(level, component) \
    LOG_ONCE_HELPER()              \
    MERGE_WORDS(log_once_helper, __LINE__).IsFirstCall() && LOG(level, component)

// Explicit namespace is specified to allow using the logger out of panda namespace.
// For exmaple, in the main function.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG(level, component, p)                                                                          \
    panda::Logger::IsLoggingOnOrAbort(panda::Logger::Level::level, panda::Logger::Component::component) && \
        panda::Logger::Message(panda::Logger::Level::level, panda::Logger::Component::component, p).GetStream()

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG(level, component) _LOG_##level(component, false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_DFX(dfx_component)                                                                     \
    panda::Logger::IsLoggingDfxOn() &&                                                             \
        panda::Logger::Message(panda::Logger::Level::ERROR, panda::Logger::DFX, false).GetStream() \
            << panda::Logger::StringfromDfxComponent(panda::Logger::LogDfxComponent::dfx_component) << " log:"

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GET_LOG_STREAM(level, component) \
    panda::Logger::Message(panda::Logger::Level::level, panda::Logger::Component::component, false).GetStream()

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PLOG(level, component) _LOG_##level(component, true)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_IF(cond, level, component) (cond) && LOG(level, component)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PLOG_IF(cond, level, component) (cond) && PLOG(level, component)

#ifndef NDEBUG

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_DEBUG(component, p) _LOG(DEBUG, component, p)

#else

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_DEBUG(component, p) false && panda::DummyStream()

#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_INFO(component, p) _LOG(INFO, component, p)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_WARNING(component, p) _LOG(WARNING, component, p)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_ERROR(component, p) _LOG(ERROR, component, p)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define _LOG_FATAL(component, p) _LOG(FATAL, component, p)

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_LOGGER_H_
