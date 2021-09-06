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

#ifndef PANDA_RUNTIME_MEM_GC_LANG_GC_LANG_H_
#define PANDA_RUNTIME_MEM_GC_LANG_GC_LANG_H_

#include "runtime/mem/gc/gc.h"

namespace panda::mem {

// GCLang class is an interlayer between language-agnostic GC class and different implementations of GC.
// It contains language-specific methods that are used in several types of GC (such as StwGC, GenGC, etc.)
//
//                              GC
//                              ^
//                              |
//                       GCLang<SpecificLanguage>
//                       ^           ^    ...   ^
//                       |           |    ...   |
// 	                    /            |    ...
//                     /             |    ...
// StwGC<SpecificLanguage> GenGC<SpecificLanguage> ...

template <class LanguageConfig>
class GCLang : public GC {
public:
    explicit GCLang(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    NO_COPY_SEMANTIC(GCLang);
    NO_MOVE_SEMANTIC(GCLang);
    void SetPandaVM(PandaVM *vm) override
    {
        root_manager_.SetPandaVM(vm);
        GC::SetPandaVM(vm);
    }

protected:
    ~GCLang() override;
    void CommonUpdateRefsToMovedObjects(const UpdateRefInAllocator &update_allocator) override;

    void VisitRoots(const GCRootVisitor &gc_root_visitor, VisitGCRootFlags flags) override
    {
        trace::ScopedTrace scoped_trace(__FUNCTION__);
        root_manager_.VisitNonHeapRoots(gc_root_visitor, flags);
    }

    void VisitClassRoots(const GCRootVisitor &gc_root_visitor) override
    {
        trace::ScopedTrace scoped_trace(__FUNCTION__);
        root_manager_.VisitClassRoots(gc_root_visitor);
    }

    void VisitCardTableRoots(CardTable *card_table, const GCRootVisitor &gc_root_visitor,
                             const MemRangeChecker &range_checker, const ObjectChecker &range_object_checker,
                             const ObjectChecker &from_object_checker, uint32_t processed_flag) override
    {
        root_manager_.VisitCardTableRoots(card_table, GetObjectAllocator(), gc_root_visitor, range_checker,
                                          range_object_checker, from_object_checker, processed_flag);
    }

    void PreRunPhasesImpl() override;

private:
    void UpdateVmRefs() override
    {
        root_manager_.UpdateVmRefs();
    }

    void UpdateGlobalObjectStorage() override
    {
        root_manager_.UpdateGlobalObjectStorage();
    }

    void UpdateClassLinkerContextRoots() override
    {
        root_manager_.UpdateClassLinkerContextRoots();
    }

    void UpdateThreadLocals() override
    {
        root_manager_.UpdateThreadLocals();
    }

    size_t VerifyHeap() override;

    RootManager<LanguageConfig> root_manager_ {};
    friend class RootManager<LanguageConfig>;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_LANG_GC_LANG_H_
