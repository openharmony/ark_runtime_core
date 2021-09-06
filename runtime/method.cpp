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

#include "verification/debug/config_load.h"
#include "verification/debug/allowlist/allowlist.h"
#include "verification/job_queue/job_queue.h"
#include "verification/job_queue/job_fill.h"
#include "verification/cache/results_cache.h"
#include "verification/util/invalid_ref.h"

#include "events/events.h"
#include "runtime/bridge/bridge.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/jit/profiling_data.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/locks.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/value-inl.h"
#include "runtime/interpreter/frame.h"
#include "runtime/interpreter/interpreter.h"
#include "libpandabase/utils/hash.h"
#include "libpandabase/utils/span.h"
#include "libpandabase/utils/utf.h"
#include "libpandabase/os/mutex.h"
#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/debug_data_accessor-inl.h"
#include "libpandafile/file-inl.h"
#include "libpandafile/line_program_state.h"
#include "libpandafile/method_data_accessor-inl.h"
#include "libpandafile/method_data_accessor.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "libpandafile/shorty_iterator.h"
#include "runtime/handle_base-inl.h"
#include "runtime/handle_scope-inl.h"
#include "libpandafile/type_helper.h"

namespace panda {

Method::Proto::Proto(const panda_file::File &pf, panda_file::File::EntityId proto_id)
{
    panda_file::ProtoDataAccessor pda(pf, proto_id);

    pda.EnumerateTypes([this](panda_file::Type type) { shorty_.push_back(type); });

    size_t ref_idx = 0;

    for (auto &t : shorty_) {
        if (t.IsPrimitive()) {
            continue;
        }

        auto id = pda.GetReferenceType(ref_idx++);
        ref_types_.emplace_back(utf::Mutf8AsCString(pf.GetStringData(id).data));
    }
}

std::string_view Method::Proto::GetReturnTypeDescriptor() const
{
    auto ret_type = GetReturnType();
    if (!ret_type.IsPrimitive()) {
        return ref_types_[0];
    }

    switch (ret_type.GetId()) {
        case panda_file::Type::TypeId::VOID:
            return "V";
        case panda_file::Type::TypeId::U1:
            return "Z";
        case panda_file::Type::TypeId::I8:
            return "B";
        case panda_file::Type::TypeId::U8:
            return "H";
        case panda_file::Type::TypeId::I16:
            return "S";
        case panda_file::Type::TypeId::U16:
            return "C";
        case panda_file::Type::TypeId::I32:
            return "I";
        case panda_file::Type::TypeId::U32:
            return "U";
        case panda_file::Type::TypeId::F32:
            return "F";
        case panda_file::Type::TypeId::I64:
            return "J";
        case panda_file::Type::TypeId::U64:
            return "Q";
        case panda_file::Type::TypeId::F64:
            return "D";
        case panda_file::Type::TypeId::TAGGED:
            return "A";
        default:
            UNREACHABLE();
    }
}

uint32_t Method::GetFullNameHashFromString(const uint8_t *str)
{
    return GetHash32String(str);
}

uint32_t Method::GetClassNameHashFromString(const uint8_t *str)
{
    return GetHash32String(str);
}

uint32_t Method::GetFullNameHash() const
{
    // NB: this function cannot be used in current unit tests, because
    //     some unit tests are using underdefined method objects
    ASSERT(panda_file_ != nullptr && file_id_.IsValid());
    PandaString full_name {ClassHelper::GetName(GetClassName().data)};
    full_name += "::";
    full_name += utf::Mutf8AsCString(GetName().data);
    auto hash = GetFullNameHashFromString(reinterpret_cast<const uint8_t *>(full_name.c_str()));
    return hash;
}

Method::UniqId Method::CalcUniqId(const uint8_t *class_descr, const uint8_t *name)
{
    auto constexpr HALF = 32ULL;
    constexpr uint64_t NO_FILE = 0xFFFFFFFFULL << HALF;
    uint64_t hash = PseudoFnvHashString(class_descr);
    hash = PseudoFnvHashString(name, hash);
    return NO_FILE | hash;
}

Method::Method(Class *klass, const panda_file::File *pf, panda_file::File::EntityId file_id,
               panda_file::File::EntityId code_id, uint32_t access_flags, uint32_t num_args, const uint16_t *shorty)
    : stor_32_ {{}, access_flags, 0, num_args, 0},
      stor_ptr_ {{}, klass, nullptr, nullptr},
      panda_file_(pf),
      file_id_(file_id),
      code_id_(code_id),
      shorty_(shorty)
{
    SetCompilationStatus(CompilationStage::NOT_COMPILED);
}

Value Method::Invoke(ManagedThread *thread, Value *args, bool proxy_call)
{
    return InvokeImpl<false>(thread, GetNumArgs(), args, proxy_call);
}

Value Method::InvokeDyn(ManagedThread *thread, uint32_t num_args, Value *args, bool proxy_call, void *data)
{
    return InvokeImpl<true>(thread, num_args, args, proxy_call, data);
}

Value Method::InvokeGen(ManagedThread *thread, const uint8_t *pc, Value acc, uint32_t num_actual_args, Value *args,
                        void *data)
{
    Frame *current_frame = thread->GetCurrentFrame();
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
    Value res(static_cast<int64_t>(0));
    panda_file::Type ret_type = GetReturnType();
    if (!Verify()) {
        auto ctx = Runtime::GetCurrent()->GetLanguageContext(*this);
        panda::ThrowVerificationException(ctx, GetFullName());
        if (ret_type.IsReference()) {
            res = Value(nullptr);
        } else {
            res = Value(static_cast<int64_t>(0));
        }
    } else {
        Span<Value> args_span(args, num_actual_args);
        auto frame_deleter = [](Frame *frame) { FreeFrame(frame); };
        PandaUniquePtr<Frame, FrameDeleter> frame(
            CreateFrameWithActualArgs(num_actual_args, num_actual_args, this, current_frame), frame_deleter);

        for (size_t i = 0; i < num_actual_args; i++) {
            if (args_span[i].IsDecodedTaggedValue()) {
                DecodedTaggedValue decoded = args_span[i].GetDecodedTaggedValue();
                frame->GetVReg(i).SetValue(decoded.value);
                frame->GetVReg(i).SetTag(decoded.tag);
            } else if (args_span[i].IsReference()) {
                frame->GetVReg(i).SetReference(args_span[i].GetAs<ObjectHeader *>());
            } else {
                frame->GetVReg(i).SetPrimitive(args_span[i].GetAs<int64_t>());
            }
        }
        frame->GetAcc().SetValue(static_cast<uint64_t>(acc.GetAs<int64_t>()));
        frame->SetData(data);

        if (UNLIKELY(frame.get() == nullptr)) {
            panda::ThrowOutOfMemoryError("CreateFrame failed: " + GetFullName());
            if (ret_type.IsReference()) {
                res = Value(nullptr);
            } else {
                res = Value(static_cast<int64_t>(0));
            }
            return res;
        }
        thread->SetCurrentFrame(frame.get());

        Runtime::GetCurrent()->GetNotificationManager()->MethodEntryEvent(thread, this);
        interpreter::Execute(thread, pc, frame.get());
        Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, this);

        thread->SetCurrentFrame(current_frame);
        res = GetReturnValueFromAcc(ret_type, thread->HasPendingException(), frame->GetAcc());
    }
    return res;
}

