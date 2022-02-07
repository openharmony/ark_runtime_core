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

#include "runtime/include/runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "assembler/assembly-literals.h"
#include "intrinsics.h"
#include "libpandabase/events/events.h"
#include "libpandabase/mem/mem_config.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/os/library_loader.h"
#include "libpandabase/os/native_stack.h"
#include "libpandabase/os/thread.h"
#include "libpandabase/utils/arena_containers.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/dfx.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/file-inl.h"
#include "libpandafile/literal_data_accessor-inl.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "runtime/core/core_language_context.h"
#include "runtime/dprofiler/dprofiler.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/language_context.h"
#include "runtime/include/locks.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/thread.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/include/tooling/debug_inf.h"
#include "mem/refstorage/reference_storage.h"
#include "runtime/mem/gc/gc_stats.h"
#include "runtime/mem/gc/stw-gc/stw-gc.h"
#include "runtime/mem/gc/crossing_map_singleton.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/mem/mem_hooks.h"
#include "runtime/mem/memory_manager.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/core/core_class_linker_extension.h"
#include "runtime/include/panda_vm.h"
#include "runtime/profilesaver/profile_saver.h"
#include "runtime/tooling/debugger.h"
#include "runtime/tooling/pt_lang_ext_private.h"
#include "runtime/include/file_manager.h"
#include "trace/trace.h"
#include "verification/cache/file_entity_cache.h"
#include "verification/cache/results_cache.h"
#include "verification/debug/config_load.h"
#include "verification/debug/context/context.h"
#include "verification/job_queue/job_queue.h"
#include "verification/type/type_systems.h"

namespace panda {

using std::unique_ptr;

Runtime *Runtime::instance = nullptr;
RuntimeOptions Runtime::options_;  // NOLINT(fuchsia-statically-constructed-objects)
os::memory::Mutex Runtime::mutex;  // NOLINT(fuchsia-statically-constructed-objects)

class RuntimeInternalAllocator {
public:
    static mem::InternalAllocatorPtr Create(bool use_malloc_for_internal_allocation)
    {
        ASSERT(mem::InternalAllocator<>::GetInternalAllocatorFromRuntime() == nullptr);

        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        mem_stats_s_ = new (std::nothrow) mem::MemStatsType();
        ASSERT(mem_stats_s_ != nullptr);

        if (use_malloc_for_internal_allocation) {
            // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
            internal_allocator_s_ = new (std::nothrow)
                mem::InternalAllocatorT<mem::InternalAllocatorConfig::MALLOC_ALLOCATOR>(mem_stats_s_);
        } else {
            // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
            internal_allocator_s_ = new (std::nothrow)
                mem::InternalAllocatorT<mem::InternalAllocatorConfig::PANDA_ALLOCATORS>(mem_stats_s_);
        }
        ASSERT(internal_allocator_s_ != nullptr);
        mem::InternalAllocator<>::InitInternalAllocatorFromRuntime(
            static_cast<mem::Allocator *>(internal_allocator_s_));

        return internal_allocator_s_;
    }

    static void Finalize()
    {
        internal_allocator_s_->VisitAndRemoveAllPools(
            [](void *mem, size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    }

    static void Destroy()
    {
        ASSERT(mem::InternalAllocator<>::GetInternalAllocatorFromRuntime() != nullptr);

        mem::InternalAllocator<>::ClearInternalAllocatorFromRuntime();
        delete static_cast<mem::Allocator *>(internal_allocator_s_);
        internal_allocator_s_ = nullptr;

        // One more check that we don't have memory leak in internal allocator.
        ASSERT(mem_stats_s_->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL) == 0);
        delete mem_stats_s_;
        mem_stats_s_ = nullptr;
    }

    static mem::InternalAllocatorPtr Get()
    {
        ASSERT(internal_allocator_s_ != nullptr);
        return internal_allocator_s_;
    }

private:
    static mem::MemStatsType *mem_stats_s_;
    static mem::InternalAllocatorPtr internal_allocator_s_;  // NOLINT(fuchsia-statically-constructed-objects)
};

mem::MemStatsType *RuntimeInternalAllocator::mem_stats_s_ = nullptr;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
mem::InternalAllocatorPtr RuntimeInternalAllocator::internal_allocator_s_ = nullptr;

// all GetLanguageContext(...) methods should be based on this one
LanguageContext Runtime::GetLanguageContext(panda_file::SourceLang lang)
{
    auto *ctx = language_contexts_[static_cast<size_t>(lang)];
    ASSERT(ctx != nullptr);
    return LanguageContext(ctx);
}

LanguageContext Runtime::GetLanguageContext(const Method &method)
{
    // See EcmaVM::GetMethodForNativeFunction
    // Remove this 'if' when the function above gets fixed
    if (method.GetPandaFile() != nullptr) {
        panda_file::MethodDataAccessor mda(*method.GetPandaFile(), method.GetFileId());
        auto res = mda.GetSourceLang();
        if (res) {
            return GetLanguageContext(res.value());
        }
    }

    // Check class source lang
    auto *cls = method.GetClass();
    return GetLanguageContext(cls->GetSourceLang());
}

LanguageContext Runtime::GetLanguageContext(const Class &cls)
{
    return GetLanguageContext(cls.GetSourceLang());
}

LanguageContext Runtime::GetLanguageContext(const BaseClass &cls)
{
    return GetLanguageContext(cls.GetSourceLang());
}

LanguageContext Runtime::GetLanguageContext(panda_file::ClassDataAccessor *cda)
{
    auto res = cda->GetSourceLang();
    if (res) {
        return GetLanguageContext(res.value());
    }

    return GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
}

LanguageContext Runtime::GetLanguageContext(const std::string &runtime_type)
{
    if (runtime_type == "core") {
        return GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    }
    if (runtime_type == "ecmascript") {
        return GetLanguageContext(panda_file::SourceLang::ECMASCRIPT);
    }
    LOG(FATAL, RUNTIME) << "Incorrect runtime_type: " << runtime_type;
    UNREACHABLE();
}

/* static */
bool Runtime::CreateInstance(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator,
                             const std::vector<LanguageContextBase *> &ctxs)
{
    Locks::Initialize();

    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)

