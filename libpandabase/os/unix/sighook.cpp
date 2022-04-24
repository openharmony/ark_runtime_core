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

#include <dlfcn.h>
#include <errno.h>   // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <stdio.h>   // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <stdlib.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <string.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <array>

#include "utils/logger.h"
#include "os/sighook.h"

#include <algorithm>
#include <initializer_list>
#include <os/mutex.h>
#include <type_traits>
#include <utility>
#include <unistd.h>

#include <securec.h>
#include <ucontext.h>

namespace panda {
static decltype(&sigaction) real_sigaction;
static decltype(&sigprocmask) real_sigprocmask;
static bool g_is_init_really {false};
static bool g_is_init_key_create {false};
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
#if PANDA_TARGET_MACOS
__attribute__((init_priority(101))) static os::memory::Mutex real_lock;
#else
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
static os::memory::Mutex real_lock;
#endif
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
static os::memory::Mutex key_create_lock;

static os::memory::PandaThreadKey GetHandlingSignalKey()
{
    static os::memory::PandaThreadKey key;
    {
        os::memory::LockHolder lock(key_create_lock);
        if (!g_is_init_key_create) {
            int rc = os::memory::PandaThreadKeyCreate(&key, nullptr);
            if (rc != 0) {
                LOG(FATAL, RUNTIME) << "Failed to create sigchain thread key: " << os::Error(rc).ToString();
            }
            g_is_init_key_create = true;
        }
    }
    return key;
}

static bool GetHandlingSignal()
{
    void *result = os::memory::PandaGetspecific(GetHandlingSignalKey());
    return reinterpret_cast<uintptr_t>(result) != 0;
}

static void SetHandlingSignal(bool value)
{
    os::memory::PandaSetspecific(GetHandlingSignalKey(), reinterpret_cast<void *>(static_cast<uintptr_t>(value)));
}

class SignalHook {
public:
    SignalHook() = default;

    ~SignalHook() = default;

    NO_COPY_SEMANTIC(SignalHook);
    NO_MOVE_SEMANTIC(SignalHook);

    bool IsHook() const
    {
        return is_hook_;
    }

    void HookSig(int signo)
    {
        if (!is_hook_) {
            RegisterAction(signo);
            is_hook_ = true;
        }
    }

    void RegisterAction(int signo)
    {
        struct sigaction handler_action = {};
        sigfillset(&handler_action.sa_mask);
        // SIGSEGV from signal handler must be handled as well
        sigdelset(&handler_action.sa_mask, SIGSEGV);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        handler_action.sa_sigaction = SignalHook::Handler;
        // SA_NODEFER+: do not block signals from the signal handler
        // SA_ONSTACK-: call signal handler on the same stack
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        handler_action.sa_flags = SA_RESTART | SA_SIGINFO | SA_NODEFER;
        real_sigaction(signo, nullptr, &old_action_);
        real_sigaction(signo, &handler_action, &user_action_);
    }

    void RegisterHookAction(const SighookAction *sa)
    {
        for (SighookAction &handler : hook_action_handlers_) {
            if (handler.sc_sigaction == nullptr) {
                handler = *sa;
                return;
            }
        }
        LOG(FATAL, RUNTIME) << "Failed to register hook action, too many handlers";
    }

    void RegisterUserAction(const struct sigaction *new_action)
    {
        user_action_register_ = true;
        if constexpr (std::is_same_v<decltype(user_action_), struct sigaction>) {
            user_action_ = *new_action;
        } else {
            user_action_.sa_flags = new_action->sa_flags;      // NOLINT
            user_action_.sa_handler = new_action->sa_handler;  // NOLINT
#if defined(SA_RESTORER)
            user_action_.sa_restorer = new_action->sa_restorer;  // NOLINT
#endif
            sigemptyset(&user_action_.sa_mask);
            (void)memcpy_s(&user_action_.sa_mask, sizeof(user_action_.sa_mask), &new_action->sa_mask,
                           std::min(sizeof(user_action_.sa_mask), sizeof(new_action->sa_mask)));
        }
    }