panda_file::Type Method::GetReturnType() const
{
    panda_file::ShortyIterator it(shorty_);
    return *it;
}

panda_file::Type Method::GetArgType(size_t idx) const
{
    if (!IsStatic()) {
        if (idx == 0) {
            return panda_file::Type(panda_file::Type::TypeId::REFERENCE);
        }

        --idx;
    }

    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    panda_file::ProtoDataAccessor pda(*panda_file_, mda.GetProtoId());
    return pda.GetArgType(idx);
}

panda_file::File::StringData Method::GetRefArgType(size_t idx) const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);

    if (!IsStatic()) {
        if (idx == 0) {
            return panda_file_->GetStringData(mda.GetClassId());
        }

        --idx;
    }

    panda_file::ProtoDataAccessor pda(*panda_file_, mda.GetProtoId());
    panda_file::File::EntityId class_id = pda.GetReferenceType(idx);
    return panda_file_->GetStringData(class_id);
}

panda_file::File::StringData Method::GetName() const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    return panda_file_->GetStringData(mda.GetNameId());
}

PandaString Method::GetFullName(bool with_signature) const
{
    PandaOStringStream ss;
    int ref_idx = 0;
    if (with_signature) {
        auto return_type = GetReturnType();
        if (return_type.IsReference()) {
            ss << ClassHelper::GetName(GetRefArgType(ref_idx++).data) << ' ';
        } else {
            ss << return_type << ' ';
        }
    }
    ss << PandaString(GetClass()->GetName()) << "::" << utf::Mutf8AsCString(Method::GetName().data);
    if (!with_signature) {
        return ss.str();
    }
    const char *sep = "";
    ss << '(';
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    panda_file::ProtoDataAccessor pda(*panda_file_, mda.GetProtoId());
    for (size_t i = 0; i < GetNumArgs(); i++) {
        auto type = GetEffectiveArgType(i);
        if (type.IsReference()) {
            ss << sep << ClassHelper::GetName(GetRefArgType(ref_idx++).data);
        } else {
            ss << sep << type;
        }
        sep = ", ";
    }
    ss << ')';
    return ss.str();
}

