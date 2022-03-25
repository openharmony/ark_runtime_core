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

#include "runtime/include/tooling/pt_thread.h"

namespace panda::tooling {
static constexpr uint32_t PT_THREAD_NONE_ID = 0xffffffff;

/* static */
const PtThread PtThread::NONE = PtThread(PT_THREAD_NONE_ID);  // NOLINT(fuchsia-statically-constructed-objects)
}  // namespace panda::tooling