    if (options.WasSetEventsOutput()) {
        Events::Create(options.GetEventsOutput(), options.GetEventsFile());
    }

    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        if (instance != nullptr) {
            return false;
        }

        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        instance = new Runtime(options, internal_allocator, ctxs);
    }

    return true;
}

/* static */
bool Runtime::Create(const RuntimeOptions &options, const std::vector<LanguageContextBase *> &ctxs)
{
    if (instance != nullptr) {
        return false;
    }

    trace::ScopedTrace scoped_trace("Runtime::Create");

    panda::mem::MemConfig::Initialize(options.GetHeapSizeLimit(), options.GetInternalMemorySizeLimit(),
                                      options.GetCompilerMemorySizeLimit(), options.GetCodeCacheSizeLimit());
    PoolManager::Initialize();

    mem::InternalAllocatorPtr internal_allocator =
        RuntimeInternalAllocator::Create(options.UseMallocForInternalAllocations());

    BlockSignals();

    CreateDfxController(options);

    CreateInstance(options, internal_allocator, ctxs);

    if (!instance->Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize runtime";
        delete instance;
        instance = nullptr;
        return false;
    }

    instance->GetPandaVM()->StartGC();

    auto *thread = ManagedThread::GetCurrent();
    instance->GetNotificationManager()->VmStartEvent();
    instance->GetNotificationManager()->VmInitializationEvent(thread->GetId());
    instance->GetNotificationManager()->ThreadStartEvent(thread->GetId());

    return true;
}

Runtime *Runtime::GetCurrent()
{
    return instance;
}

/* static */
bool Runtime::DestroyUnderLockHolder()
{
    os::memory::LockHolder<os::memory::Mutex> lock(mutex);

    if (instance == nullptr) {
        return false;
    }

    if (!instance->Shutdown()) {
        LOG(ERROR, RUNTIME) << "Failed to shutdown runtime";
        return false;
    }
    if (GetOptions().WasSetEventsOutput()) {
        Events::Destroy();
    }

    /**
     * NOTE: Users threads can call log after destroying Runtime. We can't control these
     *       when they are in NATIVE_CODE mode because we don't destroy logger
     * Logger::Destroy();
     */

    DfxController::Destroy();
    delete instance;
    instance = nullptr;
    panda::mem::MemConfig::Finalize();

    return true;
}

/* static */
bool Runtime::Destroy()
{
    if (instance == nullptr) {
        return false;
    }

    trace::ScopedTrace scoped_trace("Runtime shutdown");
    instance->GetPandaVM()->StopGC();

    // NB! stop the profile saver thread before deleting the thread list to avoid dead loop here.
    // the following WaitForThreadStop makes sure profile saver can be shut down.
    if (instance->SaveProfileInfo()) {
        ProfileSaver::Stop(false);
    }

    instance->GetPandaVM()->UninitializeThreads();

    verifier::JobQueue::Stop(instance->GetVerificationOptions().Mode.OnlyVerify);

    instance->GetNotificationManager()->VmDeathEvent();

    verifier::JobQueue::Destroy();
    verifier::TypeSystems::Destroy();
    verifier::VerificationResultCache::Destroy(Runtime::GetCurrent()->GetVerificationOptions().Cache.UpdateOnExit);

    DestroyUnderLockHolder();
    RuntimeInternalAllocator::Destroy();

    return true;
}