panda_file::File::StringData Method::GetClassName() const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    return panda_file_->GetStringData(mda.GetClassId());
}

Method::Proto Method::GetProto() const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    return Proto(*panda_file_, mda.GetProtoId());
}

uint32_t Method::GetNumericalAnnotation(AnnotationField field_id) const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    return mda.GetNumericalAnnotation(field_id);
}

panda_file::File::StringData Method::GetStringDataAnnotation(AnnotationField field_id) const
{
    ASSERT(field_id >= AnnotationField::STRING_DATA_BEGIN);
    ASSERT(field_id <= AnnotationField::STRING_DATA_END);
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    uint32_t str_offset = mda.GetNumericalAnnotation(field_id);
    if (str_offset == 0) {
        return {0, nullptr};
    }
    return panda_file_->GetStringData(panda_file::File::EntityId(str_offset));
}

uint32_t Method::FindCatchBlock(Class *cls, uint32_t pc) const
{
    ASSERT(!IsAbstract());

    auto *thread = ManagedThread::GetCurrent();
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    VMHandle<ObjectHeader> exception(thread, thread->GetException());
    thread->ClearException();

    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    panda_file::CodeDataAccessor cda(*panda_file_, mda.GetCodeId().value());

    uint32_t pc_offset = panda_file::INVALID_OFFSET;

    cda.EnumerateTryBlocks([&pc_offset, cls, pc, this](panda_file::CodeDataAccessor::TryBlock &try_block) {
        if ((try_block.GetStartPc() <= pc) && ((try_block.GetStartPc() + try_block.GetLength()) > pc)) {
            try_block.EnumerateCatchBlocks([&](panda_file::CodeDataAccessor::CatchBlock &catch_block) {
                auto type_idx = catch_block.GetTypeIdx();
                if (type_idx == panda_file::INVALID_INDEX) {
                    pc_offset = catch_block.GetHandlerPc();
                    return false;
                }

                auto type_id = GetClass()->ResolveClassIndex(type_idx);
                auto *handler_class = Runtime::GetCurrent()->GetClassLinker()->GetClass(*this, type_id);
                if (cls->IsSubClassOf(handler_class)) {
                    pc_offset = catch_block.GetHandlerPc();
                    return false;
                }
                return true;
            });
        }
        return pc_offset == panda_file::INVALID_OFFSET;
    });

    thread->SetException(exception.GetPtr());

    return pc_offset;
}

panda_file::Type Method::GetEffectiveArgType(size_t idx) const
{
    return panda_file::GetEffectiveType(GetArgType(idx));
}

panda_file::Type Method::GetEffectiveReturnType() const
{
    return panda_file::GetEffectiveType(GetReturnType());
}

