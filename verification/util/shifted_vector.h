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

#ifndef PANDA_VERIFICATION_UTIL_SHIFTED_VECTOR_H_
#define PANDA_VERIFICATION_UTIL_SHIFTED_VECTOR_H_

#include "runtime/include/mem/panda_containers.h"

namespace panda::verifier {

template <const int SHIFT, typename T, template <typename...> class VECTOR = PandaVector>
class ShiftedVector : public VECTOR<T> {
    using Base = VECTOR<T>;

public:
    ShiftedVector() = default;
    ShiftedVector(typename Base::size_type size) : Base(size) {}
    ~ShiftedVector() = default;
    DEFAULT_MOVE_SEMANTIC(ShiftedVector);
    DEFAULT_COPY_SEMANTIC(ShiftedVector);

    typename Base::reference operator[](int idx)
    {
        return Base::operator[](static_cast<typename Base::size_type>(idx + SHIFT));
    }

    typename Base::const_reference &operator[](int idx) const
    {
        return Base::operator[](static_cast<typename Base::size_type>(idx + SHIFT));
    }

    typename Base::reference at(int idx)
    {
        return Base::at(static_cast<typename Base::size_type>(idx + SHIFT));
    }

    typename Base::const_reference &at(int idx) const
    {
        return Base::at(static_cast<typename Base::size_type>(idx + SHIFT));
    }

    bool InValidRange(int idx) const
    {
        return idx + SHIFT >= 0 && static_cast<typename Base::size_type>(idx + SHIFT) < Base::size();
    }

    int begin_index() const
    {
        return -SHIFT;
    }

    int end_index() const
    {
        return static_cast<int>(Base::size()) - SHIFT;
    }

    void ExtendToInclude(int idx)
    {
        if (idx >= end_index()) {
            Base::resize(Base::size() + static_cast<size_t>(idx - end_index() + 1));
        }
    }
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_SHIFTED_VECTOR_H_
