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

#include "signal_handler.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstdlib>
#include "include/method.h"
#include <sys/ucontext.h>
#include "include/panda_vm.h"
#include "include/thread.h"
#include "include/stack_walker.h"
#if defined(PANDA_TARGET_UNIX)
#include "libpandabase/os/unix/sighooklib/sighook.h"
#endif  // PANDA_TARGET_UNIX

namespace panda {

#ifdef PANDA_TARGET_ARM32
#define CONTEXT_PC uc_->uc_mcontext.arm_pc  // NOLINT(cppcoreguidelines-macro-usage)
#define CONTEXT_FP uc_->uc_mcontext.arm_fp  // NOLINT(cppcoreguidelines-macro-usage)
#elif defined(PANDA_TARGET_ARM64)
#define CONTEXT_PC uc_->uc_mcontext.pc        // NOLINT(cppcoreguidelines-macro-usage)
#define CONTEXT_FP uc_->uc_mcontext.regs[29]  // NOLINT(cppcoreguidelines-macro-usage)
#elif defined(PANDA_TARGET_AMD64)
#define CONTEXT_PC uc_->uc_mcontext.gregs[REG_RIP]  // NOLINT(cppcoreguidelines-macro-usage)
#define CONTEXT_FP uc_->uc_mcontext.gregs[REG_RBP]  // NOLINT(cppcoreguidelines-macro-usage)
#elif defined(PANDA_TARGET_X86)
#define CONTEXT_PC uc_->uc_mcontext.gregs[REG_EIP]  // NOLINT(cppcoreguidelines-macro-usage)
#define CONTEXT_FP uc_->uc_mcontext.gregs[REG_EBP]  // NOLINT(cppcoreguidelines-macro-usage)
#endif

class SignalContext {
public:
    explicit SignalContext(void *ucontext_raw)
    {
        uc_ = reinterpret_cast<ucontext_t *>(ucontext_raw);
    }
    uintptr_t GetPC()
    {
        return CONTEXT_PC;
    }
    void SetPC(uintptr_t pc)
    {
        CONTEXT_PC = pc;
    }
    uintptr_t *GetFP()
    {
        return reinterpret_cast<uintptr_t *>(CONTEXT_FP);
    }

private:
    ucontext_t *uc_;
};

static bool IsValidStack([[maybe_unused]] ManagedThread *thread)
{
    // Issue #3649 CFrame::Initialize fires an ASSERT fail.
    // The issue is that ManagedStack is not always in a consistent state.
    return false;
}

// Something goes really wrong. Dump info and exit.
static void DumpStackTrace([[maybe_unused]] int signo, [[maybe_unused]] siginfo_t *info, [[maybe_unused]] void *context)
{
    auto thread = ManagedThread::GetCurrent();
    if (thread == nullptr) {
        LOG(ERROR, RUNTIME) << "Native thread segmentation fault";
        return;
    }
    if (!IsValidStack(thread)) {
        return;
    }

    LOG(ERROR, RUNTIME) << "Managed thread segmentation fault";
    for (StackWalker stack(thread); stack.HasFrame(); stack.NextFrame()) {
        Method *method = stack.GetMethod();
        auto *source = method->GetClassSourceFile().data;
        auto line_num = method->GetLineNumFromBytecodeOffset(stack.GetBytecodePc());
        if (source == nullptr) {
            source = utf::CStringAsMutf8("<unknown>");
        }
        LOG(ERROR, RUNTIME) << method->GetClass()->GetName() << "." << method->GetName().data << " at " << source << ":"
                            << line_num;
    }
}

static void UseDebuggerdSignalHandler(int sig)
{
    LOG(WARNING, RUNTIME) << "panda vm can not handle sig " << sig << ", call next handler";
}

static bool CallSignalActionHandler(int sig, siginfo_t *info, void *context)
{  // NOLINT
    return Runtime::GetCurrent()->GetSignalManager()->SignalActionHandler(sig, info, context);
}

bool SignalManager::SignalActionHandler(int sig, siginfo_t *info, void *context)
{
    if (InOatCode(info, context, true)) {
        for (const auto &handler : oat_code_handler_) {
            if (handler->Action(sig, info, context)) {
                return true;
            }
        }
    }

    // a signal can not handle in oat
    if (InOtherCode(sig, info, context)) {
        return true;
    }

    // Use the default exception handler function.
    UseDebuggerdSignalHandler(sig);
    return false;
}

bool SignalManager::InOatCode([[maybe_unused]] const siginfo_t *siginfo, [[maybe_unused]] const void *context,
                              [[maybe_unused]] bool check_bytecode_pc)
{
    return true;
}

bool SignalManager::InOtherCode([[maybe_unused]] int sig, [[maybe_unused]] siginfo_t *info,
                                [[maybe_unused]] void *context)
{
    return false;
}

void SignalManager::AddHandler(SignalHandler *handler, bool oat_code)
{
    if (oat_code) {
        oat_code_handler_.push_back(handler);
    } else {
        other_handlers_.push_back(handler);
    }
}

void SignalManager::RemoveHandler(SignalHandler *handler)
{
    auto it_oat = std::find(oat_code_handler_.begin(), oat_code_handler_.end(), handler);
    if (it_oat != oat_code_handler_.end()) {
        oat_code_handler_.erase(it_oat);
        return;
    }
    auto it_other = std::find(other_handlers_.begin(), other_handlers_.end(), handler);
    if (it_other != other_handlers_.end()) {
        other_handlers_.erase(it_other);
        return;
    }
    LOG(FATAL, RUNTIME) << "handler doesn't exist: " << handler;
}

void SignalManager::InitSignals()
{
    if (is_init_) {
        return;
    }
#if defined(PANDA_TARGET_UNIX)
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGABRT);
    sigdelset(&mask, SIGBUS);
    sigdelset(&mask, SIGFPE);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);

    ClearSignalHooksHandlersArray();

    // If running on device, sigchain will work and AddSpecialSignalHandlerFn in sighook will not be used
    SigchainAction sigchain_action = {
        CallSignalActionHandler,
        mask,
        SA_SIGINFO,
    };
    AddSpecialSignalHandlerFn(SIGSEGV, &sigchain_action);

    is_init_ = true;

    for (auto tmp : oat_code_handler_) {
        allocator_->Delete(tmp);
    }
    oat_code_handler_.clear();
    for (auto tmp : other_handlers_) {
        allocator_->Delete(tmp);
    }
    other_handlers_.clear();
