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

#ifndef PANDA_RUNTIME_MEM_GC_GC_ROOT_H_
#define PANDA_RUNTIME_MEM_GC_GC_ROOT_H_

#include <algorithm>
#include <ostream>
#include <vector>

#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/mem_range.h"
#include "runtime/include/class.h"
#include "runtime/include/language_config.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/interpreter/frame.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/gc/card_table.h"

namespace panda {
class PandaVM;
class ManagedThread;
}  // namespace panda

namespace panda::mem {

enum class RootType {
    ROOT_UNKNOWN = 0,
    ROOT_CLASS,
    ROOT_FRAME,
    ROOT_THREAD,
    ROOT_CLASS_LINKER,
    ROOT_TENURED,
    ROOT_VM,
    ROOT_JNI_GLOBAL,
    ROOT_JNI_LOCAL,
    ROOT_RS_GLOBAL,
    ROOT_PT_LOCAL,
    ROOT_AOT_STRING_SLOT,
};

enum class VisitGCRootFlags : uint32_t {
    ACCESS_ROOT_ALL = 1U,
    ACCESS_ROOT_ONLY_NEW = 1U << 1U,
    ACCESS_ROOT_NONE = 1U << 2U,

    ACCESS_ROOT_AOT_STRINGS_ONLY_YOUNG = 1U << 3U,

    START_RECORDING_NEW_ROOT = 1U << 10U,
    END_RECORDING_NEW_ROOT = 1U << 11U,
};

uint32_t operator&(VisitGCRootFlags left, VisitGCRootFlags right);

VisitGCRootFlags operator|(VisitGCRootFlags left, VisitGCRootFlags right);

/**
 * \brief I am groot
 */
class GCRoot {
public:
    GCRoot(RootType type, ObjectHeader *obj);
    GCRoot(RootType type, ObjectHeader *from_object, ObjectHeader *obj);

    RootType GetType() const;
    ObjectHeader *GetObjectHeader() const;
    ObjectHeader *GetFromObjectHeader() const;
    friend std::ostream &operator<<(std::ostream &os, const GCRoot &root);

    virtual ~GCRoot() = default;

    NO_COPY_SEMANTIC(GCRoot);
    NO_MOVE_SEMANTIC(GCRoot);

private:
    RootType type_;
    /**
     * From which object current root was found by reference. Usually from_object is nullptr, except when object was
     * found from card_table
     */
    ObjectHeader *from_object_;
    ObjectHeader *object_;
};

template <class LanguageConfig>
class RootManager final {
public:
    /**
     * Visit all non-heap roots(registers, thread locals, etc)
     */
    void VisitNonHeapRoots(const GCRootVisitor &gc_root_visitor,
                           VisitGCRootFlags flags = VisitGCRootFlags::ACCESS_ROOT_ALL) const;

    /**
     * Visit local roots for frame
     */
    void VisitLocalRoots(const GCRootVisitor &gc_root_visitor) const;

    /**
     * Visit card table roots
     * @param card_table - card table for scan
     * @param allocator - object allocator
     * @param root_visitor
     * @param range_checker
     * @param range_object_checker
     */
    void VisitCardTableRoots(CardTable *card_table, ObjectAllocatorBase *allocator, GCRootVisitor root_visitor,
                             MemRangeChecker range_checker, ObjectChecker range_object_checker,
                             ObjectChecker from_object_checker, uint32_t processed_flag) const;

    /**
     * Visit roots in class linker
     */
    void VisitClassRoots(const GCRootVisitor &gc_root_visitor,
                         VisitGCRootFlags flags = VisitGCRootFlags::ACCESS_ROOT_ALL) const;

    /**
     * Updates references to moved objects in TLS
     */
    void UpdateThreadLocals();

    void UpdateVmRefs();

    void UpdateGlobalObjectStorage();

    void UpdateClassLinkerContextRoots();

    void SetPandaVM(PandaVM *vm)
    {
        vm_ = vm;
    }

private:
    /**
     * Visit VM-specific roots
     */
    void VisitVmRoots(const GCRootVisitor &gc_root_visitor) const;

    void VisitRegisterRoot(const Frame::VRegister &v_register, const GCRootVisitor &gc_root_visitor) const;

    void VisitClassLinkerContextRoots(const GCRootVisitor &gc_root_visitor) const;

    /**
     * Visit roots for \param thread
     * @param thread
     */
    void VisitRootsForThread(ManagedThread *thread, const GCRootVisitor &gc_root_visitor) const;

    PandaVM *vm_ {nullptr};
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_ROOT_H_