void Runtime::InitializeVerificationResultCache(const panda::RuntimeOptions &options)
{
    auto &&verif_options = GetVerificationOptions();
    if (verif_options.Enable) {
        verifier::TypeSystems::Initialize();
        verifier::JobQueue::Initialize(verif_options.Mode.VerificationThreads);
        const auto &boot_panda_files = options.GetBootPandaFiles();
        size_t files_len = options.GetPandaFiles().empty() ? boot_panda_files.size() - 1 : boot_panda_files.size();
        for (size_t i = 0; i < files_len; i++) {
            verifier::JobQueue::AddSystemFile(boot_panda_files[i]);
        }

        auto &&cache_file = verif_options.Cache.File;
        if (!cache_file.empty()) {
            verifier::VerificationResultCache::Initialize(cache_file);
        }
    }
}

/* static */
void Runtime::Halt(int32_t status)
{
    Runtime *runtime = Runtime::GetCurrent();
    if (runtime != nullptr && runtime->exit_ != nullptr) {
        runtime->exit_(status);
    }

    // _exit is safer to call because it guarantees a safe
    // completion in case of multi-threading as static destructors aren't called
    _exit(status);
}

/* static */
void Runtime::Abort(const char *message /* = nullptr */)
{
    Runtime *runtime = Runtime::GetCurrent();
    if (runtime != nullptr && runtime->abort_ != nullptr) {
        runtime->abort_();
    }

    std::cerr << "Runtime::Abort: " << (message != nullptr ? message : "") << std::endl;
    std::abort();
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
Runtime::Runtime(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator,
                 const std::vector<LanguageContextBase *> &ctxs)
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    : internal_allocator_(internal_allocator),
      // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
      notification_manager_(new RuntimeNotificationManager(internal_allocator_)),
      // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
      debugger_library_(nullptr),
      zygote_no_threads_(false)
{
    Runtime::options_ = options;
    // ECMAScript doesn't use intrinsics
    if (options_.GetRuntimeType() == "ecmascript") {
        options_.SetShouldInitializeIntrinsics(false);
    }

    auto spaces = GetOptions().GetBootClassSpaces();

    // Default core context
    static CoreLanguageContext lcCore;
    language_contexts_[static_cast<size_t>(lcCore.GetLanguage())] = &lcCore;

    for (const auto &ctx : ctxs) {
        language_contexts_[static_cast<size_t>(ctx->GetLanguage())] = ctx;
    }

    std::vector<std::unique_ptr<ClassLinkerExtension>> extensions;
    extensions.reserve(spaces.size());

    for (const auto &space : spaces) {
        extensions.push_back(GetLanguageContext(space).CreateClassLinkerExtension());
    }

    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    class_linker_ = new ClassLinker(internal_allocator_, std::move(extensions));
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    signal_manager_ = new SignalManager(internal_allocator_);

    if (IsEnableMemoryHooks()) {
        // libbfd (which is used to get debug info from elf files) does a lot of allocations.
        // Don't track allocations in this case.
        if (!options_.IsSafepointBacktrace()) {
            mem::PandaHooks::Enable();
        }
    }

    save_profiling_info_ = false;

    VerificationOptions_.Initialize(options_);
    InitializeVerificationResultCache(options_);

    is_zygote_ = options_.IsStartAsZygote();
}

Runtime::~Runtime()
{
    VerificationOptions_.Destroy();
    panda::verifier::debug::DebugContext::Destroy();

    if (IsEnableMemoryHooks()) {
        mem::PandaHooks::Disable();
    }
    trace::ScopedTrace scoped_trace("Delete state");

    signal_manager_->DeleteHandlersArray();
    delete signal_manager_;
    delete class_linker_;
    if (dprofiler_ != nullptr) {
        internal_allocator_->Delete(dprofiler_);
    }
    delete notification_manager_;

    if (pt_lang_ext_ != nullptr) {
        internal_allocator_->Delete(pt_lang_ext_);
    }

    if (panda_vm_ != nullptr) {
        internal_allocator_->Delete(panda_vm_);
    }

    // crossing map is shared by Java VM and Js VM.
    mem::CrossingMapSingleton::Destroy();

    RuntimeInternalAllocator::Finalize();
    PoolManager::Finalize();
}

bool Runtime::IsEnableMemoryHooks() const
{
    auto log_level = Logger::IsInitialized() ? Logger::GetLevel() : Logger::Level::DEBUG;
    return options_.IsLimitStandardAlloc() &&
           (log_level == Logger::Level::FATAL || log_level == Logger::Level::ERROR) &&
           (!options_.UseMallocForInternalAllocations());
}

static PandaVector<PandaString> GetPandaFilesList(const std::vector<std::string> &stdvec)
{
    PandaVector<PandaString> res;
    for (const auto &i : stdvec) {
        // NOLINTNEXTLINE(readability-redundant-string-cstr)
        res.push_back(i.c_str());
    }

    return res;
}

PandaVector<PandaString> Runtime::GetBootPandaFiles()
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &boot_panda_files = GetPandaFilesList(options_.GetBootPandaFiles());
    return boot_panda_files;
}

PandaVector<PandaString> Runtime::GetPandaFiles()
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &app_panda_files = GetPandaFilesList(options_.GetPandaFiles());
    return app_panda_files;
}

