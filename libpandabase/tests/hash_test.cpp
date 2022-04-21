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
#include "utils/hash.h"

#include "gtest/gtest.h"
#include "utils/logger.h"
#include "mem/mem.h"
#include "os/mem.h"
#include "utils/asan_interface.h"

namespace panda {

class HashTest : public testing::Test {
public:
    HashTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0xDEADBEEF;
#endif
    }
    ~HashTest() {}

protected:
    template <class T>
    void OneObject32bitsHashTest() const;
    template <class T>
    void OneStringHashTest() const;
    template <class T>
    void StringMemHashTest() const;
    template <class T>
    void EndOfPageStringHashTest() const;
    static constexpr size_t KEY40INBYTES = 5;
    static constexpr size_t KEY32INBYTES = 4;
    static constexpr size_t KEY8INBYTES = 1;

#ifndef PAGE_SIZE
    static constexpr size_t PAGE_SIZE = SIZE_1K * 4;
#endif

    unsigned seed_;
};

template <class T>
void HashTest::OneObject32bitsHashTest() const
{
    srand(seed_);

    uint32_t object32 = rand();
    uint32_t first_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object32), KEY32INBYTES);
    uint32_t second_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object32), KEY32INBYTES);
    if (first_hash != second_hash) {
        std::cout << "Failed 32bit key hash on seed = 0x" << std::hex << seed_ << std::endl;
    }
    ASSERT_EQ(first_hash, second_hash);

    uint8_t object8 = rand();
    first_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object8), KEY8INBYTES);
    second_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object8), KEY8INBYTES);
    if (first_hash != second_hash) {
        std::cout << "Failed 32bit key hash on seed = 0x" << std::hex << seed_ << std::endl;
    }
    ASSERT_EQ(first_hash, second_hash);

    // Set 64 bits value and use only 40 bits from it
    uint64_t object40 = rand();
    first_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object40), KEY40INBYTES);
    second_hash = T::GetHash32(reinterpret_cast<uint8_t *>(&object40), KEY40INBYTES);
    if (first_hash != second_hash) {
        std::cout << "Failed 32bit key hash on seed = 0x" << std::hex << seed_ << std::endl;
    }
    ASSERT_EQ(first_hash, second_hash);
}

template <class T>
void HashTest::OneStringHashTest() const
{
    char string[] = "Over 1000!\0";
    // Dummy check
    if (sizeof(char) != sizeof(uint8_t)) {
        return;
    }
    uint8_t *mutf8_string = reinterpret_cast<uint8_t *>(string);
    uint32_t first_hash = T::GetHash32String(mutf8_string);
    uint32_t second_hash = T::GetHash32String(mutf8_string);
    ASSERT_EQ(first_hash, second_hash);
}

template <class T>
void HashTest::StringMemHashTest() const
{
    char string[] = "COULD YOU CREATE MORE COMPLEX TESTS,OK?\0";
    size_t string_size = strlen(string);
    uint8_t *mutf8_string = reinterpret_cast<uint8_t *>(string);
    uint32_t second_hash = T::GetHash32(mutf8_string, string_size);
    uint32_t first_hash = T::GetHash32String(mutf8_string);
    ASSERT_EQ(first_hash, second_hash);
}

template <class T>
void HashTest::EndOfPageStringHashTest() const
{
    size_t string_size = 3;
    constexpr size_t ALLOC_SIZE = PAGE_SIZE * 2;
    void *mem = panda::os::mem::MapRWAnonymousRaw(ALLOC_SIZE);
    ASAN_UNPOISON_MEMORY_REGION(mem, ALLOC_SIZE);
    panda::os::mem::MakeMemWithProtFlag(
        reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(mem) + PAGE_SIZE), PAGE_SIZE, PROT_NONE);
    char *string =
        reinterpret_cast<char *>((reinterpret_cast<uintptr_t>(mem) + PAGE_SIZE) - sizeof(char) * string_size);
    string[0] = 'O';
    string[1] = 'K';
    string[2U] = '\0';
    uint8_t *mutf8_string = reinterpret_cast<uint8_t *>(string);
    uint32_t second_hash = T::GetHash32(mutf8_string, string_size - 1);
    uint32_t first_hash = T::GetHash32String(mutf8_string);
    ASSERT_EQ(first_hash, second_hash);
    auto res = panda::os::mem::UnmapRaw(mem, ALLOC_SIZE);
    ASSERT_FALSE(res);
}

// If we hash an object twice, it must return the same value
// Do it for 8 bits, 32 bits and 40 bits key.
TEST_F(HashTest, OneObjectHashTest)
{
    HashTest::OneObject32bitsHashTest<MurmurHash32<DEFAULT_SEED>>();
}

// If we hash a string twice, it must return the same value
TEST_F(HashTest, OneStringHashTest)
{
    HashTest::OneStringHashTest<MurmurHash32<DEFAULT_SEED>>();
}

// If we hash a string without string method,
// we should get the same result as we use a pointer to string as a raw memory.
TEST_F(HashTest, StringMemHashTest)
{
    HashTest::StringMemHashTest<MurmurHash32<DEFAULT_SEED>>();
}

// Try to hash the string which is located at the end of allocated page.
// Check that we will not have SEGERROR here.
TEST_F(HashTest, EndOfPageStringHashTest)
{
    HashTest::EndOfPageStringHashTest<MurmurHash32<DEFAULT_SEED>>();
}

}  // namespace panda
