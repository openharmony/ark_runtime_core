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

#ifndef PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_H_
#define PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_H_

#include "libpandabase/macros.h"
#include "libpandabase/mem/mem.h"

namespace panda::mem {
class ReferenceStorage;
class RefBlock;
namespace test {
class ReferenceStorageTest;
}  // namespace test
}  // namespace panda::mem

namespace panda::mem {

class GlobalObjectStorage;

class Reference {
public:
    enum class ObjectType : uint8_t {
        /**
         * Used for objects on stack (arguments for methods)
         */
        STACK = 0,
        /**
         * Local references which were created by NewLocalRef JNI method
         */
        LOCAL = 1,
        /**
         * Local references which were created by NewGlobalRef JNI method
         */
        GLOBAL = 2,
        /**
         * Local references which were created by NewWeakGlobalRef JNI method
         */
        WEAK = 3,
        ENUM_SIZE
    };

    DEFAULT_COPY_SEMANTIC(Reference);
    DEFAULT_MOVE_SEMANTIC(Reference);
    ~Reference() = delete;
    Reference() = delete;

    bool IsStack() const
    {
        return GetType() == ObjectType::STACK;
    }

    bool IsLocal() const
    {
        ObjectType type = GetType();
        return type == ObjectType::STACK || type == ObjectType::LOCAL;
    }

    bool IsGlobal() const
    {
        return GetType() == ObjectType::GLOBAL;
    }

    bool IsWeak() const
    {
        return GetType() == ObjectType::WEAK;
    }

private:
    static constexpr auto MASK_TYPE = 3U;
    static constexpr auto MASK_WITHOUT_TYPE = sizeof(uintptr_t) == sizeof(uint64_t)
                                                  ? std::numeric_limits<uint64_t>::max() - MASK_TYPE
                                                  : std::numeric_limits<uint32_t>::max() - MASK_TYPE;

    static Reference *CreateWithoutType(uintptr_t addr)
    {
        ASSERT((addr & MASK_TYPE) == 0);
        return reinterpret_cast<Reference *>(addr);
    }

    static Reference *Create(uintptr_t addr, ObjectType type)
    {
        ASSERT((addr & MASK_TYPE) == 0);
        return SetType(addr, type);
    }

    static ObjectType GetType(const Reference *ref)
    {
        auto addr = ToUintPtr(ref);
        return static_cast<ObjectType>(addr & MASK_TYPE);
    }

    static Reference *SetType(Reference *ref, ObjectType type)
    {
        auto addr = ToUintPtr(ref);
        return SetType(addr, type);
    }

    static Reference *SetType(uintptr_t addr, ObjectType type)
    {
        ASSERT((addr & MASK_TYPE) == 0);
        return reinterpret_cast<Reference *>(addr | static_cast<uintptr_t>(type));
    }

    ObjectType GetType() const
    {
        return Reference::GetType(this);
    }

    static Reference *GetRefWithoutType(const Reference *ref)
    {
        auto addr = ToUintPtr(ref);
        return reinterpret_cast<Reference *>(addr & MASK_WITHOUT_TYPE);
    }

    friend class panda::mem::ReferenceStorage;
    friend class panda::mem::GlobalObjectStorage;
    friend class panda::mem::RefBlock;
    friend class panda::mem::test::ReferenceStorageTest;
};
}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_H_