bool Runtime::LoadBootPandaFiles(panda_file::File::OpenMode open_mode)
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &boot_panda_files = options_.GetBootPandaFiles();
    for (const auto &name : boot_panda_files) {
        if (!FileManager::LoadAbcFile(ConvertToString(name), open_mode)) {
            LOG(ERROR, RUNTIME) << "Load boot panda file failed: " << name;
            return false;
        }
    }

    return true;
}

mem::GCType Runtime::GetGCType(const RuntimeOptions &options)
{
    auto gc_type = panda::mem::GCTypeFromString(options.GetGcType());
    if (options.IsNoAsyncJit()) {
        // With no-async-jit we can force compilation inside of c2i bridge (we have IncrementHotnessCounter there)
        // and it can trigger GC which can move objects which are arguments for the method
        // because StackWalker ignores c2i frame
        return gc_type == panda::mem::GCType::GEN_GC ? panda::mem::GCType::STW_GC : gc_type;
    }
    return gc_type;
}

bool Runtime::LoadVerificationConfig()
{
    const auto &options = GetVerificationOptions();

    if (options.Enable) {
        if (options.Mode.DebugEnable) {
            if (!verifier::config::LoadConfig(options.Debug.ConfigFile)) {
                return false;
            }
        }
    }

    return true;
}

bool Runtime::CreatePandaVM(std::string_view runtime_type)
{
    if (!ManagedThread::Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize managed thread";
        return false;
    }

    panda_vm_ = PandaVM::Create(this, options_, runtime_type);
    if (panda_vm_ == nullptr) {
        LOG(ERROR, RUNTIME) << "Failed to create panda vm";
        return false;
    }

    panda_file::File::OpenMode open_mode = panda_file::File::READ_ONLY;
    if (Runtime::GetOptions().GetRuntimeType() == "ecmascript") {
        // In case of JS vm open a panda file for reading / writing
        // because EcmaVM patches bytecode in-place
        open_mode = panda_file::File::READ_WRITE;
    }
    bool load_boot_panda_files_is_failed = options_.ShouldLoadBootPandaFiles() && !LoadBootPandaFiles(open_mode);
    if (load_boot_panda_files_is_failed) {
        LOG(ERROR, RUNTIME) << "Failed to load boot panda files";
        return false;
    }

    notification_manager_->SetRendezvous(panda_vm_->GetRendezvous());

    return true;
}

bool Runtime::InitializePandaVM()
{
    if (!class_linker_->Initialize(options_.IsRuntimeCompressedStringsEnabled())) {
        LOG(ERROR, RUNTIME) << "Failed to initialize class loader";
        return false;
    }

    if (options_.ShouldInitializeIntrinsics() && !intrinsics::Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize intrinsics";
        return false;
    }

    std::string debug_library_path = options_.GetDebuggerLibraryPath();
    if (!debug_library_path.empty()) {
        if (!StartDebugger(debug_library_path)) {
            LOG(ERROR, RUNTIME) << "Failed to start debugger";
            return false;
        }
    }

    if (!panda_vm_->Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize panda vm";
        return false;
    }

    return true;
}

bool Runtime::CheckOptionsConsistency()
{
    return true;
}

void Runtime::SetPandaPath()
{
    PandaVector<PandaString> app_panda_files = GetPandaFiles();
    for (size_t i = 0; i < app_panda_files.size(); ++i) {
        panda_path_string_ += PandaStringToStd(app_panda_files[i]);
        if (i != app_panda_files.size() - 1) {
            panda_path_string_ += ":";
        }
    }
}

bool Runtime::Initialize()
{
    trace::ScopedTrace scoped_trace("Runtime::Initialize");

    if (!CheckOptionsConsistency()) {
        return false;
    }

    if (!LoadVerificationConfig()) {
        return false;
    }

    auto runtime_type = options_.GetRuntimeType();
    if (!CreatePandaVM(runtime_type)) {
        return false;
    }

    if (!InitializePandaVM()) {
        return false;
    }

    ManagedThread *thread = ManagedThread::GetCurrent();
    class_linker_->InitializeRoots(thread);
    auto ext = GetClassLinker()->GetExtension(GetLanguageContext(runtime_type));
    if (ext != nullptr) {
        thread->SetStringClassPtr(ext->GetClassRoot(ClassRoot::STRING));
    }

    fingerPrint_ = ConvertToString(options_.GetFingerprint());

    SetPandaPath();

    if (!panda_vm_->InitializeFinish()) {
        LOG(ERROR, RUNTIME) << "Failed to finish panda vm initialization";
        return false;
    }

    is_initialized_ = true;
    return true;
}

static bool GetClassAndMethod(std::string_view entry_point, PandaString *class_name, PandaString *method_name)
{
    size_t pos = entry_point.find_last_of("::");
    if (pos == std::string_view::npos) {
        return false;
    }

    *class_name = PandaString(entry_point.substr(0, pos - 1));
    *method_name = PandaString(entry_point.substr(pos + 1));

    return true;
}

