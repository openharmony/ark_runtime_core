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

#ifndef PANDA_RUNTIME_INCLUDE_RUNTIME_H_
#define PANDA_RUNTIME_INCLUDE_RUNTIME_H_

#include <atomic>
#include <memory>
#include <string>
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <vector>

#include "libpandabase/mem/arena_allocator.h"
#include "libpandabase/os/mutex.h"
#include "libpandabase/os/library_loader.h"
#include "libpandabase/utils/expected.h"
#include "libpandabase/utils/dfx.h"
#include "libpandafile/file_items.h"
#include "libpandafile/literal_data_accessor.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/gc_task.h"
#include "runtime/include/tooling/debug_interface.h"
#include "runtime/signal_handler.h"
#include "runtime/mem/allocator_adapter.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_trigger.h"
#include "runtime/mem/memory_manager.h"
#include "runtime/monitor_pool.h"
#include "runtime/string_table.h"
#include "runtime/thread_manager.h"
#include "verification/verification_options.h"

namespace panda {

class DProfiler;
class ClassLinker;
class CompilerInterface;
class RuntimeController;

class PandaVM;
class RuntimeNotificationManager;
class Trace;

namespace tooling {
class PtLangExt;
class Debugger;
}  // namespace tooling

class Runtime {
public:
    using ExitHook = void (*)(int32_t status);
    using AbortHook = void (*)();

    enum class Error {
        PANDA_FILE_LOAD_ERROR,
        INVALID_ENTRY_POINT,
        CLASS_NOT_FOUND,
        CLASS_NOT_INITIALIZED,
        METHOD_NOT_FOUND,
        CLASS_LINKER_EXTENSION_NOT_FOUND
    };

    LanguageContext GetLanguageContext(const std::string &runtime_type);
    LanguageContext GetLanguageContext(const Method &method);
    LanguageContext GetLanguageContext(const Class &cls);
    LanguageContext GetLanguageContext(const BaseClass &cls);
    LanguageContext GetLanguageContext(panda_file::ClassDataAccessor *cda);
    LanguageContext GetLanguageContext(panda_file::SourceLang lang);

    static void InitializeLogger(const RuntimeOptions &options);

    static bool CreateInstance(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator,
                               const std::vector<LanguageContextBase *> &ctxs);

    PANDA_PUBLIC_API static bool Create(const RuntimeOptions &options,
                                        const std::vector<LanguageContextBase *> &ctxs = {});

    static bool DestroyUnderLockHolder();

    PANDA_PUBLIC_API static bool Destroy();

    PANDA_PUBLIC_API static Runtime *GetCurrent();

