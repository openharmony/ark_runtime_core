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

#ifndef PANDA_RUNTIME_MEM_REM_SET_H_
#define PANDA_RUNTIME_MEM_REM_SET_H_

#include "runtime/mem/gc/card_table.h"

namespace panda::mem {

using CardPtr = CardTable::CardPtr;
using CardList = PandaVector<CardPtr>;

class RemSetLockConfig {
public:
    using CommonLock = os::memory::Mutex;
    using DummyLock = os::memory::DummyLock;
};

class Region;

/**
 * \brief Set in the Region. To record the regions and cards reference to this region.
 */
template <typename LockConfigT = RemSetLockConfig::CommonLock>
class RemSet {
public:
    explicit RemSet(Region *region);

    ~RemSet();

    NO_COPY_SEMANTIC(RemSet);
    NO_MOVE_SEMANTIC(RemSet);

    void AddRef(const void *from_field_addr);

    template <typename ObjectVisitor>
    void VisitMarkedCards(const ObjectVisitor &object_visitor);

    void Clear();

    Region *GetRegion()
    {
        return region_;
    }

    CardList *GetCardList(Region *region);

    void SetCardTable(CardTable *card_table)
    {
        card_table_ = card_table;
    }

    /**
     * Used in the barrier. Record the reference from the region of obj_addr to the region of value_addr.
     * @param obj_addr - address of the object
     * @param value_addr - address of the reference in the field
     */
    static void AddRefWithAddr(const void *obj_addr, const void *value_addr);

    /**
     * Used in the barrier. Record the reference from the region of addr to the region of the reference in it's fields.
     * @param addr - address of the object
     */
    static void TraverseObjectToAddRef(const void *addr);

private:
    CardPtr GetCardPtr(const void *addr);

    MemRange GetMemoryRange(CardPtr card);

    Region *region_;
    LockConfigT rem_set_lock_;
    PandaUnorderedMap<Region *, CardList *> regions_ GUARDED_BY(rem_set_lock_);
    InternalAllocatorPtr allocator_;

    CardTable *card_table_ = nullptr;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REM_SET_H_