static const uint8_t *GetStringArrayDescriptor(LanguageContext ctx, PandaString *out)
{
    *out = "[";
    *out += utf::Mutf8AsCString(ctx.GetStringClassDescriptor());

    return utf::CStringAsMutf8(out->c_str());
}

Expected<Method *, Runtime::Error> Runtime::ResolveEntryPoint(std::string_view entry_point)
{
    PandaString class_name;
    PandaString method_name;

    if (!GetClassAndMethod(entry_point, &class_name, &method_name)) {
        LOG(ERROR, RUNTIME) << "Invalid entry point: " << entry_point;
        return Unexpected(Runtime::Error::INVALID_ENTRY_POINT);
    }

    PandaString descriptor;
    auto class_name_bytes = ClassHelper::GetDescriptor(utf::CStringAsMutf8(class_name.c_str()), &descriptor);
    auto method_name_bytes = utf::CStringAsMutf8(method_name.c_str());

    Class *cls = nullptr;
    ClassLinkerContext *context = app_context_.ctx;
    if (context == nullptr) {
        context = class_linker_->GetExtension(GetLanguageContext(options_.GetRuntimeType()))->GetBootContext();
    }

    ManagedThread *thread = ManagedThread::GetCurrent();
    if (MTManagedThread::ThreadIsMTManagedThread(thread)) {
        ScopedManagedCodeThread sa(static_cast<MTManagedThread *>(thread));
        cls = class_linker_->GetClass(class_name_bytes, true, context);
    } else {
        cls = class_linker_->GetClass(class_name_bytes, true, context);
    }

    if (cls == nullptr) {
        LOG(ERROR, RUNTIME) << "Cannot find class '" << class_name << "'";
        return Unexpected(Runtime::Error::CLASS_NOT_FOUND);
    }

    LanguageContext ctx = GetLanguageContext(*cls);
    PandaString string_array_descriptor;
    GetStringArrayDescriptor(ctx, &string_array_descriptor);

    Method::Proto proto(PandaVector<panda_file::Type> {panda_file::Type(panda_file::Type::TypeId::VOID),
                                                       panda_file::Type(panda_file::Type::TypeId::REFERENCE)},
                        PandaVector<std::string_view> {string_array_descriptor});

    auto method = cls->GetDirectMethod(method_name_bytes, proto);
    if (method == nullptr) {
        method = cls->GetDirectMethod(method_name_bytes);
        if (method == nullptr) {
            LOG(ERROR, RUNTIME) << "Cannot find method '" << entry_point << "'";
            return Unexpected(Runtime::Error::METHOD_NOT_FOUND);
        }
    }

    return method;
}

PandaString Runtime::GetMemoryStatistics()
{
    return panda_vm_->GetMemStats()->GetStatistics(panda_vm_->GetHeapManager());
}

PandaString Runtime::GetFinalStatistics()
{
    return panda_vm_->GetGCStats()->GetFinalStatistics(panda_vm_->GetHeapManager());
}

void Runtime::NotifyAboutLoadedModules()
{
    PandaVector<const panda_file::File *> pfs;

    class_linker_->EnumerateBootPandaFiles([&pfs](const panda_file::File &pf) {
        pfs.push_back(&pf);
        return true;
    });

    for (const auto *pf : pfs) {
        GetNotificationManager()->LoadModuleEvent(pf->GetFilename());
    }
}

Expected<LanguageContext, Runtime::Error> Runtime::ExtractLanguageContext(const panda_file::File *pf,
                                                                          std::string_view entry_point)
{
    PandaString class_name;
    PandaString method_name;
    if (!GetClassAndMethod(entry_point, &class_name, &method_name)) {
        LOG(ERROR, RUNTIME) << "Invalid entry point: " << entry_point;
        return Unexpected(Runtime::Error::INVALID_ENTRY_POINT);
    }

    PandaString descriptor;
    auto class_name_bytes = ClassHelper::GetDescriptor(utf::CStringAsMutf8(class_name.c_str()), &descriptor);
    auto method_name_bytes = utf::CStringAsMutf8(method_name.c_str());

    auto class_id = pf->GetClassId(class_name_bytes);
    if (!class_id.IsValid() || pf->IsExternal(class_id)) {
        LOG(ERROR, RUNTIME) << "Cannot find class '" << class_name << "'";
        return Unexpected(Runtime::Error::CLASS_NOT_FOUND);
    }

    panda_file::ClassDataAccessor cda(*pf, class_id);
    LanguageContext ctx = GetLanguageContext(&cda);
    bool found = false;
    cda.EnumerateMethods([this, &pf, method_name_bytes, &found, &ctx](panda_file::MethodDataAccessor &mda) {
        if (!found && utf::IsEqual(pf->GetStringData(mda.GetNameId()).data, method_name_bytes)) {
            found = true;
            auto val = mda.GetSourceLang();
            if (val) {
                ctx = GetLanguageContext(val.value());
            }
        }
    });

    if (!found) {
        LOG(ERROR, RUNTIME) << "Cannot find method '" << entry_point << "'";
        return Unexpected(Runtime::Error::METHOD_NOT_FOUND);
    }

    return ctx;
}

