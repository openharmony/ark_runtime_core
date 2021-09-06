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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_HELPER_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_HELPER_H_

#include <cstdint>

#include "libpandabase/utils/span.h"
#include "libpandafile/file_items.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/object_header_config.h"

namespace panda {

// Small helper
template <class Config>
class ClassConfig {
public:
    using classWordSize = typename Config::Size;
};

class ClassHelper : private ClassConfig<MemoryModelConfig> {
public:
    using classWordSize = typename ClassConfig::classWordSize;  // To be visible outside

    static constexpr size_t OBJECT_POINTER_SIZE = sizeof(classWordSize);
    // In general for any T: sizeof(T*) != OBJECT_POINTER_SIZE
    static constexpr size_t POINTER_SIZE = sizeof(uintptr_t);

    static size_t ComputeClassSize(size_t vtable_size, size_t imt_size, size_t num_8bit_sfields,
                                   size_t num_16bit_sfields, size_t num_32bit_sfields, size_t num_64bit_sfields,
                                   size_t num_ref_sfields, size_t num_tagged_sfields);

    static const uint8_t *GetDescriptor(const uint8_t *name, PandaString *storage);

    static const uint8_t *GetTypeDescriptor(const PandaString &name, PandaString *storage);

    static const uint8_t *GetArrayDescriptor(const uint8_t *component_name, size_t rank, PandaString *storage);

    static char GetPrimitiveTypeDescriptorChar(panda_file::Type::TypeId type_id);

    static const uint8_t *GetPrimitiveTypeDescriptorStr(panda_file::Type::TypeId type_id);

    static const char *GetPrimitiveTypeStr(panda_file::Type::TypeId type_id);

    static const uint8_t *GetPrimitiveDescriptor(panda_file::Type type, PandaString *storage);

    static const uint8_t *GetPrimitiveArrayDescriptor(panda_file::Type type, size_t rank, PandaString *storage);

    template <typename Str = std::string>
    static Str GetName(const uint8_t *descriptor);

    static bool IsArrayDescriptor(const uint8_t *descriptor)
    {
        Span<const uint8_t> sp(descriptor, 1);
        return sp[0] == '[';
    }

    static const uint8_t *GetComponentDescriptor(const uint8_t *descriptor)
    {
        ASSERT(IsArrayDescriptor(descriptor));
        Span<const uint8_t> sp(descriptor, 1);
        return sp.cend();
    }

    static size_t GetDimensionality(const uint8_t *descriptor)
    {
        ASSERT(IsArrayDescriptor(descriptor));
        size_t dim = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        while (*descriptor++ == '[') {
            ++dim;
        }
        return dim;
    }
};

// Str is std::string or PandaString
/* static */
template <typename Str>
Str ClassHelper::GetName(const uint8_t *descriptor)
{
    switch (*descriptor) {
        case 'V':
            return "void";
        case 'Z':
            return "u1";
        case 'B':
            return "i8";
        case 'H':
            return "u8";
        case 'S':
            return "i16";
        case 'C':
            return "u16";
        case 'I':
            return "i32";
        case 'U':
            return "u32";
        case 'J':
            return "i64";
        case 'Q':
            return "u64";
        case 'F':
            return "f32";
        case 'D':
            return "f64";
        case 'A':
            return "any";
        default: {
            break;
        }
    }

    Str name = utf::Mutf8AsCString(descriptor);
    if (name[0] == '[') {
        return name;
    }

    std::replace(name.begin(), name.end(), '/', '.');

    ASSERT(name.size() > 2);  // 2 - L and ;

    name.erase(0, 1);
    name.pop_back();

    return name;
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_HELPER_H_