#else
    struct sigaction act1 = {};
    sigfillset(&act1.sa_mask);
    act1.sa_sigaction = RuntimeSEGVHandler;
    act1.sa_flags = SA_RESTART | SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &act1, nullptr);
#endif  // PANDA_TARGET_UNIX
}

void SignalManager::GetMethodAndReturnPcAndSp([[maybe_unused]] const siginfo_t *siginfo,
                                              [[maybe_unused]] const void *context,
                                              [[maybe_unused]] const Method **out_method,
                                              [[maybe_unused]] const uintptr_t *out_return_pc,
                                              [[maybe_unused]] const uintptr_t *out_sp)
{
    // just stub now
}

void SignalManager::DeleteHandlersArray()
{
    if (is_init_) {
        for (auto tmp : oat_code_handler_) {
            allocator_->Delete(tmp);
        }
        oat_code_handler_.clear();
        for (auto tmp : other_handlers_) {
            allocator_->Delete(tmp);
        }
        other_handlers_.clear();
        RemoveSpecialSignalHandlerFn(SIGSEGV, CallSignalActionHandler);
        is_init_ = false;
    }
}

#if defined(PANDA_TARGET_UNIX)
bool DetectSEGVFromMemory([[maybe_unused]] int sig, [[maybe_unused]] siginfo_t *siginfo,
#else
void DetectSEGVFromMemory([[maybe_unused]] int sig, [[maybe_unused]] siginfo_t *siginfo,
#endif                                    // PANDA_TARGET_UNIX
                          void *context)  // CODECHECK-NOLINT(C_RULE_ID_INDENT_CHECK)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    auto mem_fault_location = ToUintPtr(siginfo->si_addr);
    const uintptr_t MAX_OBJECT_SIZE = 1U << 30U;
    // The expected fault address is nullptr and offset is within the object
    if (mem_fault_location > MAX_OBJECT_SIZE) {
        DumpStackTrace(sig, siginfo, context);
#if defined(PANDA_TARGET_UNIX)
        return true;
#else
        LOG(FATAL, RUNTIME) << "Memory location which caused fault:" << std::hex << mem_fault_location;
#endif
    }
#if defined(PANDA_TARGET_UNIX)
    return false;
#endif
}

#if defined(PANDA_TARGET_UNIX)
bool RuntimeSEGVHandler([[maybe_unused]] int sig, [[maybe_unused]] siginfo_t *siginfo, void *context)
{
    return !DetectSEGVFromMemory(sig, siginfo, context);
}
#else
void RuntimeSEGVHandler([[maybe_unused]] int sig, [[maybe_unused]] siginfo_t *siginfo, void *context)
{
    DetectSEGVFromMemory(sig, siginfo, context);
}
#endif  // PANDA_TARGET_UNIX

bool NullPointerHandler::Action(int sig, [[maybe_unused]] siginfo_t *siginfo, [[maybe_unused]] void *context)
{
    if (sig != SIGSEGV) {
        return false;
    }
#if defined(PANDA_TARGET_UNIX)
    if (!RuntimeSEGVHandler(sig, siginfo, context)) {
        return false;
    }
#endif  // PANDA_TARGET_UNIX
    LOG(DEBUG, RUNTIME) << "NullPointerHandler happen, Throw NullPointerHandler Exception, signal:" << sig;
    // Issues 1437, NullPointer has been checked here or in aot,
    // so let's return to interpreter, and exception is not built here.
    // panda::ThrowNullPointerException()
    return true;
}

NullPointerHandler::~NullPointerHandler() = default;

}  // namespace panda