std::optional<Runtime::Error> Runtime::CreateApplicationClassLinkerContext(std::string_view filename,
                                                                           std::string_view entry_point)
{
    bool is_loaded = false;
    class_linker_->EnumerateBootPandaFiles([&is_loaded, filename](const panda_file::File &pf) {
        if (pf.GetFilename() == filename) {
            is_loaded = true;
            return false;
        }
        return true;
    });

    if (is_loaded) {
        return {};
    }

    auto pf = panda_file::OpenPandaFileOrZip(filename);
    if (pf == nullptr) {
        return Runtime::Error::PANDA_FILE_LOAD_ERROR;
    }

    auto res = ExtractLanguageContext(pf.get(), entry_point);
    if (!res) {
        return res.Error();
    }

    if (!class_linker_->HasExtension(res.Value())) {
        LOG(ERROR, RUNTIME) << "class linker hasn't " << res.Value() << " language extension";
        return Runtime::Error::CLASS_LINKER_EXTENSION_NOT_FOUND;
    }

    auto *ext = class_linker_->GetExtension(res.Value());
    app_context_.lang = ext->GetLanguage();
    app_context_.ctx = class_linker_->GetAppContext(filename);
    if (app_context_.ctx == nullptr) {
        auto app_files = GetPandaFiles();
        auto found_iter = std::find_if(app_files.begin(), app_files.end(),
                                       [&](auto &app_file_name) { return app_file_name == filename; });
        if (found_iter == app_files.end()) {
            PandaString path(filename);
            app_files.push_back(path);
        }
        app_context_.ctx = ext->CreateApplicationClassLinkerContext(app_files);
    }

    tooling::DebugInf::AddCodeMetaInfo(pf.get());
    return {};
}

Expected<int, Runtime::Error> Runtime::ExecutePandaFile(std::string_view filename, std::string_view entry_point,
                                                        const std::vector<std::string> &args)
{
    if (options_.IsDistributedProfiling()) {
        // Create app name from path to executable file.
        std::string_view app_name = [](std::string_view path) -> std::string_view {
            auto pos = path.find_last_of('/');
            return path.substr((pos == std::string_view::npos) ? 0 : (pos + 1));
        }(filename);
        StartDProfiler(app_name);
    }

    auto ctx_err = CreateApplicationClassLinkerContext(filename, entry_point);
    if (ctx_err) {
        return Unexpected(ctx_err.value());
    }

    return Execute(entry_point, args);
}

Expected<int, Runtime::Error> Runtime::Execute(std::string_view entry_point, const std::vector<std::string> &args)
{
    auto resolve_res = ResolveEntryPoint(entry_point);
    if (!resolve_res) {
        return Unexpected(resolve_res.Error());
    }

    NotifyAboutLoadedModules();

    Method *method = resolve_res.Value();

    return panda_vm_->InvokeEntrypoint(method, args);
}

void Runtime::RegisterAppInfo(const PandaVector<PandaString> &code_paths, const PandaString &profile_output_filename)
{
    for (const auto &str : code_paths) {
        LOG(INFO, RUNTIME) << "Code path: " << str;
    }
    std::string_view app_name = [](std::string_view path) -> std::string_view {
        auto pos = path.find_last_of('/');
        return path.substr((pos == std::string_view::npos) ? 0 : (pos + 1));
    }(profile_output_filename);

    StartDProfiler(app_name);

    // this is exactly the place where start the profile saver
    ProfileSaver::Start(profile_output_filename, code_paths, PandaString(app_name));
}

int Runtime::StartDProfiler(std::string_view app_name)
{
    if (dprofiler_ != nullptr) {
        LOG(ERROR, RUNTIME) << "DProfiller already started";
        return -1;
    }

    dprofiler_ = internal_allocator_->New<DProfiler>(app_name, Runtime::GetCurrent());
    return 0;
}

bool Runtime::StartDebugger(const std::string &library_path)
{
    auto handle = os::library_loader::Load(library_path);
    if (!handle) {
        return true;
    }

    using StartDebuggerT = int (*)(uint32_t, tooling::DebugInterface *, void *);

    auto sym = os::library_loader::ResolveSymbol(handle.Value(), "StartDebugger");
    if (!sym) {
        LOG(ERROR, RUNTIME) << sym.Error().ToString();
        return false;
    }

    uint32_t port = options_.GetDebuggerPort();
    SetDebugMode(true);
    if (!AttachDebugger()) {
        return false;
    }
    ASSERT(debugger_ != nullptr);

    int res = reinterpret_cast<StartDebuggerT>(sym.Value())(port, debugger_, nullptr);
    if (res != 0) {
        LOG(ERROR, RUNTIME) << "StartDebugger has failed";
        return false;
    }

    ASSERT(!debugger_library_.IsValid());
    debugger_library_ = std::move(handle.Value());

    // Turn off stdout buffering in debug mode
    setvbuf(stdout, nullptr, _IONBF, 0);
    return true;
}