    template <typename Handler>
    static auto GetCurrentSync(Handler &&handler)
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);
        return handler(*GetCurrent());
    }

    ClassLinker *GetClassLinker() const
    {
        return class_linker_;
    }

    RuntimeNotificationManager *GetNotificationManager() const
    {
        return notification_manager_;
    }

    static const RuntimeOptions &GetOptions()
    {
        return options_;
    }

    void SetZygoteNoThreadSection(bool val)
    {
        zygote_no_threads_ = val;
    }

    coretypes::String *ResolveString(PandaVM *vm, const Method &caller, panda_file::File::EntityId id);

    coretypes::String *ResolveString(PandaVM *vm, const panda_file::File &pf, panda_file::File::EntityId id,
                                     LanguageContext ctx);

    coretypes::String *ResolveString(PandaVM *vm, const uint8_t *mutf8, uint32_t length, LanguageContext ctx);

    Class *GetClassRootForLiteralTag(const ClassLinkerExtension &ext, panda_file::LiteralTag tag) const;
    static bool GetLiteralTagAndValue(const panda_file::File &pf, panda_file::File::EntityId id,
                                      panda_file::LiteralTag *tag,
                                      panda_file::LiteralDataAccessor::LiteralValue *value);

    coretypes::Array *ResolveLiteralArray(PandaVM *vm, const Method &caller, panda_file::File::EntityId id);

    coretypes::Array *ResolveLiteralArray(PandaVM *vm, const panda_file::File &pf, panda_file::File::EntityId id,
                                          LanguageContext ctx);

    void PreZygoteFork();

    void PostZygoteFork();

    Expected<int, Error> ExecutePandaFile(std::string_view filename, std::string_view entry_point,
                                          const std::vector<std::string> &args);

    int StartDProfiler(std::string_view app_name);

    Expected<int, Error> Execute(std::string_view entry_point, const std::vector<std::string> &args);

    int StartDProfiler(const PandaString &app_name);

    bool IsDebugMode() const
    {
        return is_debug_mode_;
    }

    void SetDebugMode(bool is_debug_mode)
    {
        is_debug_mode_ = is_debug_mode;
    }

    void SetDebuggerLibrary(os::library_loader::LibraryHandle debugger_library)
    {
        debugger_library_ = std::move(debugger_library);
    }

    bool IsDebuggerConnected() const
    {
        return is_debugger_connected_;
    }

    void SetDebuggerConnected(bool dbg_connected_state)
    {
        is_debugger_connected_ = dbg_connected_state;
    }

    PandaVector<PandaString> GetBootPandaFiles();

    PandaVector<PandaString> GetPandaFiles();

    // Additional VMInfo
    void RegisterAppInfo(const PandaVector<PandaString> &code_paths, const PandaString &profile_output_filename);

    // Returns true if profile saving is enabled.
    bool SaveProfileInfo() const;

    const std::string &GetProcessPackageName() const
    {
        return process_package_name_;
    }

    void SetProcessPackageName(const char *package_name)
    {
        if (package_name == nullptr) {
            process_package_name_.clear();
        } else {
            process_package_name_ = package_name;
        }
    }

    const std::string &GetProcessDataDirectory() const
    {
        return process_data_directory_;
    }

    void SetProcessDataDirectory(const char *data_dir)
    {
        if (data_dir == nullptr) {
            process_data_directory_.clear();
        } else {
            process_data_directory_ = data_dir;
        }
    }

    std::string GetPandaPath()
    {
        return panda_path_string_;
    }

    void UpdateProcessState(int state);

    bool IsZygote() const
    {
        return is_zygote_;
    }

    bool IsInitialized() const
    {
        return is_initialized_;
    }

    static const char *GetVersion()
    {
        return "1.0.0";
    }

    PandaString GetFingerprint()
    {
        return fingerPrint_;
    }

    [[noreturn]] static void Halt(int32_t status);

    void SetExitHook(ExitHook exit_hook)
    {
        ASSERT(exit_ == nullptr);
        exit_ = exit_hook;
    }

    void SetAbortHook(AbortHook abort_hook)
    {
        ASSERT(abort_ == nullptr);
        abort_ = abort_hook;
    }

    [[noreturn]] static void Abort(const char *message = nullptr);

    Expected<Method *, Error> ResolveEntryPoint(std::string_view entry_point);

    void RegisterSensitiveThread() const;

    PandaVM *GetPandaVM() const
    {
        return panda_vm_;
    }

    tooling::PtLangExt *GetPtLangExt() const
    {
        return pt_lang_ext_;
    }

    const panda::verifier::VerificationOptions &GetVerificationOptions() const
    {
        return VerificationOptions_;
    }

    panda::verifier::VerificationOptions &GetVerificationOptions()
    {
        return VerificationOptions_;
    }

    void DumpForSigQuit(std::ostream &os);

    bool IsDumpNativeCrash()
    {
        return is_dump_native_crash_;
    }

    bool IsChecksSuspend() const
    {
        return checks_suspend_;
    }

    bool IsChecksStack() const
    {
        return checks_stack_;
    }

    bool IsChecksNullptr() const
    {
        return checks_nullptr_;
    }

    bool IsStacktrace() const
    {
        return is_stacktrace_;
    }

    SignalManager *GetSignalManager()
    {
        return signal_manager_;
    }

    Trace *CreateTrace(LanguageContext ctx, PandaUniquePtr<os::unix::file::File> trace_file, size_t buffer_size);

    void SetPtLangExt(tooling::PtLangExt *pt_lang_ext);

    static mem::GCType GetGCType(const RuntimeOptions &options);

    bool AttachDebugger();

    mem::InternalAllocatorPtr GetInternalAllocator() const
    {
        return internal_allocator_;
    }

    PandaString GetMemoryStatistics();
    PandaString GetFinalStatistics();

    Expected<LanguageContext, Error> ExtractLanguageContext(const panda_file::File *pf, std::string_view entry_point);

