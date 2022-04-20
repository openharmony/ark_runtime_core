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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_H_

#include <securec.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "libpandabase/utils/utf.h"
#include "runtime/include/language_context.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/vm_handle.h"

namespace panda::coretypes {

class Array;
class String : public ObjectHeader {
public:
    static String *Cast(ObjectHeader *object)
    {
        return static_cast<String *>(object);
    }

    static String *CreateFromMUtf8(const uint8_t *mutf8_data, size_t mutf8_length, uint32_t utf16_length,
                                   bool can_be_compressed, LanguageContext ctx, PandaVM *vm, bool movable = true);

    static String *CreateFromMUtf8(const uint8_t *mutf8_data, uint32_t utf16_length, bool can_be_compressed,
                                   LanguageContext ctx, PandaVM *vm, bool movable = true);

    static String *CreateFromMUtf8(const uint8_t *mutf8_data, uint32_t utf16_length, LanguageContext ctx, PandaVM *vm,
                                   bool movable = true);

    static String *CreateFromMUtf8(const uint8_t *mutf8_data, LanguageContext ctx, PandaVM *vm, bool movable = true);

    static String *CreateFromUtf16(const uint16_t *utf16_data, uint32_t utf16_length, LanguageContext ctx, PandaVM *vm,
                                   bool movable = true);

    static String *CreateEmptyString(LanguageContext ctx, PandaVM *vm);

    static String *CreateFromString(String *str, LanguageContext ctx, PandaVM *vm);

    static String *Concat(String *jstring1, String *jstring2, LanguageContext ctx, PandaVM *vm);

    static String *CreateNewStringFromChars(uint32_t offset, uint32_t length, Array *chararray, LanguageContext ctx,
                                            PandaVM *vm);

    static String *CreateNewStringFromBytes(uint32_t offset, uint32_t length, uint32_t high_byte, Array *bytearray,
                                            LanguageContext ctx, PandaVM *vm);

    template <bool verify = true>
    uint16_t At(int32_t index);

    int32_t Compare(String *rstr);

    Array *ToCharArray(LanguageContext ctx);

    bool IsUtf16() const
    {
        return compressed_strings_enabled ? ((length_ & STRING_COMPRESSED_BIT) == STRING_UNCOMPRESSED) : true;
    }

    bool IsMUtf8() const
    {
        return compressed_strings_enabled ? ((length_ & STRING_COMPRESSED_BIT) == STRING_COMPRESSED) : true;
    }

    static size_t ComputeDataSizeUtf16(uint32_t length)
    {
        return length * sizeof(data_utf16_[0]);
    }

    /**
     * Methods for uncompressed strings (UTF16)
     */
    static size_t ComputeSizeUtf16(uint32_t utf16_length)
    {
        return sizeof(String) + ComputeDataSizeUtf16(utf16_length);
    }

    uint16_t *GetDataUtf16()
    {
        LOG_IF(!IsUtf16(), FATAL, RUNTIME) << "String: Read data as utf16 for mutf8 string";
        return data_utf16_;
    }

    /**
     * Methods for compresses strings (MUTF8 or LATIN1)
     */
    static size_t ComputeSizeMUtf8(uint32_t mutf8_length)
    {
        return sizeof(String) + mutf8_length;
    }

    /**
     * It's MUtf8 format, but without 0 in the end.
     */
    uint8_t *GetDataMUtf8()
    {
        LOG_IF(IsUtf16(), FATAL, RUNTIME) << "String: Read data as mutf8 for utf16 string";
        return reinterpret_cast<uint8_t *>(data_utf16_);
    }

    size_t GetMUtf8Length()
    {
        if (!IsUtf16()) {
            return GetLength() + 1;  // add place for zero at the end
        }
        return panda::utf::Utf16ToMUtf8Size(data_utf16_, GetLength());
    }

    size_t GetUtf16Length()
    {
        return GetLength();
    }

