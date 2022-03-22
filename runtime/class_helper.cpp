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

#include "runtime/include/class_helper.h"

#include <algorithm>

#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/bit_utils.h"
#include "runtime/include/class.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/mem/panda_string.h"

namespace panda {

static void Pad(size_t size, size_t *padding, size_t *n)
{
    while (*padding >= size && *n > 0) {
        *padding -= size;
        *n -= 1;
    }
}

/* static */
size_t ClassHelper::ComputeClassSize(size_t vtable_size, size_t imt_size, size_t num_8bit_sfields,
                                     size_t num_16bit_sfields, size_t num_32bit_sfields, size_t num_64bit_sfields,
                                     size_t num_ref_sfields, size_t num_tagged_sfields)
{
    size_t size = sizeof(Class);
    size = AlignUp(size, OBJECT_POINTER_SIZE);
    size += vtable_size * POINTER_SIZE;
    size += imt_size * POINTER_SIZE;
    size += num_ref_sfields * OBJECT_POINTER_SIZE;

    constexpr size_t SIZE_64 = sizeof(uint64_t);
    constexpr size_t SIZE_32 = sizeof(uint32_t);
    constexpr size_t SIZE_16 = sizeof(uint16_t);
    constexpr size_t SIZE_8 = sizeof(uint8_t);

    // Try to fill alignment gaps with fields that have smaller size from largest to smallests
    static_assert(coretypes::TaggedValue::TaggedTypeSize() == SIZE_64,
                  "Please fix alignment of the fields of type \"TaggedValue\"");
    if (!IsAligned<SIZE_64>(size) && (num_64bit_sfields > 0 || num_tagged_sfields > 0)) {
        size_t padding = AlignUp(size, SIZE_64) - size;
        size += padding;

        Pad(SIZE_32, &padding, &num_32bit_sfields);
        Pad(SIZE_16, &padding, &num_16bit_sfields);
        Pad(SIZE_8, &padding, &num_8bit_sfields);
    }

    if (!IsAligned<SIZE_32>(size) && num_32bit_sfields > 0) {
        size_t padding = AlignUp(size, SIZE_32) - size;
        size += padding;

        Pad(SIZE_16, &padding, &num_16bit_sfields);
        Pad(SIZE_8, &padding, &num_8bit_sfields);
    }

    if (!IsAligned<SIZE_16>(size) && num_16bit_sfields > 0) {
        size_t padding = AlignUp(size, SIZE_16) - size;
        size += padding;

        Pad(SIZE_8, &padding, &num_8bit_sfields);
    }

    size += num_64bit_sfields * SIZE_64 + num_32bit_sfields * SIZE_32 + num_16bit_sfields * SIZE_16 +
            num_8bit_sfields * SIZE_8 + num_tagged_sfields * coretypes::TaggedValue::TaggedTypeSize();

    return size;
}

/* static */
const uint8_t *ClassHelper::GetDescriptor(const uint8_t *name, PandaString *storage)
{
    return GetArrayDescriptor(name, 0, storage);
}

/* static */
const uint8_t *ClassHelper::GetArrayDescriptor(const uint8_t *component_name, size_t rank, PandaString *storage)
{
    storage->clear();
    storage->append(rank, '[');
    storage->push_back('L');
    storage->append(utf::Mutf8AsCString(component_name));
    storage->push_back(';');
    std::replace(storage->begin(), storage->end(), '.', '/');
    return utf::CStringAsMutf8(storage->c_str());
}

/* static */
char ClassHelper::GetPrimitiveTypeDescriptorChar(panda_file::Type::TypeId type_id)
{
    // static_cast isn't necessary in most implementations but may be by standard
    return static_cast<char>(*GetPrimitiveTypeDescriptorStr(type_id));
}

/* static */
const uint8_t *ClassHelper::GetPrimitiveTypeDescriptorStr(panda_file::Type::TypeId type_id)
{
    if (type_id == panda_file::Type::TypeId::REFERENCE) {
        UNREACHABLE();
        return nullptr;
    }

    return utf::CStringAsMutf8(panda_file::Type::GetSignatureByTypeId(panda_file::Type(type_id)));
}

/* static */
const char *ClassHelper::GetPrimitiveTypeStr(panda_file::Type::TypeId type_id)
{
    switch (type_id) {
        case panda_file::Type::TypeId::VOID:
            return "void";
        case panda_file::Type::TypeId::U1:
            return "bool";
        case panda_file::Type::TypeId::I8:
            return "i8";
        case panda_file::Type::TypeId::U8:
            return "u8";
        case panda_file::Type::TypeId::I16:
            return "i16";
        case panda_file::Type::TypeId::U16:
            return "u16";
        case panda_file::Type::TypeId::I32:
            return "i32";
        case panda_file::Type::TypeId::U32:
            return "u32";
        case panda_file::Type::TypeId::I64:
            return "i64";
        case panda_file::Type::TypeId::U64:
            return "u64";
        case panda_file::Type::TypeId::F32:
            return "f32";
        case panda_file::Type::TypeId::F64:
            return "f64";
        default:
            UNREACHABLE();
            break;
    }
    return nullptr;
}

/* static */
const uint8_t *ClassHelper::GetPrimitiveDescriptor(panda_file::Type type, PandaString *storage)
{
    return GetPrimitiveArrayDescriptor(type, 0, storage);
}

/* static */
const uint8_t *ClassHelper::GetPrimitiveArrayDescriptor(panda_file::Type type, size_t rank, PandaString *storage)
{
    storage->clear();
    storage->append(rank, '[');
    storage->push_back(GetPrimitiveTypeDescriptorChar(type.GetId()));
    return utf::CStringAsMutf8(storage->c_str());
}

/* static */
const uint8_t *ClassHelper::GetTypeDescriptor(const PandaString &name, PandaString *storage)
{
    *storage = "L" + name + ";";
    std::replace(storage->begin(), storage->end(), '.', '/');
    return utf::CStringAsMutf8(storage->c_str());
}

}  // namespace panda
