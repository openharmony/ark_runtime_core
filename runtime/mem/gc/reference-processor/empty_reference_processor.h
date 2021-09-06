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

#ifndef PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_EMPTY_REFERENCE_PROCESSOR_H_
#define PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_EMPTY_REFERENCE_PROCESSOR_H_

#include "reference_processor.h"

namespace panda::mem {

class EmptyReferenceProcessor : public panda::mem::ReferenceProcessor {
public:
    EmptyReferenceProcessor() = default;
    ~EmptyReferenceProcessor() override = default;

    bool IsReference([[maybe_unused]] const BaseClass *baseCls, [[maybe_unused]] const ObjectHeader *ref) override
    {
        return false;
    }

    void DelayReferenceProcessing([[maybe_unused]] const BaseClass *baseCls, [[maybe_unused]] ObjPtr reference) override
    {
    }

    void HandleReference([[maybe_unused]] GC *gc, [[maybe_unused]] PandaStackTL<ObjectHeader *> *objectsStack,
                         [[maybe_unused]] const BaseClass *baseClass, [[maybe_unused]] ObjPtr object) override
    {
    }

    void ProcessReferences([[maybe_unused]] bool concurrent, [[maybe_unused]] bool clearSoftReferences,
                           [[maybe_unused]] GCPhase gcPhase) override
    {
    }

    panda::mem::Reference *CollectClearedReferences() override
    {
        return nullptr;
    }

    void ScheduleForEnqueue([[maybe_unused]] Reference *clearedReferences) override {}

    void Enqueue([[maybe_unused]] panda::mem::Reference *clearedReferences) override {}

private:
    NO_COPY_SEMANTIC(EmptyReferenceProcessor);
    NO_MOVE_SEMANTIC(EmptyReferenceProcessor);
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_EMPTY_REFERENCE_PROCESSOR_H_
