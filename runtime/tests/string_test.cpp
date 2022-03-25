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

#include <ctime>

#include "gtest/gtest.h"
#include "libpandabase/utils/span.h"
#include "libpandabase/utils/utf.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/string-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"

namespace panda::coretypes::test {

class StringTest : public testing::Test {
public:
    StringTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0xDEADBEEF;
#endif
        srand(seed_);
        // We need to create a runtime instance to be able to create strings.
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options_);
    }

    ~StringTest()
    {
        Runtime::Destroy();
    }

    LanguageContext GetLanguageContext()
    {
        return Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    }

    void SetUp() override
    {
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    void TearDown() override
    {
        thread_->ManagedCodeEnd();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
    static constexpr uint32_t SIMPLE_UTF8_STRING_LENGTH = 13;
    static constexpr char SIMPLE_UTF8_STRING[SIMPLE_UTF8_STRING_LENGTH + 1] = "Hello, world!";
    unsigned seed_ {0};
    RuntimeOptions options_;
};

TEST_F(StringTest, EqualStringWithCompressedRawUtf8Data)
{
    std::vector<uint8_t> data {0x01, 0x05, 0x07, 0x00};
    uint32_t utf16_length = data.size() - 1;
    auto *first_string =
        String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_TRUE(String::StringsAreEqualMUtf8(first_string, data.data(), utf16_length));
}

TEST_F(StringTest, EqualStringWithNotCompressedRawUtf8Data)
{
    std::vector<uint8_t> data {0xc2, 0xa7};

    for (size_t i = 0; i < 20; i++) {
        data.push_back(0x30 + i);
    }
    data.push_back(0);

    uint32_t utf16_length = data.size() - 2U;
    auto *first_string =
        String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_TRUE(String::StringsAreEqualMUtf8(first_string, data.data(), utf16_length));
}

TEST_F(StringTest, NotEqualStringWithNotCompressedRawUtf8Data)
{
    std::vector<uint8_t> data1 {0xc2, 0xa7, 0x33, 0x00};
    std::vector<uint8_t> data2 {0xc2, 0xa7, 0x34, 0x00};
    uint32_t utf16_length = 2;
    auto *first_string =
        String::CreateFromMUtf8(data1.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_FALSE(String::StringsAreEqualMUtf8(first_string, data2.data(), utf16_length));
}

TEST_F(StringTest, NotEqualStringNotCompressedStringWithCompressedRawData)
{
    std::vector<uint8_t> data1 {0xc2, 0xa7, 0x33, 0x00};
    std::vector<uint8_t> data2 {0x02, 0x07, 0x04, 0x00};
    uint32_t utf16_length = 2;
    auto *first_string =
        String::CreateFromMUtf8(data1.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_FALSE(String::StringsAreEqualMUtf8(first_string, data2.data(), utf16_length));
}

TEST_F(StringTest, NotEqualCompressedStringWithUncompressedRawUtf8Data)
{
    std::vector<uint8_t> data1 {0x02, 0x07, 0x04, 0x00};
    std::vector<uint8_t> data2 {0xc2, 0xa7, 0x33, 0x00};
    uint32_t utf16_length = 2;
    auto *first_string =
        String::CreateFromMUtf8(data1.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_FALSE(String::StringsAreEqualMUtf8(first_string, data2.data(), utf16_length));
}

TEST_F(StringTest, EqualStringWithMUtf8DifferentLength)
{
    std::vector<uint8_t> data1 {0xc2, 0xa7, 0x33, 0x00};
    std::vector<uint8_t> data2 {0xc2, 0xa7, 0x00};
    uint32_t utf16_length = 2;
    auto *first_string =
        String::CreateFromMUtf8(data1.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_FALSE(String::StringsAreEqualMUtf8(first_string, data2.data(), utf16_length - 1));
}

TEST_F(StringTest, EqualStringWithRawUtf16Data)
{
    std::vector<uint16_t> data {0xffc3, 0x33, 0x00};
    auto *first_string =
        String::CreateFromUtf16(data.data(), data.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto second_string = reinterpret_cast<const uint16_t *>(data.data());
    ASSERT_TRUE(String::StringsAreEqualUtf16(first_string, second_string, data.size()));
}

TEST_F(StringTest, CompareCompressedStringWithRawUtf16)
{
    std::vector<uint16_t> data;

    for (size_t i = 0; i < 30; i++) {
        data.push_back(i + 1);
    }
    data.push_back(0);

    auto *first_string = String::CreateFromUtf16(data.data(), data.size() - 1, GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
    auto second_string = reinterpret_cast<const uint16_t *>(data.data());
    ASSERT_TRUE(String::StringsAreEqualUtf16(first_string, second_string, data.size() - 1));
}

TEST_F(StringTest, EqualStringWithRawUtf16DifferentLength)
{
    std::vector<uint16_t> data1 {0xffc3, 0x33, 0x00};
    std::vector<uint16_t> data2 {0xffc3, 0x33, 0x55, 0x00};
    auto *first_string =
        String::CreateFromUtf16(data1.data(), data1.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto second_string = reinterpret_cast<const uint16_t *>(data2.data());
    ASSERT_FALSE(String::StringsAreEqualUtf16(first_string, second_string, data2.size()));
}

TEST_F(StringTest, NotEqualStringWithRawUtf16Data)
{
    std::vector<uint16_t> data1 {0xffc3, 0x33, 0x00};
    std::vector<uint16_t> data2 {0xffc3, 0x34, 0x00};
    auto *first_string =
        String::CreateFromUtf16(data1.data(), data1.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());

    auto second_string = reinterpret_cast<const uint16_t *>(data2.data());
    ASSERT_FALSE(String::StringsAreEqualUtf16(first_string, second_string, data2.size()));
}

TEST_F(StringTest, compressedHashCodeUtf8)
{
    String *first_string =
        String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(SIMPLE_UTF8_STRING), SIMPLE_UTF8_STRING_LENGTH,
                                GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto string_hash_code = first_string->GetHashcode();
    auto raw_hash_code =
        String::ComputeHashcodeMutf8(reinterpret_cast<const uint8_t *>(SIMPLE_UTF8_STRING), SIMPLE_UTF8_STRING_LENGTH);

    ASSERT_EQ(string_hash_code, raw_hash_code);
}
TEST_F(StringTest, notCompressedHashCodeUtf8)
{
    std::vector<uint8_t> data {0xc2, 0xa7};

    size_t size = 1;
    for (size_t i = 0; i < 20; i++) {
        data.push_back(0x30 + i);
        size += 1;
    }
    data.push_back(0);

    String *first_string = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(data.data()), size,
                                                   GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto string_hash_code = first_string->GetHashcode();
    auto raw_hash_code = String::ComputeHashcodeMutf8(reinterpret_cast<const uint8_t *>(data.data()), size);

    ASSERT_EQ(string_hash_code, raw_hash_code);
}

TEST_F(StringTest, compressedHashCodeUtf16)
{
    std::vector<uint16_t> data;

    size_t size = 30;
    for (size_t i = 0; i < size; i++) {
        data.push_back(i + 1);
    }
    data.push_back(0);

    auto *first_string =
        String::CreateFromUtf16(data.data(), data.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto string_hash_code = first_string->GetHashcode();
    auto raw_hash_code = String::ComputeHashcodeUtf16(data.data(), data.size());
    ASSERT_EQ(string_hash_code, raw_hash_code);
}

TEST_F(StringTest, notCompressedHashCodeUtf16)
{
    std::vector<uint16_t> data {0xffc3, 0x33, 0x00};
    auto *first_string =
        String::CreateFromUtf16(data.data(), data.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    auto string_hash_code = first_string->GetHashcode();
    auto raw_hash_code = String::ComputeHashcodeUtf16(data.data(), data.size());
    ASSERT_EQ(string_hash_code, raw_hash_code);
}

TEST_F(StringTest, lengthUtf8)
{
    String *string =
        String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(SIMPLE_UTF8_STRING), SIMPLE_UTF8_STRING_LENGTH,
                                GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetLength(), SIMPLE_UTF8_STRING_LENGTH);
}

TEST_F(StringTest, lengthUtf16)
{
    std::vector<uint16_t> data {0xffc3, 0x33, 0x00};
    auto *string =
        String::CreateFromUtf16(data.data(), data.size(), GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetLength(), data.size());
}

TEST_F(StringTest, DifferentLengthStringCompareTest)
{
    static constexpr uint32_t f_string_length = 8;
    static constexpr char f_string[f_string_length + 1] = "Hello, w";
    String *first_string =
        String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(SIMPLE_UTF8_STRING), SIMPLE_UTF8_STRING_LENGTH,
                                GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(first_string->GetLength(), SIMPLE_UTF8_STRING_LENGTH);
    String *second_string = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(f_string), f_string_length,
                                                    GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(second_string->GetLength(), f_string_length);
    ASSERT_EQ(String::StringsAreEqual(first_string, second_string), false);
}

TEST_F(StringTest, ForeignLengthAndCopyTest1b0)
{
    std::vector<uint8_t> data {'a', 'b', 'c', 'd', 'z', 0xc0, 0x80, 0x00};
    uint32_t utf16_length = data.size();
    String *string = String::CreateFromMUtf8(data.data(), utf16_length - 2U, GetLanguageContext(),
                                             Runtime::GetCurrent()->GetPandaVM());  // c080 is U+0000
    ASSERT_EQ(string->GetMUtf8Length(), data.size());
    ASSERT_EQ(string->GetUtf16Length(), data.size() - 2U);  // \0 doesn't counts for UTF16
    std::vector<uint8_t> out8(data.size());
    ASSERT_EQ(string->CopyDataMUtf8(out8.data(), out8.size()), data.size());
    ASSERT_EQ(out8, data);
    std::vector<uint16_t> res16 {'a', 'b', 'c', 'd', 'z', 0x00};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataUtf16(out16.data(), out16.size()), res16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, ForeignLengthAndCopyTest1b)
{
    std::vector<uint8_t> data {'a', 'b', 'c', 'd', 'z', 0x7f, 0x00};
    uint32_t utf16_length = data.size();
    String *string = String::CreateFromMUtf8(data.data(), utf16_length - 1, GetLanguageContext(),
                                             Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetMUtf8Length(), data.size());
    ASSERT_EQ(string->GetUtf16Length(), data.size() - 1);  // \0 doesn't counts for UTF16
    std::vector<uint8_t> out8(data.size());
    ASSERT_EQ(string->CopyDataMUtf8(out8.data(), out8.size()), data.size());
    ASSERT_EQ(out8, data);
    std::vector<uint16_t> res16 {'a', 'b', 'c', 'd', 'z', 0x7f};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataUtf16(out16.data(), out16.size()), res16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, ForeignLengthAndCopyTest2b)
{
    std::vector<uint8_t> data {0xc2, 0xa7, 0x33, 0x00};  // UTF-16 size is 2
    String *string = String::CreateFromMUtf8(data.data(), 2, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetMUtf8Length(), data.size());
    ASSERT_EQ(string->GetUtf16Length(), 2);  // \0 doesn't counts for UTF16
    std::vector<uint8_t> out8(data.size());
    ASSERT_EQ(string->CopyDataMUtf8(out8.data(), out8.size()), data.size());
    ASSERT_EQ(out8, data);
    std::vector<uint16_t> res16 {0xa7, 0x33};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataUtf16(out16.data(), out16.size()), res16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, ForeignLengthAndCopyTest3b)
{
    std::vector<uint8_t> data {0xef, 0xbf, 0x83, 0x33, 0x00};  // UTF-16 size is 2
    String *string = String::CreateFromMUtf8(data.data(), 2, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetMUtf8Length(), data.size());
    ASSERT_EQ(string->GetUtf16Length(), 2);  // \0 doesn't counts for UTF16
    std::vector<uint8_t> out8(data.size());
    ASSERT_EQ(string->CopyDataMUtf8(out8.data(), out8.size()), data.size());
    ASSERT_EQ(out8, data);
    std::vector<uint16_t> res16 {0xffc3, 0x33};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataUtf16(out16.data(), out16.size()), res16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, ForeignLengthAndCopyTest6b)
{
    std::vector<uint8_t> data {0xed, 0xa0, 0x81, 0xed, 0xb0, 0xb7, 0x20, 0x00};  // UTF-16 size is 3
    // We support 4-byte utf-8 sequences, so {0xd801, 0xdc37} is encoded to 4 bytes instead of 6
    std::vector<uint8_t> utf8_data {0xf0, 0x90, 0x90, 0xb7, 0x20, 0x00};
    String *string = String::CreateFromMUtf8(data.data(), 3, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string->GetMUtf8Length(), utf8_data.size());
    ASSERT_EQ(string->GetUtf16Length(), 3);  // \0 doesn't counts for UTF16
    std::vector<uint8_t> out8(utf8_data.size());
    string->CopyDataMUtf8(out8.data(), out8.size());
    ASSERT_EQ(out8, utf8_data);
    std::vector<uint16_t> res16 {0xd801, 0xdc37, 0x20};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataUtf16(out16.data(), out16.size()), res16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, RegionCopyTestMutf8)
{
    std::vector<uint8_t> data {'a', 'b', 'c', 'd', 'z', 0x00};
    uint32_t utf16_length = data.size() - 1;
    String *string =
        String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    size_t start = 2;
    size_t len = string->GetMUtf8Length();
    std::vector<uint8_t> res = {'c', 'd', 0x00};
    std::vector<uint8_t> out8(res.size());
    ASSERT_EQ(string->CopyDataRegionMUtf8(out8.data(), start, len - start - 1 - 1, out8.size()), out8.size() - 1);
    out8[out8.size() - 1] = '\0';
    ASSERT_EQ(out8, res);
    size_t len16 = string->GetUtf16Length();
    std::vector<uint16_t> res16 = {'c', 'd'};
    std::vector<uint16_t> out16(res16.size());
    ASSERT_EQ(string->CopyDataRegionUtf16(out16.data(), start, len16 - start - 1, out16.size()), out16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, RegionCopyTestUtf16)
{
    std::vector<uint8_t> data {'a', 'b', 'c', 'd', 'z', 0xc2, 0xa7, 0x00};
    uint32_t utf16_length = data.size() - 1 - 1;
    String *string =
        String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    size_t start = 2;
    std::vector<uint8_t> res = {'c', 'd', 'z', 0x00};
    std::vector<uint8_t> out8(res.size());
    ASSERT_EQ(string->CopyDataRegionMUtf8(out8.data(), start, 3, out8.size()), out8.size() - 1);
    out8[out8.size() - 1] = '\0';
    ASSERT_EQ(out8, res);
    size_t len16 = string->GetUtf16Length();
    std::vector<uint16_t> out16(len16 - start - 1);
    std::vector<uint16_t> res16 = {'c', 'd', 'z'};
    ASSERT_EQ(string->CopyDataRegionUtf16(out16.data(), start, 3, out16.size()), out16.size());
    ASSERT_EQ(out16, res16);
}

TEST_F(StringTest, SameLengthStringCompareTest)
{
    static constexpr uint32_t string_length = 10;
    char *f_string = new char[string_length + 1];
    char *s_string = new char[string_length + 1];

    for (uint32_t i = 0; i < string_length; i++) {
        // Hack for ConvertMUtf8ToUtf16 call.
        // We should use char from 0x7f to 0x0 if we want to
        // generate one utf16 (0x00xx) from this mutf8.
        uint8_t val1 = rand();
        val1 = val1 >> 1;
        if (val1 == 0) {
            val1++;
        }

        uint8_t val2 = rand();
        val2 = val2 >> 1;
        if (val2 == 0) {
            val2++;
        }

        f_string[i] = val1;
        s_string[i] = val2;
    }
    // Set the last elements in strings with size more than 0x8 to disable compressing.
    // This will leads to count two MUtf-8 bytes as one UTF-16 so length = string_length - 1
    f_string[string_length - 2U] = uint8_t(0x80);
    s_string[string_length - 2U] = uint8_t(0x80);
    f_string[string_length - 1] = uint8_t(0x01);
    s_string[string_length - 1] = uint8_t(0x01);
    f_string[string_length] = '\0';
    s_string[string_length] = '\0';

    String *first_utf16_string = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(f_string), string_length - 1,
                                                         GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    // Try to use function with automatic length detection
    String *second_utf16_string = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(s_string),
                                                          GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(first_utf16_string->GetLength(), string_length - 1);
    ASSERT_EQ(second_utf16_string->GetLength(), string_length - 1);

    // Dirty hack to not create utf16 for our purpose, just reuse old one
    // Try to create compressed strings.
    String *first_utf8_string = String::CreateFromUtf16(first_utf16_string->GetDataUtf16(), string_length - 1,
                                                        GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    String *second_utf8_string = String::CreateFromUtf16(first_utf16_string->GetDataUtf16(), string_length - 1,
                                                         GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(first_utf8_string->GetLength(), string_length - 1);
    ASSERT_EQ(second_utf8_string->GetLength(), string_length - 1);

    ASSERT_EQ(String::StringsAreEqual(first_utf16_string, second_utf16_string), strcmp(f_string, s_string) == 0);
    ASSERT_EQ(String::StringsAreEqual(first_utf16_string, second_utf8_string),
              first_utf16_string->IsUtf16() == second_utf8_string->IsUtf16());
    ASSERT_EQ(String::StringsAreEqual(first_utf8_string, second_utf8_string), true);
    ASSERT_TRUE(first_utf16_string->IsUtf16());
    ASSERT_TRUE(String::StringsAreEqualUtf16(first_utf16_string, first_utf16_string->GetDataUtf16(),
                                             first_utf16_string->GetLength()));

    delete[] f_string;
    delete[] s_string;
}

TEST_F(StringTest, ObjectSize)
{
    {
        std::vector<uint8_t> data {'1', '2', '3', '4', '5', 0x00};
        uint32_t utf16_length = data.size();
        String *string = String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
        ASSERT_EQ(string->ObjectSize(), String::ComputeSizeMUtf8(utf16_length));
    }

    {
        std::vector<uint8_t> data {0x80, 0x01, 0x80, 0x02, 0x00};
        uint32_t utf16_length = data.size() / 2U;
        String *string = String::CreateFromMUtf8(data.data(), utf16_length, GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
        ASSERT_EQ(string->ObjectSize(), String::ComputeSizeUtf16(utf16_length));
    }
}

TEST_F(StringTest, AtTest)
{
    // utf8
    std::vector<uint8_t> data1 {'a', 'b', 'c', 'd', 'z', 0};
    String *string = String::CreateFromMUtf8(data1.data(), data1.size() - 1, GetLanguageContext(),
                                             Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(false, string->IsUtf16());
    for (uint32_t i = 0; i < data1.size() - 1; i++) {
        ASSERT_EQ(data1[i], string->At(i));
    }

    // utf16
    std::vector<uint16_t> data2 {'a', 'b', 0xab, 0xdc, 'z', 0};
    string = String::CreateFromUtf16(data2.data(), data2.size() - 1, GetLanguageContext(),
                                     Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(true, string->IsUtf16());
    for (uint32_t i = 0; i < data2.size() - 1; i++) {
        ASSERT_EQ(data2[i], string->At(i));
    }

    // utf16 -> utf8
    std::vector<uint16_t> data3 {'a', 'b', 121, 122, 'z', 0};
    string = String::CreateFromUtf16(data3.data(), data3.size() - 1, GetLanguageContext(),
                                     Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(false, string->IsUtf16());
    for (uint32_t i = 0; i < data3.size() - 1; i++) {
        ASSERT_EQ(data3[i], string->At(i));
    }
}

TEST_F(StringTest, IndexOfTest)
{
    std::vector<uint8_t> data1 {'a', 'b', 'c', 'd', 'z', 0};
    std::vector<uint8_t> data2 {'b', 'c', 'd', 0};
    std::vector<uint16_t> data3 {'a', 'b', 'c', 'd', 'z', 0};
    std::vector<uint16_t> data4 {'b', 'c', 'd', 0};
    String *string1 = String::CreateFromMUtf8(data1.data(), data1.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string2 = String::CreateFromMUtf8(data2.data(), data2.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string3 = String::CreateFromUtf16(data3.data(), data3.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string4 = String::CreateFromUtf16(data4.data(), data4.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());

    auto index = string1->IndexOf(string2, 1);
    auto index1 = string1->IndexOf(string4, 1);
    auto index2 = string3->IndexOf(string2, 1);
    auto index3 = string3->IndexOf(string4, 1);
    std::cout << index << std::endl;
    ASSERT_EQ(index, index2);
    ASSERT_EQ(index1, index3);
    index = string1->IndexOf(string2, 2);
    index1 = string1->IndexOf(string4, 2);
    index2 = string3->IndexOf(string2, 2);
    index3 = string3->IndexOf(string4, 2);
    std::cout << index << std::endl;
    ASSERT_EQ(index, index2);
    ASSERT_EQ(index1, index3);
}

TEST_F(StringTest, CompareTest)
{
    // utf8
    std::vector<uint8_t> data1 {'a', 'b', 'c', 'd', 'z', 0};
    std::vector<uint8_t> data2 {'a', 'b', 'c', 'd', 'z', 'x', 0};
    std::vector<uint16_t> data3 {'a', 'b', 'c', 'd', 'z', 0};
    std::vector<uint16_t> data4 {'a', 'b', 'd', 'c', 'z', 0};
    String *string1 = String::CreateFromMUtf8(data1.data(), data1.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string2 = String::CreateFromMUtf8(data2.data(), data2.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string3 = String::CreateFromUtf16(data3.data(), data3.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string4 = String::CreateFromUtf16(data4.data(), data4.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(false, string1->IsUtf16());
    ASSERT_EQ(false, string2->IsUtf16());
    ASSERT_EQ(false, string3->IsUtf16());
    ASSERT_EQ(false, string4->IsUtf16());
    ASSERT_LT(string1->Compare(string2), 0);
    ASSERT_GT(string2->Compare(string1), 0);
    ASSERT_EQ(string1->Compare(string3), 0);
    ASSERT_EQ(string3->Compare(string1), 0);
    ASSERT_LT(string2->Compare(string4), 0);
    ASSERT_GT(string4->Compare(string2), 0);

    // utf8 vs utf16
    std::vector<uint16_t> data5 {'a', 'b', 0xab, 0xdc, 'z', 0};
    String *string5 = String::CreateFromUtf16(data5.data(), data5.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(true, string5->IsUtf16());
    ASSERT_LT(string2->Compare(string5), 0);
    ASSERT_GT(string5->Compare(string2), 0);
    ASSERT_LT(string4->Compare(string5), 0);
    ASSERT_GT(string5->Compare(string4), 0);

    // utf16 vs utf16
    std::vector<uint16_t> data6 {'a', 0xab, 0xab, 0};
    String *string6 = String::CreateFromUtf16(data6.data(), data6.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string7 = String::CreateFromUtf16(data6.data(), data6.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(true, string6->IsUtf16());
    ASSERT_EQ(true, string7->IsUtf16());
    ASSERT_LT(string5->Compare(string6), 0);
    ASSERT_GT(string6->Compare(string5), 0);
    ASSERT_EQ(string6->Compare(string7), 0);
    ASSERT_EQ(string7->Compare(string6), 0);

    // compare with self
    ASSERT_EQ(string1->Compare(string1), 0);
    ASSERT_EQ(string2->Compare(string2), 0);
    ASSERT_EQ(string3->Compare(string3), 0);
    ASSERT_EQ(string4->Compare(string4), 0);
    ASSERT_EQ(string5->Compare(string5), 0);
    ASSERT_EQ(string6->Compare(string6), 0);
    ASSERT_EQ(string7->Compare(string7), 0);
}

TEST_F(StringTest, ConcatTest)
{
    // utf8 + utf8
    std::vector<uint8_t> data1 {'f', 'g', 'h', 0};
    std::vector<uint8_t> data2 {'a', 'b', 'c', 'd', 'e', 0};
    std::vector<uint8_t> data3;
    data3.insert(data3.end(), data1.begin(), data1.end() - 1);
    data3.insert(data3.end(), data2.begin(), data2.end());

    String *string1 = String::CreateFromMUtf8(data1.data(), data1.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string2 = String::CreateFromMUtf8(data2.data(), data2.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string30 = String::CreateFromMUtf8(data3.data(), data3.size() - 1, GetLanguageContext(),
                                               Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(false, string1->IsUtf16());
    ASSERT_EQ(false, string2->IsUtf16());
    String *string31 = String::Concat(string1, string2, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string30->Compare(string31), 0);
    ASSERT_EQ(string31->Compare(string30), 0);

    // utf8 + utf16
    std::vector<uint16_t> data4 {'a', 'b', 0xab, 0xdc, 'z', 0};
    std::vector<uint16_t> data5 {'f', 'g', 'h', 'a', 'b', 0xab, 0xdc, 'z', 0};  // data1 + data4
    String *string4 = String::CreateFromUtf16(data4.data(), data4.size() - 1, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());
    String *string50 = String::CreateFromUtf16(data5.data(), data5.size() - 1, GetLanguageContext(),
                                               Runtime::GetCurrent()->GetPandaVM());
    String *string51 = String::Concat(string1, string4, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string50->GetLength(), string51->GetLength());
    ASSERT_EQ(string50->Compare(string51), 0);
    ASSERT_EQ(string51->Compare(string50), 0);

    // utf16 + utf16
    std::vector<uint16_t> data6;
    data6.insert(data6.end(), data4.begin(), data4.end() - 1);
    data6.insert(data6.end(), data5.begin(), data5.end());
    String *string60 = String::CreateFromUtf16(data6.data(), data6.size() - 1, GetLanguageContext(),
                                               Runtime::GetCurrent()->GetPandaVM());
    String *string61 = String::Concat(string4, string50, GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(string60->Compare(string61), 0);
    ASSERT_EQ(string61->Compare(string60), 0);
}

TEST_F(StringTest, DoReplaceTest0)
{
    static constexpr uint32_t string_length = 10;
    char *f_string = new char[string_length + 1];
    char *s_string = new char[string_length + 1];

    for (uint32_t i = 0; i < string_length; i++) {
        f_string[i] = 'A' + i;
        s_string[i] = 'A' + i;
    }
    f_string[0] = 'Z';
    f_string[string_length] = '\0';
    s_string[string_length] = '\0';

    String *f_string_s = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(f_string), GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
    String *s_string_s = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(s_string), GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
    String *t_string_s =
        String::DoReplace(f_string_s, 'Z', 'A', GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(String::StringsAreEqual(t_string_s, s_string_s), true);

    delete[] f_string;
    delete[] s_string;
}

TEST_F(StringTest, FastSubstringTest0)
{
    uint32_t string_length = 10;
    char *f_string = new char[string_length + 1];
    for (uint32_t i = 0; i < string_length; i++) {
        f_string[i] = 'A' + i;
    }
    f_string[string_length] = '\0';

    uint32_t sub_string_length = 5;
    uint32_t sub_string_start = 1;
    char *s_string = new char[sub_string_length + 1];
    for (uint32_t j = 0; j < sub_string_length; j++) {
        s_string[j] = f_string[sub_string_start + j];
    }
    s_string[sub_string_length] = '\0';

    String *f_string_s = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(f_string), GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
    String *s_string_s = String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(s_string), GetLanguageContext(),
                                                 Runtime::GetCurrent()->GetPandaVM());
    String *t_string_s = String::FastSubString(f_string_s, sub_string_start, sub_string_length, GetLanguageContext(),
                                               Runtime::GetCurrent()->GetPandaVM());
    ASSERT_EQ(String::StringsAreEqual(t_string_s, s_string_s), true);

    delete[] f_string;
    delete[] s_string;
}

TEST_F(StringTest, ToCharArray)
{
    // utf8
    std::vector<uint8_t> data {'a', 'b', 'c', 'd', 'e', 0};
    String *utf8_string = String::CreateFromMUtf8(data.data(), data.size() - 1, GetLanguageContext(),
                                                  Runtime::GetCurrent()->GetPandaVM());
    Array *new_array = utf8_string->ToCharArray(GetLanguageContext());
    for (uint32_t i = 0; i < new_array->GetLength(); ++i) {
        ASSERT_EQ(data[i], new_array->Get<uint16_t>(i));
    }

    std::vector<uint16_t> data1 {'f', 'g', 'h', 'a', 'b', 0x8ab, 0xdc, 'z', 0};
    String *utf16_string = String::CreateFromUtf16(data1.data(), data1.size() - 1, GetLanguageContext(),
                                                   Runtime::GetCurrent()->GetPandaVM());
    Array *new_array1 = utf16_string->ToCharArray(GetLanguageContext());
    for (uint32_t i = 0; i < new_array1->GetLength(); ++i) {
        ASSERT_EQ(data1[i], new_array1->Get<uint16_t>(i));
    }
}

TEST_F(StringTest, CreateNewStingFromCharArray)
{
    std::vector<uint16_t> data {'f', 'g', 'h', 'a', 'b', 0x8ab, 0xdc, 'z', 0};
    String *utf16_string = String::CreateFromUtf16(data.data(), data.size() - 1, GetLanguageContext(),
                                                   Runtime::GetCurrent()->GetPandaVM());
    Array *char_array = utf16_string->ToCharArray(GetLanguageContext());

    uint32_t char_array_length = 5;
    uint32_t char_array_offset = 1;
    std::vector<uint16_t> data1(char_array_length + 1);
    for (uint32_t i = 0; i < char_array_length; ++i) {
        data1[i] = data[i + char_array_offset];
    }
    data1[char_array_length] = 0;
    String *utf16_string1 = String::CreateFromUtf16(data1.data(), data1.size() - 1, GetLanguageContext(),
                                                    Runtime::GetCurrent()->GetPandaVM());

    String *result = String::CreateNewStringFromChars(char_array_offset, char_array_length, char_array,
                                                      GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());

    ASSERT_EQ(String::StringsAreEqual(result, utf16_string1), true);
}

TEST_F(StringTest, CreateNewStingFromByteArray)
{
    std::vector<uint8_t> data {'f', 'g', 'h', 'a', 'b', 0xab, 0xdc, 'z', 0};
    uint32_t byte_array_length = 5;
    uint32_t byte_array_offset = 1;
    uint32_t high_byte = 0;

    std::vector<uint16_t> data1(byte_array_length);
    for (uint32_t i = 0; i < byte_array_length; ++i) {
        data1[i] = (high_byte << 8) + (data[i + byte_array_offset] & 0xFF);
    }
    // NB! data1[byte_array_length] = 0; NOT NEEDED
    String *string1 = String::CreateFromUtf16(data1.data(), byte_array_length, GetLanguageContext(),
                                              Runtime::GetCurrent()->GetPandaVM());

    // NB! LanguageContext ctx = LanguageContext(panda_file::SourceLang::JAVA_8); NOT CORRECT
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *klass = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx)->GetClassRoot(panda::ClassRoot::ARRAY_I8);
    Array *byte_array = Array::Create(klass, data.size() - 1);
    Span<uint8_t> sp(data.data(), data.size() - 1);
    for (uint32_t i = 0; i < data.size() - 1; i++) {
        byte_array->Set<uint8_t>(i, sp[i]);
    }

    String *result = String::CreateNewStringFromBytes(byte_array_offset, byte_array_length, high_byte, byte_array,
                                                      GetLanguageContext(), Runtime::GetCurrent()->GetPandaVM());

    ASSERT_EQ(String::StringsAreEqual(result, string1), true);
}

}  // namespace panda::coretypes::test
