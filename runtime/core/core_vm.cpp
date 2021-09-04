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

#include "core_vm.h"
#include "utils/expected.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/include/thread.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/mem/gc/reference-processor/empty_reference_processor.h"
#include "runtime/mem/refstorage/global_object_storage.h"

namespace panda::core {

// Create MemoryManager by RuntimeOptions
static mem::MemoryManager *CreateMM(LanguageContext ctx, mem::InternalAllocatorPtr internal_allocator,
                                    const RuntimeOptions &options)
{
    mem::MemoryManager::HeapOptions heap_options {
        nullptr,                                      // is_object_finalizeble_func
        nullptr,                                      // register_finalize_reference_func
        options.GetMaxGlobalRefSize(),                // max_global_ref_size
        options.IsGlobalReferenceSizeCheckEnabled(),  // is_global_reference_size_check_enabled
        false,                                        // is_single_thread
        options.IsUseTlabForAllocations(),            // is_use_tlab_for_allocations
        options.IsStartAsZygote(),                    // is_start_as_zygote
    };

    mem::GCTriggerConfig gc_trigger_config(options.GetGcTriggerType(), options.GetGcDebugTriggerStart(),
                                           options.GetMinExtraHeapSize(), options.GetMaxExtraHeapSize(),
                                           options.GetSkipStartupGcCount());

    mem::GCSettings gc_settings {options.IsGcEnableTracing(),
                                 panda::mem::NativeGcTriggerTypeFromString(options.GetNativeGcTriggerType()),
                                 options.IsGcDumpHeap(),
                                 options.IsConcurrentGcEnabled(),
                                 options.IsRunGcInPlace(),
                                 options.IsPreGcHeapVerifyEnabled(),
                                 options.IsPostGcHeapVerifyEnabled(),
                                 options.IsFailOnHeapVerification()};

    mem::GCType gc_type = Runtime::GetGCType(options);

    return mem::MemoryManager::Create(ctx, internal_allocator, gc_type, gc_settings, gc_trigger_config, heap_options);
}

/* static */
Expected<PandaCoreVM *, PandaString> PandaCoreVM::Create(Runtime *runtime, const RuntimeOptions &options)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    mem::MemoryManager *mm = CreateMM(ctx, runtime->GetInternalAllocator(), options);
    if (mm == nullptr) {
        return Unexpected(PandaString("Cannot create MemoryManager"));
    }

    auto allocator = mm->GetHeapManager()->GetInternalAllocator();
    PandaCoreVM *core_vm = allocator->New<PandaCoreVM>(runtime, options, mm);
    core_vm->InitializeGC();

    // Create Main Thread
    core_vm->main_thread_ = MTManagedThread::Create(runtime, core_vm);
    ASSERT(core_vm->main_thread_ == ManagedThread::GetCurrent());

    core_vm->thread_manager_->SetMainThread(core_vm->main_thread_);

    return core_vm;
}

PandaCoreVM::PandaCoreVM(Runtime *runtime, [[maybe_unused]] const RuntimeOptions &options, mem::MemoryManager *mm)
    : runtime_(runtime), mm_(mm)
{
    mem::HeapManager *heap_manager = mm_->GetHeapManager();
    mem::InternalAllocatorPtr allocator = heap_manager->GetInternalAllocator();
    string_table_ = allocator->New<StringTable>();
    monitor_pool_ = allocator->New<MonitorPool>(allocator);
    reference_processor_ = allocator->New<mem::EmptyReferenceProcessor>();
    thread_manager_ = allocator->New<ThreadManager>(allocator);
    rendezvous_ = allocator->New<Rendezvous>();
}

PandaCoreVM::~PandaCoreVM()
{
    delete main_thread_;

    mem::InternalAllocatorPtr allocator = mm_->GetHeapManager()->GetInternalAllocator();
    allocator->Delete(rendezvous_);
    allocator->Delete(thread_manager_);
    allocator->Delete(reference_processor_);
    allocator->Delete(monitor_pool_);
    allocator->Delete(string_table_);
    mm_->Finalize();
    mem::MemoryManager::Destroy(mm_);
}

bool PandaCoreVM::Initialize()
{
    return true;
}

bool PandaCoreVM::InitializeFinish()
{
    return true;
}

void PandaCoreVM::UninitializeThreads()
{
    // Wait until all threads finish the work
    thread_manager_->WaitForDeregistration();
    main_thread_->Destroy();
}

void PandaCoreVM::PreStartup()
{
    mm_->PreStartup();
}

void PandaCoreVM::PreZygoteFork()
{
    mm_->PreZygoteFork();
}

void PandaCoreVM::PostZygoteFork()
{
    mm_->PostZygoteFork();
}

void PandaCoreVM::InitializeGC()
{
    mm_->InitializeGC();
}

