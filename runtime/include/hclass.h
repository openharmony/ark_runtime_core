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

#ifndef PANDA_RUNTIME_INCLUDE_HCLASS_H_
#define PANDA_RUNTIME_INCLUDE_HCLASS_H_

#include "runtime/include/class.h"

namespace panda {

namespace coretypes {
class DynClass;
}  // namespace coretypes

// Class for objects in DYNAMIC_CLASS languages like JavaScript
class HClass : public BaseClass {
public:
    static constexpr uint32_t HCLASS = 1U << 1U;
    static constexpr uint32_t STRING = 1U << 2U;
    static constexpr uint32_t ARRAY = 1U << 3U;
    static constexpr uint32_t NATIVE_POINTER = 1U << 4U;
    static constexpr uint32_t IS_DICTIONARY_ARRAY = 1U << 5U;
    static constexpr uint32_t IS_BUILTINS_CTOR = 1U << 6U;
    static constexpr uint32_t IS_CALLABLE = 1U << 7U;

    static constexpr uint32_t BITS_SIZE = 8;

public:
    HClass(uint32_t flags, panda_file::SourceLang lang) : BaseClass(lang)
    {
        SetFlags(flags | BaseClass::DYNAMIC_CLASS);
    }

    void SetFlags(uint32_t flags)
    {
        ASSERT(flags & BaseClass::DYNAMIC_CLASS);
        BaseClass::SetFlags(flags);
    }

    inline bool IsNativePointer() const
    {
        return (GetFlags() & NATIVE_POINTER) != 0;
    }

    inline bool IsArray() const
    {
        return (GetFlags() & ARRAY) != 0;
    }

    inline bool IsString() const
    {
        return (GetFlags() & STRING) != 0;
    }

    inline bool IsHClass() const
    {
        return (GetFlags() & HCLASS) != 0;
    }

    void SetDictionary()
    {
        uint32_t flags = BaseClass::GetFlags() | IS_DICTIONARY_ARRAY;
        ASSERT(flags & IS_DICTIONARY_ARRAY);
        BaseClass::SetFlags(flags);
    }

    bool IsDictionary() const
    {
        return (BaseClass::GetFlags() & IS_DICTIONARY_ARRAY) != 0U;
    }

    void SetBuiltinsCtorMode()
    {
        uint32_t flags = BaseClass::GetFlags() | IS_BUILTINS_CTOR;
        ASSERT(flags & IS_BUILTINS_CTOR);
        BaseClass::SetFlags(flags);
    }

    bool IsBuiltinsConstructor() const
    {
        return (BaseClass::GetFlags() & IS_BUILTINS_CTOR) != 0U;
    }

    void SetCallable(bool flag)
    {
        if (flag) {
            uint32_t flags = BaseClass::GetFlags() | IS_CALLABLE;
            ASSERT(flags & IS_CALLABLE);
            BaseClass::SetFlags(flags);
        } else {
            uint32_t flags = BaseClass::GetFlags() & (~(IS_CALLABLE));
            ASSERT(!(flags & IS_CALLABLE));
            BaseClass::SetFlags(flags);
        }
    }

    bool IsCallable() const
    {
        return (BaseClass::GetFlags() & IS_CALLABLE) != 0U;
    }

    ~HClass() = default;

    DEFAULT_COPY_SEMANTIC(HClass);
    DEFAULT_MOVE_SEMANTIC(HClass);

private:
    friend class coretypes::DynClass;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_HCLASS_H_
