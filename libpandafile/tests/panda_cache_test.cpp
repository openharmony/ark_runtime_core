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

#include "file.h"
#include "panda_cache.h"

#include <gtest/gtest.h>
#include <thread>

namespace panda {

class Method;
class Field;
class Class;

namespace panda_file::test {

using EntityId = File::EntityId;

static void *GetNewMockPointer()
{
    static int id = 1;
    return reinterpret_cast<void *>(id++);
}
TEST(PandaCache, TestMethodCache)
{
    PandaCache cache;
    EntityId id1(100);
    ASSERT_EQ(cache.GetMethodFromCache(id1), nullptr);

    auto *method1 = reinterpret_cast<Method *>(GetNewMockPointer());
    cache.SetMethodCache(id1, method1);
    ASSERT_EQ(cache.GetMethodFromCache(id1), method1);

    EntityId id2(10000);
    auto *method2 = reinterpret_cast<Method *>(GetNewMockPointer());
    cache.SetMethodCache(id2, method2);
    ASSERT_EQ(cache.GetMethodFromCache(id2), method2);
}

TEST(PandaCache, TestFieldCache)
{
    PandaCache cache;
    EntityId id1(100);
    EntityId new_id1(id1.GetOffset() << 2U);
    ASSERT_EQ(cache.GetFieldFromCache(new_id1), nullptr);

    auto *field1 = reinterpret_cast<Field *>(GetNewMockPointer());
    cache.SetFieldCache(new_id1, field1);
    ASSERT_EQ(cache.GetFieldFromCache(new_id1), field1);

    EntityId id2(10000);
    EntityId new_id2(id2.GetOffset() << 2U);
    auto *field2 = reinterpret_cast<Field *>(GetNewMockPointer());
    cache.SetFieldCache(new_id2, field2);
    ASSERT_EQ(cache.GetFieldFromCache(new_id2), field2);
}

TEST(PandaCache, TestClassCache)
{
    PandaCache cache;
    EntityId id1(100);
    ASSERT_EQ(cache.GetClassFromCache(id1), nullptr);

    auto *class1 = reinterpret_cast<Class *>(GetNewMockPointer());
    cache.SetClassCache(id1, class1);
    ASSERT_EQ(cache.GetClassFromCache(id1), class1);

    EntityId id2(10000);
    auto *class2 = reinterpret_cast<Class *>(GetNewMockPointer());
    cache.SetClassCache(id2, class2);
    ASSERT_EQ(cache.GetClassFromCache(id2), class2);
}

struct ElementMock {
    int data;
};

static ElementMock *GetNewMockElement(int i)
{
    auto *m = new ElementMock();
    m->data = i;
    return m;
}

static const int NUMBER_OF_READERS = 4;
static const int NUMBER_OF_ELEMENTS = 4;

class CacheOps {
public:
    explicit CacheOps(PandaCache *cache) : cache_(cache) {}

    virtual ~CacheOps() = default;

    void runWriter()
    {
        for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
            EntityId id(i);
            auto *m = GetNewMockElement(i);
            SetElement(id, m);
            ASSERT_EQ(GetElement(id), m);
        }
    }

    void runReader() const
    {
        for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
            EntityId id(i);
            auto *m = GetElement(id);
            while (m == nullptr) {
                m = GetElement(id);
            }
            int d = m->data;
            ASSERT_EQ(d, i);
        }
    }

protected:
    PandaCache *cache_;
    virtual ElementMock *GetElement(EntityId id) const = 0;
    virtual void SetElement(EntityId id, ElementMock *m) = 0;
};