int32_t Method::GetLineNumFromBytecodeOffset(uint32_t bc_offset) const
{
    panda_file::MethodDataAccessor mda(*panda_file_, file_id_);
    auto debug_info_id = mda.GetDebugInfoId();
    if (!debug_info_id) {
        return -1;
    }

    using Opcode = panda_file::LineNumberProgramItem::Opcode;
    using EntityId = panda_file::File::EntityId;

    panda_file::DebugInfoDataAccessor dda(*panda_file_, debug_info_id.value());
    const uint8_t *program = dda.GetLineNumberProgram();
    auto size = panda_file_->GetSpanFromId(panda_file_->GetIdFromPointer(program)).size();
    auto opcode_sp = Span(reinterpret_cast<const Opcode *>(program), size);

    panda_file::LineProgramState state(*panda_file_, EntityId(0), dda.GetLineStart(), dda.GetConstantPool());

    size_t i = 0;
    Opcode opcode;
    size_t prev_line = state.GetLine();
    while ((opcode = opcode_sp[i++]) != Opcode::END_SEQUENCE) {
        switch (opcode) {
            case Opcode::ADVANCE_LINE: {
                auto line_diff = state.ReadSLeb128();
                state.AdvanceLine(line_diff);
                break;
            }
            case Opcode::ADVANCE_PC: {
                auto pc_diff = state.ReadULeb128();
                state.AdvancePc(pc_diff);
                break;
            }
            default: {
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
                auto opcode_value = static_cast<uint8_t>(opcode);
                if (opcode_value < panda_file::LineNumberProgramItem::OPCODE_BASE) {
                    break;
                }

                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
                auto adjust_opcode = opcode_value - panda_file::LineNumberProgramItem::OPCODE_BASE;
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
                uint32_t pc_diff = adjust_opcode / panda_file::LineNumberProgramItem::LINE_RANGE;
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
                int32_t line_diff = adjust_opcode % panda_file::LineNumberProgramItem::LINE_RANGE +
                                    panda_file::LineNumberProgramItem::LINE_BASE;

                state.AdvancePc(pc_diff);
                state.AdvanceLine(line_diff);

                if (state.GetAddress() == bc_offset) {
                    return state.GetLine();
                }

                if (state.GetAddress() > bc_offset) {
                    return prev_line;
                }

                prev_line = state.GetLine();

                break;
            }
        }
    }

    return state.GetLine();
}

panda_file::File::StringData Method::GetClassSourceFile() const
{
    panda_file::ClassDataAccessor cda(*panda_file_, GetClass()->GetFileId());
    auto source_file_id = cda.GetSourceFileId();
    if (!source_file_id) {
        return {0, nullptr};
    }

    return panda_file_->GetStringData(source_file_id.value());
}

bool Method::IsVerified() const
{
    if (IsIntrinsic()) {
        return true;
    }
    auto stage = GetVerificationStage();
    return stage == VerificationStage::VERIFIED_OK || stage == VerificationStage::VERIFIED_FAIL;
}

void Method::WaitForVerification()
{
    if (GetVerificationStage() == VerificationStage::WAITING) {
        LOG(DEBUG, VERIFIER) << "Method '" << GetFullName() << std::hex << "' ( 0x" << GetUniqId() << ", 0x"
                             << reinterpret_cast<uintptr_t>(this) << " ) is waiting to be verified";
        panda::verifier::JobQueue::WaitForVerification(
            [this] { return GetVerificationStage() == VerificationStage::WAITING; },
            [this] {
                auto &runtime = *Runtime::GetCurrent();
                auto &&verif_options = runtime.GetVerificationOptions();
                auto does_not_fail = verif_options.Mode.VerifierDoesNotFail;
                SetVerificationStage(does_not_fail ? VerificationStage::VERIFIED_OK : VerificationStage::VERIFIED_FAIL);
            });
    }
}

void Method::SetVerified(bool result)
{
    verifier::VerificationResultCache::CacheResult(GetUniqId(), result);
    SetVerificationStage(result ? VerificationStage::VERIFIED_OK : VerificationStage::VERIFIED_FAIL);
    panda::verifier::JobQueue::SignalMethodVerified();
}