    struct sigaction GetUserAction() const
    {
        if constexpr (std::is_same_v<decltype(user_action_), struct sigaction>) {
            return user_action_;
        } else {
            struct sigaction result {
            };
            result.sa_flags = user_action_.sa_flags;      // NOLINT
            result.sa_handler = user_action_.sa_handler;  // NOLINT
#if defined(SA_RESTORER)
            result.sa_restorer = user_action_.sa_restorer;
#endif
            (void)memcpy_s(&result.sa_mask, sizeof(result.sa_mask), &user_action_.sa_mask,
                           std::min(sizeof(user_action_.sa_mask), sizeof(result.sa_mask)));
            return result;
        }
    }

    static void Handler(int signo, siginfo_t *siginfo, void *ucontext_raw);
    static void CallOldAction(int signo, siginfo_t *siginfo, void *ucontext_raw);

    void RemoveHookAction(bool (*action)(int, siginfo_t *, void *))
    {
        for (size_t i = 0; i < HOOK_LENGTH; ++i) {
            if (hook_action_handlers_[i].sc_sigaction == action) {
                for (size_t j = i; j < HOOK_LENGTH - 1; ++j) {
                    hook_action_handlers_[j] = hook_action_handlers_[j + 1];
                }
                hook_action_handlers_[HOOK_LENGTH - 1].sc_sigaction = nullptr;
                return;
            }
        }
        LOG(FATAL, RUNTIME) << "Failed to find removed hook handler";
    }

    bool IsUserActionRegister() const
    {
        return user_action_register_;
    }

    void ClearHookActionHandlers()
    {
        for (SighookAction &handler : hook_action_handlers_) {
            handler.sc_sigaction = nullptr;
        }
    }

private:
    static bool SetHandlingSignal(int signo, siginfo_t *siginfo, void *ucontext_raw);

