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

#ifndef PANDA_RUNTIME_MEM_GC_GC_REASON_H_
#define PANDA_RUNTIME_MEM_GC_GC_REASON_H_

namespace panda::mem {

enum class GCReason {
    GC_REASON_OOM,
    GC_REASON_HEAP_OCCUPANCY_THRESHOLD,
};
}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_REASON_H_
