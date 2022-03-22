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

#ifndef PANDA_RUNTIME_BRIDGE_BRIDGE_H_
#define PANDA_RUNTIME_BRIDGE_BRIDGE_H_

#include <cstdint>

namespace panda {

class Method;
class Frame;
class ManagedThread;

struct DecodedTaggedValue {
    DecodedTaggedValue() = default;
    DecodedTaggedValue(int64_t v, int64_t t) : value(v), tag(t) {}

    int64_t value;  // NOLINT(misc-non-private-member-variables-in-classes)
    int64_t tag;    // NOLINT(misc-non-private-member-variables-in-classes)
};

inline bool operator==(const DecodedTaggedValue &v1, const DecodedTaggedValue &v2)
{
    return v1.value == v2.value && v1.tag == v2.tag;
}

inline bool operator!=(const DecodedTaggedValue &v1, const DecodedTaggedValue &v2)
{
    return !(v1 == v2);
}
extern "C" void InterpreterToCompiledCodeBridge(const uint8_t *, const Frame *, const Method *, ManagedThread *);
extern "C" void InterpreterToCompiledCodeBridgeDyn(const uint8_t *, const Frame *, const Method *, ManagedThread *);
extern "C" DecodedTaggedValue InvokeCompiledCodeWithArgArray(const int64_t *, const Frame *, const Method *,
                                                             ManagedThread *);
extern "C" DecodedTaggedValue InvokeCompiledCodeWithArgArrayDyn(const int64_t *, uint32_t, const Frame *,
                                                                const Method *, ManagedThread *);

extern "C" int64_t InvokeInterpreter(ManagedThread *thread, const uint8_t *pc, Frame *frame, Frame *last_frame);

const void *GetCompiledCodeToInterpreterBridge(const Method *method);

}  // namespace panda

#endif  // PANDA_RUNTIME_BRIDGE_BRIDGE_H_
