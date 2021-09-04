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

#ifndef PANDA_RUNTIME_INTERPRETER_VREGISTER_INL_H_
#define PANDA_RUNTIME_INTERPRETER_VREGISTER_INL_H_

#include "runtime/interpreter/vregister.h"

namespace panda::interpreter {

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<int32_t, M> {
    ALWAYS_INLINE static inline int32_t Get(const VRegisterIface<T> &vreg)
    {
        return vreg.Get();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<uint32_t, M> {
    ALWAYS_INLINE static inline uint32_t Get(const VRegisterIface<T> &vreg)
    {
        return vreg.Get();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<int8_t, M> {
    ALWAYS_INLINE static inline int8_t Get(const VRegisterIface<T> &vreg)
    {
        return static_cast<int8_t>(vreg.Get());
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<uint8_t, M> {
    ALWAYS_INLINE static inline uint8_t Get(const VRegisterIface<T> &vreg)
    {
        return static_cast<uint8_t>(vreg.Get());
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<int16_t, M> {
    ALWAYS_INLINE static inline int16_t Get(const VRegisterIface<T> &vreg)
    {
        return static_cast<int16_t>(vreg.Get());
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<uint16_t, M> {
    ALWAYS_INLINE static inline uint16_t Get(const VRegisterIface<T> &vreg)
    {
        return static_cast<uint16_t>(vreg.Get());
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<int64_t, M> {
    ALWAYS_INLINE static inline int64_t Get(const VRegisterIface<T> &vreg)
    {
        return vreg.GetLong();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<uint64_t, M> {
    ALWAYS_INLINE static inline uint64_t Get(const VRegisterIface<T> &vreg)
    {
        return vreg.GetLong();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<float, M> {
    ALWAYS_INLINE static inline float Get(const VRegisterIface<T> &vreg)
    {
        return vreg.GetFloat();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<double, M> {
    ALWAYS_INLINE static inline double Get(const VRegisterIface<T> &vreg)
    {
        return vreg.GetDouble();
    }
};

template <class T>
template <class M>
struct VRegisterIface<T>::ValueAccessor<ObjectHeader *, M> {
    ALWAYS_INLINE static inline ObjectHeader *Get(const VRegisterIface<T> &vreg)
    {
        return vreg.GetReference();
    }
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_VREGISTER_INL_H_