    inline size_t CopyDataMUtf8(uint8_t *buf, size_t max_length)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        buf[max_length - 1] = '\0';
        return CopyDataRegionMUtf8(buf, 0, GetLength(), max_length) + 1;  // add place for zero at the end
    }

    size_t CopyDataRegionMUtf8(uint8_t *buf, size_t start, size_t length, size_t max_length)
    {
        if (length > max_length) {
            return 0;
        }
        uint32_t len = GetLength();
        if (start + length > len) {
            return 0;
        }
        if (!IsUtf16()) {
            constexpr size_t MAX_LEN = std::numeric_limits<size_t>::max() / 2 - 1;
            if (length > MAX_LEN) {
                LOG(FATAL, RUNTIME) << __func__ << " length is higher than half of size_t::max";
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (memcpy_s(buf, sizeof(uint8_t) * (max_length + 1), GetDataMUtf8() + start, length) !=
                EOK) {
                LOG(FATAL, RUNTIME) << __func__ << " length is higher than buf size";
            }
            return length;
        }
        return panda::utf::ConvertRegionUtf16ToMUtf8(GetDataUtf16(), buf, length, max_length - 1, start);
    }

    inline uint32_t CopyDataUtf16(uint16_t *buf, uint32_t max_length)
    {
        return CopyDataRegionUtf16(buf, 0, GetLength(), max_length);
    }

    uint32_t CopyDataRegionUtf16(uint16_t *buf, uint32_t start, uint32_t length, uint32_t max_length)
    {
        if (length > max_length) {
            return 0;
        }
        uint32_t len = GetLength();
        if (start + length > len) {
            return 0;
        }
        if (IsUtf16()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (memcpy_s(buf, sizeof(uint16_t) * max_length, GetDataUtf16() + start, ComputeDataSizeUtf16(length)) !=
                EOK) {
                LOG(FATAL, RUNTIME) << __func__ << " length is higher than buf size";
            }
            return length;
        }
        return panda::utf::ConvertRegionMUtf8ToUtf16(GetDataMUtf8(), buf, len, length, start);
    }

    uint32_t GetLength() const
    {
        uint32_t length;
        if (compressed_strings_enabled) {
            length = length_ >> 1U;
        } else {
            length = length_;
        }
        return length;
    }

    bool IsEmpty() const
    {
        // do not shift right length because it is always zero for empty string
        return length_ == 0;
    }

    size_t ObjectSize() const
    {
        uint32_t length = GetLength();
        return IsUtf16() ? ComputeSizeUtf16(length) : ComputeSizeMUtf8(length);
    }

    uint32_t GetHashcode()
    {
        if (hashcode_ == 0) {
            hashcode_ = ComputeHashcode();
        }
        return hashcode_;
    }

    int32_t IndexOf(String *rhs, int pos = 0);

    static constexpr uint32_t GetLengthOffset()
    {
        return MEMBER_OFFSET(String, length_);
    }

    static constexpr uint32_t GetDataOffset()
    {
        return MEMBER_OFFSET(String, data_utf16_);
    }

    static constexpr uint32_t GetStringCompressionMask()
    {
        return STRING_COMPRESSED_BIT;
    }

    /**
     * Compares strings by bytes. It doesn't check canonical unicode equivalence.
     */
    static bool StringsAreEqual(String *str1, String *str2);
    /**
     * Compares strings by bytes. It doesn't check canonical unicode equivalence.
     */
    static bool StringsAreEqualMUtf8(String *str1, const uint8_t *mutf8_data, uint32_t utf16_length);
    static bool StringsAreEqualMUtf8(String *str1, const uint8_t *mutf8_data, uint32_t utf16_length,
                                     bool can_be_compressed);
    /**
     * Compares strings by bytes. It doesn't check canonical unicode equivalence.
     */
    static bool StringsAreEqualUtf16(String *str1, const uint16_t *utf16_data, uint32_t utf16_data_length);
    static String *DoReplace(String *src, uint16_t old_c, uint16_t new_c, LanguageContext ctx, PandaVM *vm);
    static uint32_t ComputeHashcodeMutf8(const uint8_t *mutf8_data, uint32_t length, bool can_be_compressed);
    static uint32_t ComputeHashcodeMutf8(const uint8_t *mutf8_data, uint32_t length);
    static uint32_t ComputeHashcodeUtf16(uint16_t *utf16_data, uint32_t length);

    static void SetCompressedStringsEnabled(bool val)
    {
        compressed_strings_enabled = val;
    }

    static bool GetCompressedStringsEnabled()
    {
        return compressed_strings_enabled;
    }

    static String *FastSubString(String *src, uint32_t start, uint32_t utf16_length, LanguageContext ctx,
                                 PandaVM *vm = nullptr);

    static bool CanBeCompressedMUtf8(const uint8_t *mutf8_data);

protected:
    void SetLength(uint32_t length, bool compressed = false)
    {
        if (compressed_strings_enabled) {
            ASSERT(length < 0x80000000U);
            // Use 0u for compressed/utf8 expression
            length_ = (length << 1U) | (compressed ? STRING_COMPRESSED : STRING_UNCOMPRESSED);
        } else {
            length_ = length;
        }
    }

    void SetHashcode(uint32_t hashcode)
    {
        hashcode_ = hashcode;
    }

    uint32_t ComputeHashcode();
    static bool CanBeCompressed(const uint16_t *utf16_data, uint32_t utf16_length);
    static void CopyUtf16AsMUtf8(const uint16_t *utf16_from, uint8_t *mutf8_to, uint32_t utf16_length);

private:
    static bool compressed_strings_enabled;
    static constexpr uint32_t STRING_COMPRESSED_BIT = 0x1;
    enum CompressedStatus {
        STRING_COMPRESSED,
        STRING_UNCOMPRESSED,
    };

    static bool IsASCIICharacter(uint16_t data)
    {
        // \0 is not considered ASCII in Modified-UTF8
        return data - 1U < utf::MUTF8_1B_MAX;
    }

    static bool CanBeCompressedMUtf8(const uint8_t *mutf8_data, uint32_t mutf8_length);
    static bool CanBeCompressedUtf16(const uint16_t *utf16_data, uint32_t utf16_length, uint16_t non);
    static bool CanBeCompressedMUtf8(const uint8_t *mutf8_data, uint32_t mutf8_length, uint16_t non);

    /**
     * str1 should have the same length as mutf16_data.
     * Converts mutf8_data to mutf16 and compare it with given mutf16_data.
     */
    static bool IsMutf8EqualsUtf16(const uint8_t *utf8_data, uint32_t utf8_data_length, const uint16_t *utf16_data,
                                   uint32_t utf16_data_length);

    static bool IsMutf8EqualsUtf16(const uint8_t *utf8_data, const uint16_t *utf16_data, uint32_t utf16_data_length);

    template <typename T>
    /**
     * Check that two spans are equal. Should have the same length.
     */
    static bool StringsAreEquals(Span<const T> &str1, Span<const T> &str2);

    template <typename T1, typename T2>
    static int32_t IndexOf(Span<const T1> &lhs_sp, Span<const T2> &rhs_sp, int32_t pos, int32_t max);

    static String *AllocStringObject(size_t length, bool compressed, LanguageContext ctx, PandaVM *vm = nullptr,
                                     bool movable = true);

    // In last bit of length_ we store if this string is compressed or not.
    uint32_t length_;
    uint32_t hashcode_;
    // A pointer to the string data stored after the string header.
    // Data can be stored in mutf8 or utf16 form according to compressed bit.
    __extension__ uint16_t data_utf16_[0];  // NOLINT(modernize-avoid-c-arrays)
};

constexpr uint32_t STRING_LENGTH_OFFSET = 8U;
static_assert(STRING_LENGTH_OFFSET == panda::coretypes::String::GetLengthOffset());
constexpr uint32_t STRING_DATA_OFFSET = 16U;
static_assert(STRING_DATA_OFFSET == panda::coretypes::String::GetDataOffset());

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_H_