bool Runtime::AttachDebugger()
{
    ASSERT(is_debug_mode_);
    auto pt_lang_ext = tooling::CreatePtLangExt(options_.GetRuntimeType());
    if (!pt_lang_ext) {
        LOG(ERROR, RUNTIME) << "Cannot create PtLangExt";
        return false;
    }
    pt_lang_ext_ = pt_lang_ext.release();
    ASSERT(debugger_ == nullptr);
    debugger_ = internal_allocator_->New<tooling::Debugger>(this);
    return true;
}

bool Runtime::Shutdown()
{
    if (IsDebugMode() && debugger_library_.IsValid()) {
        using StopDebugger = int (*)();

        ASSERT(debugger_library_.IsValid());
        auto sym = os::library_loader::ResolveSymbol(debugger_library_, "StopDebugger");
        if (!sym) {
            LOG(ERROR, RUNTIME) << sym.Error().ToString();
            return false;
        }

        int res = reinterpret_cast<StopDebugger>(sym.Value())();
        if (res != 0) {
            LOG(ERROR, RUNTIME) << "StopDebugger has failed";
            return false;
        }
    }

    if (debugger_ != nullptr) {
        internal_allocator_->Delete(debugger_);
    }

    return ManagedThread::Shutdown();
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const Method &caller, panda_file::File::EntityId id)
{
    auto *pf = caller.GetPandaFile();
    LanguageContext ctx = GetLanguageContext(caller);
    return ResolveString(vm, *pf, id, ctx);
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const panda_file::File &pf, panda_file::File::EntityId id,
                                          LanguageContext ctx)
{
    coretypes::String *str = vm->GetStringTable()->GetInternalStringFast(pf, id);
    if (str != nullptr) {
        return str;
    }
    str = vm->GetStringTable()->GetOrInternInternalString(pf, id, ctx);
    return str;
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const uint8_t *mutf8, uint32_t length, LanguageContext ctx)
{
    return vm->GetStringTable()->GetOrInternString(mutf8, length, ctx);
}

coretypes::Array *Runtime::ResolveLiteralArray(PandaVM *vm, const Method &caller, panda_file::File::EntityId id)
{
    auto *pf = caller.GetPandaFile();
    LanguageContext ctx = GetLanguageContext(caller);
    return ResolveLiteralArray(vm, *pf, id, ctx);
}

Class *Runtime::GetClassRootForLiteralTag(const ClassLinkerExtension &ext, panda_file::LiteralTag tag) const
{
    switch (tag) {
        case panda_file::LiteralTag::ARRAY_I8:
            return ext.GetClassRoot(ClassRoot::ARRAY_I8);
        case panda_file::LiteralTag::ARRAY_I16:
            return ext.GetClassRoot(ClassRoot::ARRAY_I16);
        case panda_file::LiteralTag::ARRAY_I32:
            return ext.GetClassRoot(ClassRoot::ARRAY_I32);
        case panda_file::LiteralTag::ARRAY_I64:
            return ext.GetClassRoot(ClassRoot::ARRAY_I64);
        case panda_file::LiteralTag::ARRAY_F32:
            return ext.GetClassRoot(ClassRoot::ARRAY_F32);
        case panda_file::LiteralTag::ARRAY_F64:
            return ext.GetClassRoot(ClassRoot::ARRAY_F64);
        case panda_file::LiteralTag::ARRAY_STRING:
            return ext.GetClassRoot(ClassRoot::ARRAY_STRING);
        case panda_file::LiteralTag::TAGVALUE:
        case panda_file::LiteralTag::BOOL:
        case panda_file::LiteralTag::INTEGER:
        case panda_file::LiteralTag::FLOAT:
        case panda_file::LiteralTag::DOUBLE:
        case panda_file::LiteralTag::STRING:
        case panda_file::LiteralTag::METHOD:
        case panda_file::LiteralTag::GENERATORMETHOD:
        case panda_file::LiteralTag::ACCESSOR:
        case panda_file::LiteralTag::NULLVALUE: {
            break;
        }
        default: {
            break;
        }
    }
    UNREACHABLE();
    return nullptr;
}

/* static */
bool Runtime::GetLiteralTagAndValue(const panda_file::File &pf, panda_file::File::EntityId id,
                                    panda_file::LiteralTag *tag, panda_file::LiteralDataAccessor::LiteralValue *value)
{
    panda_file::File::EntityId literalArraysId = pf.GetLiteralArraysId();
    panda_file::LiteralDataAccessor literal_data_accessor(pf, literalArraysId);
    bool result = false;
    literal_data_accessor.EnumerateLiteralVals(
        id, [tag, value, &result](const panda_file::LiteralDataAccessor::LiteralValue &val,
                                  const panda_file::LiteralTag &tg) {
            *tag = tg;
            *value = val;
            result = true;
        });
    return result;
}