private:
    void NotifyAboutLoadedModules();

    std::optional<Error> CreateApplicationClassLinkerContext(std::string_view filename, std::string_view entry_point);

    bool LoadVerificationConfig();

    bool CreatePandaVM(std::string_view runtime_type);

    bool InitializePandaVM();

    bool CheckOptionsConsistency();

    void SetPandaPath();

    bool Initialize();

    bool Shutdown();

    bool LoadBootPandaFiles(panda_file::File::OpenMode open_mode);

    bool StartDebugger(const std::string &library_path);

    bool IsEnableMemoryHooks() const;

    static void CreateDfxController(const RuntimeOptions &options);

    static void BlockSignals();

    inline void InitializeVerificationResultCache(const RuntimeOptions &options);

    Runtime(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator,
            const std::vector<LanguageContextBase *> &ctxs);

    ~Runtime();

    static Runtime *instance;
    static RuntimeOptions options_;
    static os::memory::Mutex mutex;

    mem::InternalAllocatorPtr internal_allocator_;
    RuntimeNotificationManager *notification_manager_;
    ClassLinker *class_linker_;
    DProfiler *dprofiler_ = nullptr;

    PandaVM *panda_vm_ = nullptr;

    SignalManager *signal_manager_ {nullptr};

    // Language context
    static constexpr size_t LANG_EXTENSIONS_COUNT = static_cast<size_t>(panda_file::SourceLang::LAST) + 1;
    std::array<LanguageContextBase *, LANG_EXTENSIONS_COUNT> language_contexts_ {nullptr};

    // For IDE is really connected.
    bool is_debug_mode_ {false};
    bool is_debugger_connected_ {false};
    tooling::PtLangExt *pt_lang_ext_ {nullptr};
    tooling::DebugInterface *debugger_ {nullptr};
    os::library_loader::LibraryHandle debugger_library_;

    // Additional VMInfo
    std::string process_package_name_;
    std::string process_data_directory_;

    // For saving class path.
    std::string panda_path_string_;

    AbortHook abort_ = nullptr;
    ExitHook exit_ = nullptr;

    bool zygote_no_threads_;
    bool is_zygote_;
    bool is_initialized_ {false};

    bool save_profiling_info_;

    bool checks_suspend_ {false};
    bool checks_stack_ {true};
    bool checks_nullptr_ {true};
    bool is_stacktrace_ {false};

    bool is_dump_native_crash_ {true};

    PandaString fingerPrint_ = "unknown";

    // Verification
    panda::verifier::VerificationOptions VerificationOptions_;

    struct AppContext {
        ClassLinkerContext *ctx {nullptr};
        std::optional<panda_file::SourceLang> lang;
    };
    AppContext app_context_ {};

    NO_COPY_SEMANTIC(Runtime);
    NO_MOVE_SEMANTIC(Runtime);
};

inline mem::AllocatorAdapter<void> GetInternalAllocatorAdapter(const Runtime *runtime)
{
    return runtime->GetInternalAllocator()->Adapter();
}

void InitSignals();

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_RUNTIME_H_
