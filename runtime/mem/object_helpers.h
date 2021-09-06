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

#ifndef PANDA_RUNTIME_MEM_OBJECT_HELPERS_H_
#define PANDA_RUNTIME_MEM_OBJECT_HELPERS_H_

#include <functional>

#include "runtime/include/coretypes/tagged_value.h"
#include "libpandafile/file_items.h"
#include "runtime/include/language_config.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/object_header_config.h"
#include "libpandabase/utils/logger.h"

namespace panda {
class Class;
class Field;
class ManagedThread;
class ObjectHeader;
class PandaVM;
}  // namespace panda

namespace panda::coretypes {
class DynClass;
}  // namespace panda::coretypes

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_DEBUG_OBJ_HELPERS LOG(DEBUG, GC) << vm->GetGC()->GetLogPrefix()

class GC;

size_t GetObjectSize(const void *mem);

PandaString GetDebugInfoAboutObject(const ObjectHeader *header);

void DumpObject(ObjectHeader *object_header, std::basic_ostream<char, std::char_traits<char>> *o_stream = &std::cerr);

void DumpClass(Class *cls, std::basic_ostream<char, std::char_traits<char>> *o_stream = &std::cerr);

[[nodiscard]] ObjectHeader *GetForwardAddress(ObjectHeader *object_header);

const char *GetFieldName(const Field &field);

size_t GetDynClassInstanceSize(coretypes::DynClass *object);

class GCStaticObjectHelpers {
public:
    static void TraverseAllObjects(ObjectHeader *object_header,
                                   const std::function<void(ObjectHeader *, ObjectHeader *)> &obj_visitor);

    template <typename FieldVisitor>
    static void TraverseObject(ObjectHeader *object_header, BaseClass *base_cls, const FieldVisitor &field_visitor);

    template <typename ElementVisitor>
    static void TraverseArray(ObjectHeader *object_header, BaseClass *base_cls,
                              const ElementVisitor &array_element_visitor);

    static void UpdateRefsToMovedObjects(PandaVM *vm, ObjectHeader *object, BaseClass *base_cls);
};

class GCDynamicObjectHelpers {
public:
    static void TraverseAllObjects(ObjectHeader *object_header,
                                   const std::function<void(ObjectHeader *, ObjectHeader *)> &obj_visitor);

    template <typename FieldVisitor>
    static void TraverseObject(ObjectHeader *object_header, BaseClass *base_cls, const FieldVisitor &field_visitor);

    template <typename ElementVisitor>
    static void TraverseArray(ObjectHeader *object_header, BaseClass *base_cls,
                              const ElementVisitor &array_element_visitor);

    static void UpdateRefsToMovedObjects(PandaVM *vm, ObjectHeader *object, BaseClass *base_cls);

    static void RecordDynWeakReference(GC *gc, coretypes::TaggedType *value);
    static void HandleDynWeakReferences(GC *gc);

private:
    static void UpdateDynArray(PandaVM *vm, ObjectHeader *object_header, array_size_t index, ObjectHeader *obj_ref);

    static void UpdateDynObjectRef(PandaVM *vm, ObjectHeader *object_header, size_t offset, ObjectHeader *field_obj_ref,
                                   bool is_update_classword);
};

template <LangTypeT LangType>
class GCObjectHelpers {
};

template <>
class GCObjectHelpers<LANG_TYPE_STATIC> {
public:
    using Value = GCStaticObjectHelpers;
};

template <>
class GCObjectHelpers<LANG_TYPE_DYNAMIC> {
public:
    using Value = GCDynamicObjectHelpers;
};

template <LangTypeT LangType>
using ObjectHelpers = typename GCObjectHelpers<LangType>::Value;

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_OBJECT_HELPERS_H_