bool Method::Verify()
{
    if (IsIntrinsic()) {
        return true;
    }
    auto stage = GetVerificationStage();
    if (stage == VerificationStage::VERIFIED_OK) {
        return true;
    }
    if (stage == VerificationStage::VERIFIED_FAIL) {
        return false;
    }

    EnqueueForVerification();
    auto &runtime = *Runtime::GetCurrent();
    auto &&verif_options = runtime.GetVerificationOptions();
    if (verif_options.Mode.VerifierDoesNotFail) {
        return true;
    }
    WaitForVerification();

    return Verify();
}

Method::~Method()
{
    WaitForVerification();
}

bool Method::AddJobInQueue()
{
    if (code_id_.IsValid() && !SKIP_VERIFICATION(GetUniqId())) {
        if (ExchangeVerificationStage(VerificationStage::WAITING) == VerificationStage::WAITING) {
            return true;
        }
        if (verifier::VerificationResultCache::Enabled()) {
            auto status = verifier::VerificationResultCache::Check(GetUniqId());
            switch (status) {
                case verifier::VerificationResultCache::Status::OK:
                    SetVerificationStage(VerificationStage::VERIFIED_OK);
                    LOG(INFO, VERIFIER) << "Verification result of method '" << GetFullName() << "' was cached: OK";
                    return true;
                case verifier::VerificationResultCache::Status::FAILED:
                    SetVerificationStage(VerificationStage::VERIFIED_FAIL);
                    LOG(INFO, VERIFIER) << "Verification result of method '" << GetFullName() << "' was cached: FAIL";
                    return true;
                default:
                    break;
            }
        }
        auto &job = panda::verifier::JobQueue::NewJob(*this);
        if (Invalid(job)) {
            LOG(INFO, VERIFIER) << "Method '" << GetFullName()
                                << "' cannot be enqueued for verification. Cannot create job object.";
            auto &runtime = *Runtime::GetCurrent();
            auto &&verif_options = runtime.GetVerificationOptions();
            auto does_not_fail = verif_options.Mode.VerifierDoesNotFail;
            SetVerificationStage(does_not_fail ? VerificationStage::VERIFIED_OK : VerificationStage::VERIFIED_FAIL);
            return true;
        }
        if (!panda::verifier::FillJob(job)) {
            LOG(INFO, VERIFIER) << "Method '" << GetFullName() << "' cannot be enqueued for verification";
            auto &runtime = *Runtime::GetCurrent();
            auto &&verif_options = runtime.GetVerificationOptions();
            auto does_not_fail = verif_options.Mode.VerifierDoesNotFail;
            SetVerificationStage(does_not_fail ? VerificationStage::VERIFIED_OK : VerificationStage::VERIFIED_FAIL);
            panda::verifier::JobQueue::DisposeJob(&job);
            return true;
        }
        panda::verifier::JobQueue::AddJob(job);
        LOG(INFO, VERIFIER) << "Method '" << GetFullName() << std::hex << "' ( 0x" << GetUniqId() << ", 0x"
                            << reinterpret_cast<uintptr_t>(this) << " ) enqueued for verification";
        return true;
    }

    return false;
}

void Method::EnqueueForVerification()
{
    if (GetVerificationStage() != VerificationStage::NOT_VERIFIED) {
        return;
    }
    auto &runtime = *Runtime::GetCurrent();
    auto &&verif_options = runtime.GetVerificationOptions();
    if (verif_options.Enable) {
        if (verif_options.Mode.DebugEnable) {
            auto hash = GetFullNameHash();
            PandaString class_name {ClassHelper::GetName(GetClassName().data)};
            auto class_hash = GetFullNameHashFromString(reinterpret_cast<const uint8_t *>(class_name.c_str()));
            panda::verifier::config::MethodIdCalculationHandler(class_hash, hash, GetUniqId());
        }

        bool is_system = false;
        if (!verif_options.Mode.DoNotAssumeLibraryMethodsVerified) {
            auto *klass = GetClass();
            if (klass != nullptr) {
                auto *file = klass->GetPandaFile();
                is_system = file != nullptr && verifier::JobQueue::IsSystemFile(file);
            }
        }
        if (!is_system && AddJobInQueue()) {
            return;
        }
    }
    if (verif_options.Show.Status) {
        LOG(INFO, VERIFIER) << "Verification result of method '" << GetFullName() << "': SKIP";
    }
    SetVerified(true);
}

