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

#ifndef PANDA_VERIFICATION_UTIL_INVALID_REF_H_
#define PANDA_VERIFICATION_UTIL_INVALID_REF_H_

namespace panda::verifier {

template <typename T>
struct InvalidRef {
    operator T &() const
    {
        union {
            void *ptr;
            T &(InvalidRef::*m_ptr)() const;
        } u;
        u.m_ptr = &InvalidRef::operator T &;
        T *t = reinterpret_cast<T *>(u.ptr);
        return static_cast<T &>(*t);
    }
    bool operator==(const T &t) const
    {
        return &t == &operator T &();
    }
};

template <typename T>
T &Invalid()
{
    return InvalidRef<T>();
}

template <typename T>
bool Invalid(const T &t)
{
    return InvalidRef<T>() == t;
}

template <typename T>
bool Valid(const T &t)
{
    return !Invalid<T>(t);
}
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_INVALID_REF_H_
