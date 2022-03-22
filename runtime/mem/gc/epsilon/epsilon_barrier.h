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

#ifndef PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_BARRIER_H_
#define PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_BARRIER_H_

namespace panda::mem {

template <typename Compiler>
class EpsilonBarrier {
public:
    typename Compiler::Node *GenerateBarrier();
};

// Temporary make it c style - it is easier for the JIT to call this
extern "C" void CallEpsilonBarrier();

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_BARRIER_H_