void PandaCoreVM::StartGC()
{
    mm_->StartGC();
}

void PandaCoreVM::StopGC()
{
    mm_->StopGC();
}

void PandaCoreVM::HandleReferences(const GCTask &task)
{
    LOG(DEBUG, REF_PROC) << "Start processing cleared references";
    mem::GC *gc = mm_->GetGC();
    gc->ProcessReferences(gc->GetGCPhase(), task);
}

void PandaCoreVM::HandleEnqueueReferences()
{
    LOG(DEBUG, REF_PROC) << "Start HandleEnqueueReferences";
    mm_->GetGC()->EnqueueReferences();
    LOG(DEBUG, REF_PROC) << "Finish HandleEnqueueReferences";
}

void PandaCoreVM::HandleGCFinished() {}

bool PandaCoreVM::CheckEntrypointSignature(Method *entrypoint)
{
    if (entrypoint->GetNumArgs() == 0) {
        return true;
    }

    if (entrypoint->GetNumArgs() > 1) {
        return false;
    }

    auto *pf = entrypoint->GetPandaFile();
    panda_file::MethodDataAccessor mda(*pf, entrypoint->GetFileId());
    panda_file::ProtoDataAccessor pda(*pf, mda.GetProtoId());

    if (pda.GetArgType(0).GetId() != panda_file::Type::TypeId::REFERENCE) {
        return false;
    }

    auto type_id = pda.GetReferenceType(0);
    auto string_data = pf->GetStringData(type_id);
    const char class_name[] = "[Lpanda/String;";  // NOLINT(modernize-avoid-c-arrays)

    return utf::IsEqual({string_data.data, string_data.utf16_length},
                        {utf::CStringAsMutf8(class_name), sizeof(class_name) - 1});
}

static coretypes::Array *CreateArgumentsArray(const std::vector<std::string> &args, LanguageContext ctx,
                                              ClassLinker *class_linker, PandaVM *vm)
{
    const char class_name[] = "[Lpanda/String;";  // NOLINT(modernize-avoid-c-arrays)
    auto *array_klass = class_linker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(class_name));
    if (array_klass == nullptr) {
        LOG(FATAL, RUNTIME) << "Class " << class_name << " not found";
    }

    auto thread = MTManagedThread::GetCurrent();
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    auto *array = coretypes::Array::Create(array_klass, args.size());
    VMHandle<coretypes::Array> array_handle(thread, array);

    for (size_t i = 0; i < args.size(); i++) {
        auto *str = coretypes::String::CreateFromMUtf8(utf::CStringAsMutf8(args[i].data()), args[i].length(), ctx, vm);
        array_handle.GetPtr()->Set(i, str);
    }

    return array_handle.GetPtr();
}

Expected<int, Runtime::Error> PandaCoreVM::InvokeEntrypointImpl(Method *entrypoint,
                                                                const std::vector<std::string> &args)
{
    Runtime *runtime = Runtime::GetCurrent();
    MTManagedThread *thread = MTManagedThread::GetCurrent();
    LanguageContext ctx = runtime->GetLanguageContext(*entrypoint);
    ASSERT(ctx.GetLanguage() == panda_file::SourceLang::PANDA_ASSEMBLY);

    ScopedManagedCodeThread sj(thread);
    ClassLinker *class_linker = runtime->GetClassLinker();
    if (!class_linker->InitializeClass(thread, entrypoint->GetClass())) {
        LOG(ERROR, RUNTIME) << "Cannot initialize class '" << entrypoint->GetClass()->GetName() << "'";
        return Unexpected(Runtime::Error::CLASS_NOT_INITIALIZED);
    }

    ObjectHeader *object_header = nullptr;
    if (entrypoint->GetNumArgs() == 1) {
        coretypes::Array *arg_array = CreateArgumentsArray(args, ctx, runtime_->GetClassLinker(), thread->GetVM());
        object_header = arg_array;
    }

    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    VMHandle<ObjectHeader> args_handle(thread, object_header);
    Value arg_val(args_handle.GetPtr());
    Value v = entrypoint->Invoke(thread, &arg_val);

    return v.GetAs<int>();
}

ObjectHeader *PandaCoreVM::GetOOMErrorObject()
{
    LOG(FATAL, RUNTIME) << "UNIMPLEMENTED: " << __FUNCTION__ << " +" << __LINE__;
    return nullptr;
}

void PandaCoreVM::HandleUncaughtException(ObjectHeader * /* exception */)
{
    LOG(FATAL, RUNTIME) << "UNIMPLEMENTED: " << __FUNCTION__ << " +" << __LINE__;
}

void PandaCoreVM::VisitVmRoots(const GCRootVisitor &visitor)
{
    GetGlobalObjectStorage()->VisitObjects(visitor, mem::RootType::ROOT_RS_GLOBAL);
}

}  // namespace panda::core
