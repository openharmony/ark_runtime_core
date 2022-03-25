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
// All common ObjectHeader methods can be found here:
// - Get/Set Mark or Class word
// - Get size of the object header and an object itself
// - Get/Generate an object hash
// Methods, specific for Class word:
// - Get different object fields
// - Return object type
// - Verify object
// - Is it a subclass of not
// - Get field addr
// Methods, specific for Mark word:
// - Object locked/unlocked
// - Marked for GC or not
// - Monitor functions (get monitor, notify, notify all, wait)
// - Forwarded or not

#ifndef PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_H_
#define PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_H_

#include <atomic>
#include <ctime>

#include "runtime/include/class_helper.h"
#include "runtime/mark_word.h"

namespace panda {

namespace object_header_traits {

constexpr const uint32_t LINEAR_X = 1103515245U;
constexpr const uint32_t LINEAR_Y = 12345U;
constexpr const uint32_t LINEAR_SEED = 987654321U;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
static auto hash_seed = std::atomic<uint32_t>(LINEAR_SEED + std::time(nullptr));

}  // namespace object_header_traits

class BaseClass;
class Class;
class Field;
class ManagedThread;

class ObjectHeader {
public:
    // Simple getters and setters for Class and Mark words.
    // Use it only in single thread
    inline MarkWord GetMark() const
    {
        return *(const_cast<MarkWord *>(reinterpret_cast<const MarkWord *>(&markWord_)));
    }
    inline void SetMark(volatile MarkWord mark_word)
    {
        markWord_ = mark_word.Value();
    }

    inline MarkWord AtomicGetMark() const
    {
        auto ptr = const_cast<MarkWord *>(reinterpret_cast<const MarkWord *>(&markWord_));
        auto atomic_ptr = reinterpret_cast<std::atomic<MarkWord> *>(ptr);
        return atomic_ptr->load();
    }

    inline void SetClass(BaseClass *klass)
    {
        static_assert(sizeof(ClassHelper::classWordSize) == sizeof(object_pointer_type));
        reinterpret_cast<std::atomic<ClassHelper::classWordSize> *>(&classWord_)
            ->store(static_cast<ClassHelper::classWordSize>(ToObjPtrType(klass)), std::memory_order_release);
        ASSERT(AtomicClassAddr<BaseClass>() == klass);
    }

    template <typename T>
    inline T *ClassAddr() const
    {
        return AtomicClassAddr<T>();
    }

    template <typename T>
    inline T *AtomicClassAddr() const
    {
        auto ptr = const_cast<ClassHelper::classWordSize *>(&classWord_);
        return reinterpret_cast<T *>(
            reinterpret_cast<std::atomic<ClassHelper::classWordSize> *>(ptr)->load(std::memory_order_acquire));
    }

    // Generate hash value for an object.
    static inline uint32_t GenerateHashCode()
    {
        uint32_t ex_val;
        uint32_t n_val;
        do {
            ex_val = object_header_traits::hash_seed.load(std::memory_order_relaxed);
            n_val = ex_val * object_header_traits::LINEAR_X + object_header_traits::LINEAR_Y;
        } while (!object_header_traits::hash_seed.compare_exchange_weak(ex_val, n_val, std::memory_order_relaxed) ||
                 (ex_val & MarkWord::HASH_MASK) == 0);
        return ex_val & MarkWord::HASH_MASK;
    }

    // Get Hash value for an object.
    uint32_t GetHashCode();
    uint32_t GetHashCodeFromMonitor(Monitor *monitor_p);

    // Size of object header
    static constexpr int ObjectHeaderSize()
    {
        return sizeof(ObjectHeader);
    }

    static constexpr size_t GetClassOffset()
    {
        return MEMBER_OFFSET(ObjectHeader, classWord_);
    }

    static constexpr size_t GetMarkWordOffset()
    {
        return MEMBER_OFFSET(ObjectHeader, markWord_);
    }

