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

#ifndef PANDA_RUNTIME_ENTRYPOINTS_ENTRYPOINTS_H_
#define PANDA_RUNTIME_ENTRYPOINTS_ENTRYPOINTS_H_

#include "runtime/include/method.h"

namespace panda {

class Frame;

extern "C" Frame *CreateFrame(uint32_t nregs, Method *method, Frame *prev);
extern "C" Frame *CreateFrameWithActualArgs(uint32_t nregs, uint32_t num_actual_args, Method *method, Frame *prev);
extern "C" Frame *CreateFrameWithActualArgsAndSize(uint32_t size, uint32_t nregs, uint32_t num_actual_args,
                                                   Method *method, Frame *prev);
extern "C" void FreeFrame(Frame *frame);
extern "C" void ThrowInstantiationErrorEntrypoint(Class *klass);

}  // namespace panda

#endif  // PANDA_RUNTIME_ENTRYPOINTS_ENTRYPOINTS_H_