class MethodCacheOps : public CacheOps {
public:
    explicit MethodCacheOps(PandaCache *cache) : CacheOps(cache) {}

protected:
    ElementMock *GetElement(EntityId id) const override
    {
        Method *f = cache_->GetMethodFromCache(id);
        if (f == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<ElementMock *>(f);
    }

    void SetElement(EntityId id, ElementMock *m) override
    {
        auto *f = reinterpret_cast<Method *>(m);
        cache_->SetMethodCache(id, f);
    }
};

class FieldCacheOps : public CacheOps {
public:
    explicit FieldCacheOps(PandaCache *cache) : CacheOps(cache) {}

protected:
    ElementMock *GetElement(EntityId id) const override
    {
        // CacheOps.runReader expect no conflicts
        EntityId new_id(id.GetOffset() << 2U);
        Field *f = cache_->GetFieldFromCache(new_id);
        if (f == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<ElementMock *>(f);
    }

    void SetElement(EntityId id, ElementMock *m) override
    {
        // CacheOps.runReader expect no conflicts
        EntityId new_id(id.GetOffset() << 2U);
        auto *f = reinterpret_cast<Field *>(m);
        cache_->SetFieldCache(new_id, f);
    }
};

class ClassCacheOps : public CacheOps {
public:
    explicit ClassCacheOps(PandaCache *cache) : CacheOps(cache) {}

protected:
    ElementMock *GetElement(EntityId id) const override
    {
        Class *cl = cache_->GetClassFromCache(id);
        if (cl == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<ElementMock *>(cl);
    }

    void SetElement(EntityId id, ElementMock *m) override
    {
        auto *cl = reinterpret_cast<Class *>(m);
        cache_->SetClassCache(id, cl);
    }
};

void MethodWriterThread(PandaCache *cache)
{
    MethodCacheOps ops(cache);
    ops.runWriter();
}

void MethodReaderThread(PandaCache *cache)
{
    MethodCacheOps ops(cache);
    ops.runReader();
}

void FieldWriterThread(PandaCache *cache)
{
    FieldCacheOps ops(cache);
    ops.runWriter();
}

void FieldReaderThread(PandaCache *cache)
{
    FieldCacheOps ops(cache);
    ops.runReader();
}

void ClassWriterThread(PandaCache *cache)
{
    ClassCacheOps ops(cache);
    ops.runWriter();
}

void ClassReaderThread(PandaCache *cache)
{
    ClassCacheOps ops(cache);
    ops.runReader();
}

void cleanMethodMocks(const PandaCache *cache)
{
    for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
        EntityId id(i);
        auto *m = reinterpret_cast<ElementMock *>(cache->GetMethodFromCache(id));
        delete m;
    }
}

void cleanFieldMocks(const PandaCache *cache)
{
    for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
        EntityId id(i);
        EntityId new_id(id.GetOffset() << 2U);
        auto *m = reinterpret_cast<ElementMock *>(cache->GetFieldFromCache(new_id));
        delete m;
    }
}

void cleanClassMocks(const PandaCache *cache)
{
    for (int i = 0; i < NUMBER_OF_ELEMENTS; i++) {
        EntityId id(i);
        auto *m = reinterpret_cast<ElementMock *>(cache->GetClassFromCache(id));
        delete m;
    }
}

TEST(PandaCache, TestManyThreadsMethodCache)
{
    PandaCache cache;

    std::thread threads[NUMBER_OF_READERS];
    auto writer = std::thread(MethodWriterThread, &cache);
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i] = std::thread(MethodReaderThread, &cache);
    }
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i].join();
    }
    writer.join();
    cleanMethodMocks(&cache);
}

TEST(PandaCache, TestManyThreadsFieldCache)
{
    PandaCache cache;

    std::thread threads[NUMBER_OF_READERS];
    auto writer = std::thread(FieldWriterThread, &cache);
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i] = std::thread(FieldReaderThread, &cache);
    }
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i].join();
    }
    writer.join();
    cleanFieldMocks(&cache);
}

TEST(PandaCache, TestManyThreadsClassCache)
{
    PandaCache cache;

    std::thread threads[NUMBER_OF_READERS];
    auto writer = std::thread(ClassWriterThread, &cache);
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i] = std::thread(ClassReaderThread, &cache);
    }
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        threads[i].join();
    }
    writer.join();
    cleanClassMocks(&cache);
}

}  // namespace panda_file::test
}  // namespace panda