    // Garbage collection method
    template <bool atomic_flag = true>
    inline bool IsMarkedForGC() const
    {
        if constexpr (!atomic_flag) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            return GetMark().IsMarkedForGC();
        }
        return AtomicGetMark().IsMarkedForGC();
    }
    template <bool atomic_flag = true>
    inline void SetMarkedForGC()
    {
        if constexpr (!atomic_flag) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            SetMark(GetMark().SetMarkedForGC());
            return;
        }
        bool res;
        do {
            MarkWord word = AtomicGetMark();
            res = AtomicSetMark(word, word.SetMarkedForGC());
        } while (!res);
    }
    template <bool atomic_flag = true>
    inline void SetUnMarkedForGC()
    {
        if constexpr (!atomic_flag) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            SetMark(GetMark().SetUnMarkedForGC());
            return;
        }
        bool res;
        do {
            MarkWord word = AtomicGetMark();
            res = AtomicSetMark(word, word.SetUnMarkedForGC());
        } while (!res);
    }
    inline bool IsForwarded() const
    {
        return AtomicGetMark().GetState() == MarkWord::ObjectState::STATE_GC;
    }

    // Type test methods
    inline bool IsInstance() const;

    // Get field address in Class
    inline void *FieldAddr(int offset) const;

    bool AtomicSetMark(MarkWord old_mark_word, MarkWord new_mark_word);

    // Accessors to typical Class types
    template <class T, bool is_volatile = false>
    T GetFieldPrimitive(size_t offset) const;

    template <class T, bool is_volatile = false>
    void SetFieldPrimitive(size_t offset, T value);

    template <bool is_volatile = false, bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetFieldObject(int offset) const;

    template <bool is_volatile = false, bool need_write_barrier = true, bool is_dyn = false>
    void SetFieldObject(size_t offset, ObjectHeader *value);

    template <class T>
    T GetFieldPrimitive(const Field &field) const;

    template <class T>
    void SetFieldPrimitive(const Field &field, T value);

    template <bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetFieldObject(const Field &field) const;

    template <bool need_write_barrier = true, bool is_dyn = false>
    void SetFieldObject(const Field &field, ObjectHeader *value);

    // Pass thread parameter to speed up interpreter
    template <bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetFieldObject(ManagedThread *thread, const Field &field);

    template <bool need_write_barrier = true, bool is_dyn = false>
    void SetFieldObject(ManagedThread *thread, const Field &field, ObjectHeader *value);

    template <bool is_volatile = false, bool need_write_barrier = true, bool is_dyn = false>
    void SetFieldObject(ManagedThread *thread, size_t offset, ObjectHeader *value);

    template <class T>
    T GetFieldPrimitive(size_t offset, std::memory_order memory_order) const;

    template <class T>
    void SetFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetFieldObject(size_t offset, std::memory_order memory_order) const;

    template <bool need_write_barrier = true, bool is_dyn = false>
    void SetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    bool CompareAndSetFieldPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                     bool strong);

    template <bool need_write_barrier = true, bool is_dyn = false>
    bool CompareAndSetFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                  std::memory_order memory_order, bool strong);

    template <typename T>
    T CompareAndExchangeFieldPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                       bool strong);

    template <bool need_write_barrier = true, bool is_dyn = false>
    ObjectHeader *CompareAndExchangeFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                                std::memory_order memory_order, bool strong);

    template <typename T>
    T GetAndSetFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_write_barrier = true, bool is_dyn = false>
    ObjectHeader *GetAndSetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    T GetAndAddFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseOrFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseAndFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseXorFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    /*
     * Is the object is an instance of specified class.
     * Object of type O is instance of type T if O is the same as T or is subtype of T. For arrays T should be a root
     * type in type hierarchy or T is such array that O array elements are the same or subtype of T array elements.
     */
    inline bool IsInstanceOf(Class *klass);

    // Verification methods
    static void Verify(ObjectHeader *object_header);

    static ObjectHeader *Create(BaseClass *klass);

    static ObjectHeader *CreateNonMovable(BaseClass *klass);

    static ObjectHeader *Clone(ObjectHeader *src);

    static ObjectHeader *ShallowCopy(ObjectHeader *src);

    size_t ObjectSize() const;

private:
    MarkWord::markWordSize markWord_;
    ClassHelper::classWordSize classWord_;

    /**
     * Allocates memory for the Object. No ctor is called.
     * @param klass - class of Object
     * @param non_movable - if true, object will be allocated in non-movable space
     * @return pointer to the created Object
     */
    static ObjectHeader *CreateObject(BaseClass *klass, bool non_movable);
};

constexpr uint32_t OBJECT_HEADER_CLASS_OFFSET = 4U;
static_assert(OBJECT_HEADER_CLASS_OFFSET == panda::ObjectHeader::GetClassOffset());

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_H_
