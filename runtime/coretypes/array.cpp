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

#include "runtime/include/coretypes/array.h"

#include "runtime/arch/memory_helpers.h"
#include "runtime/dyn_class_linker_extension.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"

namespace panda::coretypes {

static Array *AllocateArray(panda::BaseClass *array_class, size_t elem_size, array_size_t length,
                            panda::SpaceType space_type, const PandaVM *vm = Runtime::GetCurrent()->GetPandaVM())
{
    size_t size = Array::ComputeSize(elem_size, length);
    if (UNLIKELY(size == 0)) {
        LOG(ERROR, RUNTIME) << "Illegal array size: element size: " << elem_size << " array length: " << length;
        ThrowOutOfMemoryError("OOM when allocating array");
        return nullptr;
    }
    if (LIKELY(space_type == panda::SpaceType::SPACE_TYPE_OBJECT)) {
        return static_cast<coretypes::Array *>(
            vm->GetHeapManager()->AllocateObject(array_class, size, DEFAULT_ALIGNMENT, MTManagedThread::GetCurrent()));
    }
    if (space_type == panda::SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT) {
        return static_cast<coretypes::Array *>(vm->GetHeapManager()->AllocateNonMovableObject(
            array_class, size, DEFAULT_ALIGNMENT, ManagedThread::GetCurrent()));
    }
    UNREACHABLE();
}

/* static */
Array *Array::Create(panda::Class *array_class, const uint8_t *data, array_size_t length, panda::SpaceType space_type)
{
    size_t elem_size = array_class->GetComponentSize();
    auto *array = AllocateArray(array_class, elem_size, length, space_type);
    if (UNLIKELY(array == nullptr)) {
        LOG(ERROR, RUNTIME) << "Failed to allocate array.";
        return nullptr;
    }
    // Order is what matters here: GC can read data before it is copied if we set length first.
    // length == 0 is guaranteed by AllocateArray
    TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
    array->SetLength(length);
    (void)memcpy_s(array->GetData(), array->GetLength() * elem_size, data, length * elem_size);
    TSAN_ANNOTATE_IGNORE_WRITES_END();
    // Witout full memory barrier it is possible that architectures with weak memory order can try fetching array
    // legth before it's set
    arch::FullMemoryBarrier();
    return array;
}

/* static */
Array *Array::Create(panda::Class *array_class, array_size_t length, panda::SpaceType space_type)
{
    size_t elem_size = array_class->GetComponentSize();
    auto *array = AllocateArray(array_class, elem_size, length, space_type);
    if (array == nullptr) {
        return nullptr;
    }
    // No need to memset - it is done in allocator
    TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
    array->SetLength(length);
    TSAN_ANNOTATE_IGNORE_WRITES_END();
    // Witout full memory barrier it is possible that architectures with weak memory order can try fetching array
    // legth before it's set
    arch::FullMemoryBarrier();
    return array;
}

/* static */
Array *Array::Create(DynClass *dynarrayclass, array_size_t length, panda::SpaceType space_type)
{
    size_t elem_size = coretypes::TaggedValue::TaggedTypeSize();
    HClass *array_class = dynarrayclass->GetHClass();
    auto *array = AllocateArray(array_class, elem_size, length, space_type);
    if (array == nullptr) {
        return nullptr;
    }
    // No need to memset - it is done in allocator
    TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
    array->SetLength(length);
    TSAN_ANNOTATE_IGNORE_WRITES_END();
    // Witout full memory barrier it is possible that architectures with weak memory order can try fetching array
    // legth before it's set
    arch::FullMemoryBarrier();
    return array;
}

/* static */
Array *Array::CreateTagged(const PandaVM *vm, panda::BaseClass *array_class, array_size_t length,
                           panda::SpaceType space_type, TaggedValue init_value)
{
    size_t elem_size = coretypes::TaggedValue::TaggedTypeSize();
    auto *array = AllocateArray(array_class, elem_size, length, space_type, vm);
    // Order is matters here: GC can read data before it copied if we set length first.
    // length == 0 is guaranteed by AllocateArray
    for (array_size_t i = 0; i < length; i++) {
        array->Set<TaggedType, false, true>(i, init_value.GetRawData());
    }
    TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
    array->SetLength(length);
    TSAN_ANNOTATE_IGNORE_WRITES_END();
    // Witout full memory barrier it is possible that architectures with weak memory order can try fetching array
    // legth before it's set
    arch::FullMemoryBarrier();
    return array;
}

}  // namespace panda::coretypes
