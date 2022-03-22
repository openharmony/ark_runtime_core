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

#ifndef PANDA_RUNTIME_INCLUDE_COMPILER_INTERFACE_H_
#define PANDA_RUNTIME_INCLUDE_COMPILER_INTERFACE_H_

#include "libpandabase/macros.h"

namespace panda {

class Method;
class CompilerInterface {
public:
    CompilerInterface() = default;

    virtual bool CompileMethod(Method *method, uintptr_t bytecode_offset, bool osr) = 0;

    virtual void Destroy() = 0;

    virtual void PreZygoteFork() = 0;

    virtual void PostZygoteFork() = 0;

    virtual void *GetOsrCode(const Method *method) = 0;

    virtual void SetOsrCode(const Method *method, void *ptr) = 0;

    virtual void RemoveOsrCode(const Method *method) = 0;

    virtual ~CompilerInterface() = default;

    NO_COPY_SEMANTIC(CompilerInterface);
    NO_MOVE_SEMANTIC(CompilerInterface);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_COMPILER_INTERFACE_H_