    constexpr static const int HOOK_LENGTH = 2;
    bool is_hook_ {false};
    std::array<SighookAction, HOOK_LENGTH> hook_action_handlers_ {};
    struct sigaction user_action_ {
    };
    struct sigaction old_action_ = {};
    bool user_action_register_ {false};
};

static std::array<SignalHook, _NSIG + 1> signal_hooks;

void SignalHook::CallOldAction(int signo, siginfo_t *siginfo, void *ucontext_raw)
{
    auto handler_flags = static_cast<size_t>(signal_hooks[signo].old_action_.sa_flags);
    sigset_t mask = signal_hooks[signo].old_action_.sa_mask;
    real_sigprocmask(SIG_SETMASK, &mask, nullptr);

    if ((handler_flags & SA_SIGINFO)) {                                              // NOLINT
        signal_hooks[signo].old_action_.sa_sigaction(signo, siginfo, ucontext_raw);  // NOLINT
    } else {
        if (signal_hooks[signo].old_action_.sa_handler == nullptr) {  // NOLINT
            real_sigaction(signo, &signal_hooks[signo].old_action_, nullptr);
            kill(getpid(), signo);  // Send signal again
            return;
        }
        signal_hooks[signo].old_action_.sa_handler(signo);  // NOLINT
    }
}

bool SignalHook::SetHandlingSignal(int signo, siginfo_t *siginfo, void *ucontext_raw)
{
    for (const auto &handler : signal_hooks[signo].hook_action_handlers_) {
        if (handler.sc_sigaction == nullptr) {
            break;
        }

        bool handler_noreturn = ((handler.sc_flags & SIGHOOK_ALLOW_NORETURN) != 0);
        sigset_t previous_mask;
        real_sigprocmask(SIG_SETMASK, &handler.sc_mask, &previous_mask);

        bool old_handle_key = GetHandlingSignal();
        if (!handler_noreturn) {
            ::panda::SetHandlingSignal(true);
        }
        if (handler.sc_sigaction(signo, siginfo, ucontext_raw)) {
            ::panda::SetHandlingSignal(old_handle_key);
            return false;
        }

        real_sigprocmask(SIG_SETMASK, &previous_mask, nullptr);
        ::panda::SetHandlingSignal(old_handle_key);
    }

    return true;
}

void SignalHook::Handler(int signo, siginfo_t *siginfo, void *ucontext_raw)
{
    if (!GetHandlingSignal()) {
        if (!SetHandlingSignal(signo, siginfo, ucontext_raw)) {
            return;
        }
    }

    // If not set user handler, call linker handler
    if (!signal_hooks[signo].IsUserActionRegister()) {
        CallOldAction(signo, siginfo, ucontext_raw);
        return;
    }

    // Call user handler
    auto handler_flags = static_cast<size_t>(signal_hooks[signo].user_action_.sa_flags);
    auto *ucontext = static_cast<ucontext_t *>(ucontext_raw);
    sigset_t mask;
    sigemptyset(&mask);
    constexpr size_t N = sizeof(sigset_t) * 2;
    for (size_t i = 0; i < N; ++i) {
        if (sigismember(&ucontext->uc_sigmask, i) == 1 ||
            sigismember(&signal_hooks[signo].user_action_.sa_mask, i) == 1) {
            sigaddset(&mask, i);
        }
    }

    if ((handler_flags & SA_NODEFER) == 0) {  // NOLINT
        sigaddset(&mask, signo);
    }
    real_sigprocmask(SIG_SETMASK, &mask, nullptr);

    if ((handler_flags & SA_SIGINFO)) {                                               // NOLINT
        signal_hooks[signo].user_action_.sa_sigaction(signo, siginfo, ucontext_raw);  // NOLINT
    } else {
        auto handler = signal_hooks[signo].user_action_.sa_handler;  // NOLINT
        if (handler == SIG_IGN) {                                    // NOLINT
            return;
        }
        if (handler == SIG_DFL) {  // NOLINT
            LOG(FATAL, RUNTIME) << "Actually signal:" << signo << " | register sigaction's handler == SIG_DFL";
        }
        handler(signo);
    }

    // If user handler does not exit, continue to call Old Action
    CallOldAction(signo, siginfo, ucontext_raw);
}

template <typename Sigaction>
static bool FindRealSignal(Sigaction *real_fun, [[maybe_unused]] Sigaction hook_fun, const char *name)
{
    void *find_fun = dlsym(RTLD_NEXT, name);
    if (find_fun != nullptr) {
        *real_fun = reinterpret_cast<Sigaction>(find_fun);
    } else {
        find_fun = dlsym(RTLD_DEFAULT, name);
        if (find_fun == nullptr || reinterpret_cast<uintptr_t>(find_fun) == reinterpret_cast<uintptr_t>(hook_fun) ||
            reinterpret_cast<uintptr_t>(find_fun) == reinterpret_cast<uintptr_t>(sigaction)) {
            LOG(ERROR, RUNTIME) << "dlsym(RTLD_DEFAULT, " << name << ") can not find really " << name;
            return false;
        }
        *real_fun = reinterpret_cast<Sigaction>(find_fun);
    }
    LOG(INFO, RUNTIME) << "Find " << name << " success";
    return true;
}

#if PANDA_TARGET_MACOS
__attribute__((constructor(102))) static bool InitRealSignalFun()
#else
__attribute__((constructor)) static bool InitRealSignalFun()
#endif
{
    {
        os::memory::LockHolder lock(real_lock);
        if (!g_is_init_really) {
            bool is_error = true;
            is_error = is_error && FindRealSignal(&real_sigaction, sigaction, "sigaction");
            is_error = is_error && FindRealSignal(&real_sigprocmask, sigprocmask, "sigprocmask");
            if (is_error) {
                g_is_init_really = true;
            }
            return is_error;
        }
    }
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming)
static int RegisterUserHandler(int signal, const struct sigaction *new_action, struct sigaction *old_action,
                               int (*really)(int, const struct sigaction *, struct sigaction *))
{
    // Just hook signal in range, otherwise use libc sigaction
    if (signal <= 0 || signal >= _NSIG) {
        LOG(ERROR, RUNTIME) << "Illegal signal " << signal;
        return -1;
    }

    if (signal_hooks[signal].IsHook()) {
        auto user_action = signal_hooks[signal].SignalHook::GetUserAction();
        if (new_action != nullptr) {
            signal_hooks[signal].RegisterUserAction(new_action);
        }
        if (old_action != nullptr) {
            *old_action = user_action;
        }
        return 0;
    }

    return really(signal, new_action, old_action);
}

int RegisterUserMask(int how, const sigset_t *new_set, sigset_t *old_set,
                     int (*really)(int, const sigset_t *, sigset_t *))
{
    if (GetHandlingSignal()) {
        return really(how, new_set, old_set);
    }

    if (new_set == nullptr) {
        return really(how, new_set, old_set);
    }

    sigset_t build_sigset = *new_set;
    if (how == SIG_BLOCK || how == SIG_SETMASK) {
        for (int i = 1; i < _NSIG; ++i) {
            if (signal_hooks[i].IsHook() && (sigismember(&build_sigset, i) != 0)) {
                sigdelset(&build_sigset, i);
            }
        }
    }
    const sigset_t *build_sigset_const = &build_sigset;
    return really(how, build_sigset_const, old_set);
}

// NOTE: issue #2681
// Using ADDRESS_SANITIZER will expose a bug. Try to define 'sigaction' which will make SIGSEGV happen
#ifdef USE_ADDRESS_SANITIZER
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int sigaction([[maybe_unused]] int __sig, [[maybe_unused]] const struct sigaction *__restrict __act,
                         [[maybe_unused]] struct sigaction *__oact)  // NOLINT(readability-identifier-naming)
{
    if (!InitRealSignalFun()) {
        return -1;
    }
    return RegisterUserHandler(__sig, __act, __oact, real_sigaction);
}
#else
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int sigactionStub([[maybe_unused]] int __sig, [[maybe_unused]] const struct sigaction *__restrict __act,
                             [[maybe_unused]] struct sigaction *__oact)  // NOLINT(readability-identifier-naming)
{
    if (!InitRealSignalFun()) {
        return -1;
    }
    return RegisterUserHandler(__sig, __act, __oact, real_sigaction);
}
#endif  // USE_ADDRESS_SANITIZER

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int sigprocmask(int how, const sigset_t *new_set, sigset_t *old_set)  // NOLINT
{
    if (!InitRealSignalFun()) {
        return -1;
    }
    return RegisterUserMask(how, new_set, old_set, real_sigprocmask);
}

extern "C" void RegisterHookHandler(int signal, const SighookAction *sa)
{
    if (!InitRealSignalFun()) {
        return;
    }

    if (signal <= 0 || signal >= _NSIG) {
        LOG(FATAL, RUNTIME) << "Illegal signal " << signal;
    }

    signal_hooks[signal].RegisterHookAction(sa);
    signal_hooks[signal].HookSig(signal);
}

extern "C" void RemoveHookHandler(int signal, bool (*action)(int, siginfo_t *, void *))
{
    if (!InitRealSignalFun()) {
        return;
    }

    if (signal <= 0 || signal >= _NSIG) {
        LOG(FATAL, RUNTIME) << "Illegal signal " << signal;
    }

    signal_hooks[signal].RemoveHookAction(action);
}

extern "C" void CheckOldHookHandler(int signal)
{
    if (!InitRealSignalFun()) {
        return;
    }

    if (signal <= 0 || signal >= _NSIG) {
        LOG(FATAL, RUNTIME) << "Illegal signal " << signal;
    }

    // Get old action
    struct sigaction old_action {
    };
    real_sigaction(signal, nullptr, &old_action);

    if (old_action.sa_sigaction != SignalHook::Handler) {  // NOLINT
        LOG(ERROR, RUNTIME) << "Error: check old hook handler found unexpected action "
                            << (old_action.sa_sigaction != nullptr);  // NOLINT
        signal_hooks[signal].RegisterAction(signal);
    }
}

extern "C" void AddSpecialSignalHandlerFn(int signal, SigchainAction *sa)
{
    LOG(DEBUG, RUNTIME) << "Panda sighook RegisterHookHandler is used, signal:" << signal << " action:" << sa;
    RegisterHookHandler(signal, reinterpret_cast<SighookAction *>(sa));
}

extern "C" void RemoveSpecialSignalHandlerFn(int signal, bool (*fn)(int, siginfo_t *, void *))
{
    LOG(DEBUG, RUNTIME) << "Panda sighook RemoveHookHandler is used, signal:"
                        << "sigaction";
    RemoveHookHandler(signal, fn);
}

extern "C" void EnsureFrontOfChain(int signal)
{
    LOG(DEBUG, RUNTIME) << "Panda sighook CheckOldHookHandler is used, signal:" << signal;
    CheckOldHookHandler(signal);
}

void ClearSignalHooksHandlersArray()
{
    g_is_init_really = false;
    g_is_init_key_create = false;
    for (int i = 1; i < _NSIG; i++) {
        signal_hooks[i].ClearHookActionHandlers();
    }
}
}  // namespace panda
