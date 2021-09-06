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

#ifndef PANDA_RUNTIME_MEM_GC_DYNAMIC_GC_DYNAMIC_DATA_H_
#define PANDA_RUNTIME_MEM_GC_DYNAMIC_GC_DYNAMIC_DATA_H_

#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/mem/gc/gc_extension_data.h"

namespace panda::mem {
class GCDynamicData : public GCExtensionData {
public:
    explicit GCDynamicData(InternalAllocatorPtr a) : allocator(a)
    {
        dyn_weak_references_ = a->New<PandaStack<coretypes::TaggedType *>>(a->Adapter());

#ifndef NDEBUG
        SetLangType(LANG_TYPE_DYNAMIC);
#endif  // NDEBUG
    }

    ~GCDynamicData() override
    {
        allocator->Delete(dyn_weak_references_);
    }

    PandaStack<coretypes::TaggedType *> *GetDynWeakReferences()
    {
        return dyn_weak_references_;
    }

private:
    PandaStack<coretypes::TaggedType *> *dyn_weak_references_;
    InternalAllocatorPtr allocator;

    NO_COPY_SEMANTIC(GCDynamicData);
    NO_MOVE_SEMANTIC(GCDynamicData);
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_DYNAMIC_GC_DYNAMIC_DATA_H_