coretypes::Array *Runtime::ResolveLiteralArray(PandaVM *vm, const panda_file::File &pf, panda_file::File::EntityId id,
                                               LanguageContext ctx)
{
    panda_file::LiteralTag tag;
    panda_file::LiteralDataAccessor::LiteralValue value;

    coretypes::Array *array = nullptr;

    if (GetLiteralTagAndValue(pf, id, &tag, &value)) {
        panda_file::File::EntityId value_id(std::get<uint32_t>(value));
        auto sp = pf.GetSpanFromId(value_id);
        auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
        auto ext = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx);
        // special handling of arrays of strings
        if (tag == panda_file::LiteralTag::ARRAY_STRING) {
            array = coretypes::Array::Create(GetClassRootForLiteralTag(*ext, tag), len);
            VMHandle<coretypes::Array> obj(ManagedThread::GetCurrent(), array);
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (size_t i = 0; i < len; i++) {
                auto str_id = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
                auto str = Runtime::GetCurrent()->ResolveString(vm, pf, panda_file::File::EntityId(str_id), ctx);
                obj->Set<ObjectHeader *>(i, str);
            }
            array = obj.GetPtr();
        } else {
            array = coretypes::Array::Create(GetClassRootForLiteralTag(*ext, tag), sp.data(), len);
        }
    }

    return array;
}

void Runtime::UpdateProcessState([[maybe_unused]] int state)
{
    LOG(INFO, RUNTIME) << __func__ << " is an empty implementation now.";
}

void Runtime::RegisterSensitiveThread() const
{
    LOG(INFO, RUNTIME) << __func__ << " is an empty implementation now.";
}

void Runtime::CreateDfxController(const RuntimeOptions &options)
{
    DfxController::Initialize();
#ifdef PANDA_TARGET_UNIX
    DfxController::SetOptionValue(DfxOptionHandler::REFERENCE_DUMP, options.GetReferenceDump());
    DfxController::SetOptionValue(DfxOptionHandler::SIGNAL_HANDLER, options.GetSignalHandler());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGQUIT, options.GetSigquitFlag());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGUSR1, options.GetSigusr1Flag());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGUSR2, options.GetSigusr2Flag());
    DfxController::SetOptionValue(DfxOptionHandler::MOBILE_LOG, options.GetMobileLogFlag());
#endif
    DfxController::SetOptionValue(DfxOptionHandler::DFXLOG, options.GetDfxLog());
}

void Runtime::BlockSignals()
{
#if defined(PANDA_TARGET_UNIX)
    sigset_t set;
    if (sigemptyset(&set) == -1) {
        LOG(ERROR, RUNTIME) << "sigemptyset failed";
        return;
    }
#ifdef PANDA_TARGET_MOBILE
    int rc = 0;
    rc += sigaddset(&set, SIGQUIT);
    rc += sigaddset(&set, SIGUSR1);
    rc += sigaddset(&set, SIGUSR2);
    if (rc < 0) {
        LOG(ERROR, RUNTIME) << "sigaddset failed";
        return;
    }
#endif  // PANDA_TARGET_MOBILE

    if (os::native_stack::g_PandaThreadSigmask(SIG_BLOCK, &set, nullptr) != 0) {
        LOG(ERROR, RUNTIME) << "PandaThreadSigmask failed";
    }
#endif  // PANDA_TARGET_UNIX
}

void Runtime::DumpForSigQuit(std::ostream &os)
{
    os << "\n";
    os << "-> Dump class loaders\n";
    class_linker_->EnumerateContextsForDump(
        [](ClassLinkerContext *ctx, std::ostream &stream, ClassLinkerContext *parent) {
            ctx->Dump(stream);
            return ctx->FindClassLoaderParent(parent);
        },
        os);
    os << "\n";

    // dump GC
    os << "-> Dump GC\n";
    os << GetFinalStatistics();
    os << "\n";

    // dump memory management
    os << "-> Dump memory management\n";
    os << GetMemoryStatistics();
    os << "\n";
}

void Runtime::PreZygoteFork()
{
    panda_vm_->PreZygoteFork();
}

void Runtime::PostZygoteFork()
{
    panda_vm_->PostZygoteFork();
}

// Returns true if profile saving is enabled. GetJit() will be not null in this case.
bool Runtime::SaveProfileInfo() const
{
    return save_profiling_info_;
}

Trace *Runtime::CreateTrace([[maybe_unused]] LanguageContext ctx,
                            [[maybe_unused]] PandaUniquePtr<os::unix::file::File> trace_file,
                            [[maybe_unused]] size_t buffer_size)
{
    LOG(FATAL, RUNTIME) << "Method tracing isn't supported at the moment!";
    return nullptr;
}
}  // namespace panda
