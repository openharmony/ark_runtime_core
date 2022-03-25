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

#ifndef PANDA_RUNTIME_SIGNAL_HANDLER_H_
#define PANDA_RUNTIME_SIGNAL_HANDLER_H_

#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <cstdint>
#include <iostream>
#include <vector>
#include <libpandabase/macros.h>
#include "runtime/include/mem/panda_containers.h"

namespace panda {

class Method;
class SignalHandler;

class SignalManager {
public:
    void InitSignals();

    bool IsInitialized()
    {
        return is_init_;
    }

    bool SignalActionHandler(int sig, siginfo_t *info, void *context);
    bool InOatCode(const siginfo_t *siginfo, const void *context, bool check_bytecode_pc);
    bool InOtherCode(int sig, siginfo_t *info, void *context);

    void AddHandler(SignalHandler *handler, bool oat_code);

    void RemoveHandler(SignalHandler *handler);
    void GetMethodAndReturnPcAndSp(const siginfo_t *siginfo, const void *context, const Method **out_method,
                                   const uintptr_t *out_return_pc, const uintptr_t *out_sp);

    mem::InternalAllocatorPtr GetAllocator()
    {
        return allocator_;
    }

    void DeleteHandlersArray();

    explicit SignalManager(mem::InternalAllocatorPtr allocator) : allocator_(allocator) {}
    SignalManager(SignalManager &&) = delete;
    SignalManager &operator=(SignalManager &&) = delete;
    virtual ~SignalManager() = default;

private:
    bool is_init_ {false};
    mem::InternalAllocatorPtr allocator_;
    PandaVector<SignalHandler *> oat_code_handler_;
    PandaVector<SignalHandler *> other_handlers_;
    NO_COPY_SEMANTIC(SignalManager);
};

class SignalHandler {
public:
    SignalHandler() = default;

    virtual bool Action(int sig, siginfo_t *siginfo, void *context) = 0;

    SignalHandler(SignalHandler &&) = delete;
    SignalHandler &operator=(SignalHandler &&) = delete;
    virtual ~SignalHandler() = default;

private:
    NO_COPY_SEMANTIC(SignalHandler);
};

class NullPointerHandler final : public SignalHandler {
public:
    NullPointerHandler() = default;

    bool Action(int sig, siginfo_t *siginfo, void *context) override;

    NullPointerHandler(NullPointerHandler &&) = delete;
    NullPointerHandler &operator=(NullPointerHandler &&) = delete;
    ~NullPointerHandler() override;

private:
    NO_COPY_SEMANTIC(NullPointerHandler);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_SIGNAL_HANDLER_H_