Method::VerificationStage Method::GetVerificationStage() const
{
    return BitsToVerificationStage(stor_32_.access_flags_.load());
}

void Method::SetVerificationStage(VerificationStage stage)
{
    stor_32_.access_flags_.fetch_or((static_cast<uint32_t>(stage) << VERIFICATION_STATUS_SHIFT));
}

Method::VerificationStage Method::ExchangeVerificationStage(VerificationStage stage)
{
    return BitsToVerificationStage(
        stor_32_.access_flags_.fetch_or(static_cast<uint32_t>(stage) << VERIFICATION_STATUS_SHIFT));
}

Method::VerificationStage Method::BitsToVerificationStage(uint32_t bits)
{
    uint32_t val = (bits & VERIFICATION_STATUS_MASK) >> VERIFICATION_STATUS_SHIFT;
    // To avoid if - else for conversion set bit index to VerificationStage
    // y = 4x / 3 function for integers is used. It produces correct values for
    // all correct inputs:
    //                state  value __builtin_ffs 4x/3 VerificationStage
    //         not verified:  000        0        0     NOT_VERIFIED
    //              waiting:  100        3        3     WAITING
    // verification success:  110        2        2     VERIFIED_OK
    //  verification failed:  101        1        1     VERIFIED_FAIL
    return static_cast<VerificationStage>(4U * panda_bit_utils_ffs(val) / 3U);
}

void Method::StartProfiling()
{
    ASSERT(!ManagedThread::GetCurrent()->GetVM()->GetGC()->IsGCRunning() || Locks::mutator_lock->HasLock());

    // Some thread already started profiling
    if (IsProfilingWithoutLock()) {
        return;
    }

    mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
    PandaVector<uint32_t> vcalls;

    Span<const uint8_t> instructions(GetInstructions(), GetCodeSize());
    for (BytecodeInstruction inst(instructions.begin()); inst.GetAddress() < instructions.end();
         inst = inst.GetNext()) {
        if (inst.HasFlag(BytecodeInstruction::Flags::CALL_VIRT)) {
            vcalls.push_back(inst.GetAddress() - GetInstructions());
        }
    }
    if (vcalls.empty()) {
        return;
    }
    ASSERT(std::is_sorted(vcalls.begin(), vcalls.end()));

    auto data = allocator->Alloc(RoundUp(sizeof(ProfilingData), alignof(CallSiteInlineCache)) +
                                 sizeof(CallSiteInlineCache) * vcalls.size());
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto profiling_data = new (data) ProfilingData(vcalls.size());

    auto ics = profiling_data->GetInlineCaches();
    for (size_t i = 0; i < vcalls.size(); i++) {
        ics[i].Init(vcalls[i]);
    }

    ProfilingData *old_value = nullptr;
    while (!profiling_data_.compare_exchange_weak(old_value, profiling_data)) {
        if (old_value != nullptr) {
            // We're late, some thread already started profiling.
            allocator->Delete(data);
            return;
        }
    }
    EVENT_INTERP_PROFILING(events::InterpProfilingAction::START, GetFullName(), vcalls.size());
}

void Method::StopProfiling()
{
    ASSERT(!ManagedThread::GetCurrent()->GetVM()->GetGC()->IsGCRunning() || Locks::mutator_lock->HasLock());

    if (!IsProfilingWithoutLock()) {
        return;
    }

    EVENT_INTERP_PROFILING(events::InterpProfilingAction::STOP, GetFullName(),
                           GetProfilingData()->GetInlineCaches().size());

    mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
    allocator->Free(GetProfilingData());
    profiling_data_ = nullptr;
}

}  // namespace panda
