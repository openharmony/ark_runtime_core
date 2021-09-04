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

#ifndef PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_REFERENCE_PROCESSOR_H_
#define PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_REFERENCE_PROCESSOR_H_

#include "libpandabase/macros.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/mem/vm_handle.h"

namespace panda {

class ObjectHeader;
class BaseClass;

namespace mem {

enum class GCPhase;
class GC;
class Reference;

}  // namespace mem
}  // namespace panda

namespace panda::mem {

using ObjPtr = std::variant<const ObjectHeader *, coretypes::TaggedType *>;

/**
 * General language-independent interface for ReferenceProcessing.
 */
class ReferenceProcessor {
public:
    explicit ReferenceProcessor() = default;
    NO_COPY_SEMANTIC(ReferenceProcessor);
    NO_MOVE_SEMANTIC(ReferenceProcessor);
    virtual ~ReferenceProcessor() = 0;

    /**
     * True if current object is Reference and its referent is not marked yet (maybe need to process this reference)
     */
    virtual bool IsReference(const BaseClass *baseCls, const ObjectHeader *ref) = 0;

    /**
     * Process discovered Reference in the future. Called by GC in marking phase.
     */
    virtual void DelayReferenceProcessing(const BaseClass *baseCls, ObjPtr reference) = 0;

    /**
     * Handle reference with GC point of view (mark needed fields, if necessary)
     */
    virtual void HandleReference(GC *gc, PandaStackTL<ObjectHeader *> *objectsStack, const BaseClass *cls,
                                 ObjPtr object) = 0;

    /**
     * Process all references which we discovered by GC.
     */
    virtual void ProcessReferences(bool concurrent, bool clearSoftReferences, GCPhase gcPhase) = 0;

    /**
     * Collect all processed references. They were cleared in the previous phase - we only collect them.
     */
    virtual panda::mem::Reference *CollectClearedReferences() = 0;

    virtual void ScheduleForEnqueue(Reference *clearedReferences) = 0;

    /**
     * Enqueue cleared references to corresponding queue, if necessary.
     */
    virtual void Enqueue(panda::mem::Reference *clearedReferences) = 0;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_REFERENCE_PROCESSOR_REFERENCE_PROCESSOR_H_
