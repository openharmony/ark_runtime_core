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

#ifndef PANDA_LIBPANDABASE_UTILS_LIST_H_
#define PANDA_LIBPANDABASE_UTILS_LIST_H_

#include "macros.h"

namespace panda {

template <typename T>
class List;
template <typename T>
class ListIterator;

/**
 * Intrusive forward list node, which shall be inherited by list element class.
 */
class ListNode {
public:
    ListNode() = default;
    ~ListNode() = default;

    explicit ListNode(ListNode *next) : next_(next) {}
    ListNode(const ListNode & /* unused */) : next_(nullptr) {}
    ListNode(ListNode && /* unused */) noexcept : next_(nullptr) {}

    ListNode &operator=(const ListNode & /* unused */)  // NOLINT(bugprone-unhandled-self-assignment, cert-oop54-cpp)
    {
        return *this;
    }
    ListNode &operator=(ListNode && /* unused */) noexcept
    {
        return *this;
    }

private:
    mutable const ListNode *next_ {nullptr};

    template <typename T>
    friend class List;
    template <typename T>
    friend class ListIterator;
};

/**
 * Intrusive forward list iterator
 */
template <typename T>
class ListIterator : public std::iterator<std::forward_iterator_tag, T> {
public:
    ListIterator() = default;
    explicit ListIterator(const ListNode *node) : node_(node) {}

    template <typename OtherT, typename = typename std::enable_if<std::is_same<T, const OtherT>::value>::type>
    ListIterator(const ListIterator<OtherT> &src)  // NOLINT(google-explicit-constructor)
        : node_(src.node_)
    {
    }

    ListIterator &operator++()
    {
        ASSERT(node_);
        node_ = node_->next_;
        return *this;
    }

    const ListIterator operator++(int)  // NOLINT(readability-const-return-type)
    {
        ASSERT(node_);
        ListIterator tmp(*this);
        node_ = node_->next_;
        return tmp;
    }

    ListIterator operator+(int n)
    {
        ASSERT(node_);
        ListIterator tmp(*this);
        std::advance(tmp, n);
        return tmp;
    }

    T &operator*() const
    {
        return *(static_cast<T *>(const_cast<ListNode *>(node_)));
    }

    T *operator->() const
    {
        return static_cast<T *>(const_cast<ListNode *>(node_));
    }

    ~ListIterator() = default;

    DEFAULT_COPY_SEMANTIC(ListIterator);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(ListIterator);

private:
    const ListNode *node_ {nullptr};

    template <typename OtherT>
    friend class ListIterator;

    template <typename OtherT>
    friend class List;

    template <typename T1, typename T2>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend typename std::enable_if<std::is_same<const T1, const T2>::value, bool>::type operator==(
        const ListIterator<T1> &lhs, const ListIterator<T2> &rhs);
};

/**
 * Intrusive forward list
 */
template <typename T>
class List {
public:
    using ValueType = T;
    using Reference = T &;
    using ConstReference = const T &;
    using Iterator = ListIterator<T>;
    using ConstIterator = ListIterator<const T>;

    List() = default;
    List(const List & /* unused */) = delete;
    List(List &&other) noexcept
    {
        head_ = other.head_;
        other.head_ = nullptr;
    }
    ~List() = default;

    List &operator=(const List & /* unused */) = delete;
    List &operator=(List &&other) noexcept
    {
        head_ = other.head_;
        other.head_ = nullptr;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator before_begin()
    {
        return Iterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator before_begin() const
    {
        return ConstIterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator begin()
    {
        return Iterator(head_.next_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator begin() const
    {
        return ConstIterator(head_.next_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator end()
    {
        return Iterator(nullptr);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator end() const
    {
        return ConstIterator(nullptr);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator cbefore_begin() const
    {
        return ConstIterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator cbegin() const
    {
        return ConstIterator(head_.next_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator cend() const
    {
        return ConstIterator(nullptr);
    }

    bool Empty() const
    {
        return begin() == end();
    }

    Reference Front()
    {
        return *begin();
    }

    ConstReference Front() const
    {
        return *begin();
    }

    void PushFront(ValueType &value)
    {
        InsertAfter(before_begin(), value);
    }

    void PushFront(ValueType &&value)
    {
        InsertAfter(before_begin(), value);
    }

    void PopFront()
    {
        ASSERT(!Empty());
        EraseAfter(before_begin());
    }

    Iterator InsertAfter(ConstIterator position, ValueType &value)
    {
        auto new_node = static_cast<const ListNode *>(&value);
        new_node->next_ = position.node_->next_;
        position.node_->next_ = new_node;
        return Iterator(new_node);
    }

    template <typename InputIterator>
    Iterator InsertAfter(ConstIterator position, InputIterator first, InputIterator last)
    {
        while (first != last) {
            position = InsertAfter(position, *first++);
        }
        return Iterator(position.node_);
    }

    Iterator EraseAfter(ConstIterator position)
    {
        ConstIterator last = position;
        constexpr size_t SHIFT = 2;
        std::advance(last, SHIFT);
        return EraseAfter(position, last);
    }

    /**
     * Erase elements in range (position, last)
     */
    Iterator EraseAfter(ConstIterator position, ConstIterator last)
    {
        ASSERT(position != last);
        position.node_->next_ = last.node_;
        return Iterator(last.node_);
    }

    bool Remove(const ValueType &value)
    {
        return RemoveIf([&value](const ValueType &v) { return value == v; });
    }

    template <typename Predicate>
    bool RemoveIf(Predicate pred)
    {
        bool found = false;
        Iterator prev = before_begin();
        for (Iterator current = begin(); current != end(); ++current) {
            if (pred(*current)) {
                found = true;
                EraseAfter(prev);
                current = prev;
            } else {
                prev = current;
            }
        }
        return found;
    }

    void Swap(List &other) noexcept
    {
        std::swap(head_.next_, other.head_.next_);
    }

    void Clear()
    {
        head_.next_ = nullptr;
    }

    /**
     * Transfer all elements from other list into place after position.
     */
    void Splice(ConstIterator position, List &other)
    {
        Splice(position, other, other.before_begin(), other.end());
    }

    /**
     * Transfer single element first+1 into place after position.
     */
    void Splice(ConstIterator position, List &other, ConstIterator first)
    {
        constexpr size_t SHIFT = 2;
        Splice(position, other, first, first + SHIFT);
    }

    /**
     * Transfer the elements in the range (first,last) into place after position.
     */
    void Splice(ConstIterator position, List &src_list, ConstIterator first, ConstIterator last)
    {
        ASSERT(position != end());
        ASSERT(first != last);

        if (++ConstIterator(first) == last) {
            return;
        }

        if (++ConstIterator(position) == end() && last == src_list.end()) {
            position.node_->next_ = first.node_->next_;
            first.node_->next_ = nullptr;
            return;
        }
        ConstIterator before_last = first;
        while (++ConstIterator(before_last) != last) {
            ++before_last;
        }

        const ListNode *first_taken = first.node_->next_;
        first.node_->next_ = last.node_;
        before_last.node_->next_ = position.node_->next_;
        position.node_->next_ = first_taken;
    }

private:
    ListNode head_;
};

template <typename T, typename OtherT>
typename std::enable_if<std::is_same<const T, const OtherT>::value, bool>::type operator==(
    const ListIterator<T> &lhs, const ListIterator<OtherT> &rhs)
{
    return lhs.node_ == rhs.node_;
}

template <typename T, typename OtherT>
typename std::enable_if<std::is_same<const T, const OtherT>::value, bool>::type operator!=(
    const ListIterator<T> &lhs, const ListIterator<OtherT> &rhs)
{
    return !(lhs == rhs);
}

/**
 * Intrusive doubly list node
 */
struct DListNode {
    DListNode *prev = nullptr;
    DListNode *next = nullptr;
};

/**
 * Intrusive doubly list iterator
 */
template <typename T>
class DListIterator {
public:
    DListIterator() = default;
    ~DListIterator() = default;

    explicit DListIterator(T *node) : node_(node) {}
    DEFAULT_COPY_SEMANTIC(DListIterator);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(DListIterator);

    DListIterator &operator++()
    {
        ASSERT(node_);
        node_ = node_->next;
        return *this;
    }

    // NOLINTNEXTLINE(cert-dcl21-cpp)
    DListIterator operator++(int)
    {
        ASSERT(node_);
        auto ret = *this;
        ++(*this);
        return ret;
    }

    T &operator*() const
    {
        ASSERT(node_);
        return *node_;
    }

    T *operator->() const
    {
        ASSERT(node_);
        return node_;
    }

    bool operator==(DListIterator other) const
    {
        return node_ == other.node_;
    }

    bool operator!=(DListIterator other) const
    {
        return !(*this == other);
    }

private:
    T *node_ = nullptr;
    friend class DList;
};

/**
 * Intrusive doubly list reverse iterator
 */
template <typename T>
class DListReverseIterator {
public:
    DListReverseIterator() = default;
    ~DListReverseIterator() = default;

    explicit DListReverseIterator(T *node) : node_(node) {}
    DEFAULT_COPY_SEMANTIC(DListReverseIterator);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(DListReverseIterator);

    DListIterator<T> base()
    {
        DListIterator<T> it(node_);
        return it;
    }

    DListReverseIterator &operator++()
    {
        ASSERT(node_);
        node_ = node_->prev;
        return *this;
    }

    // NOLINTNEXTLINE(cert-dcl21-cpp)
    DListReverseIterator operator++(int)
    {
        ASSERT(node_);
        auto ret = *this;
        ++(*this);
        return ret;
    }

    T &operator*() const
    {
        ASSERT(node_);
        return *node_;
    }

    T *operator->() const
    {
        ASSERT(node_);
        return node_;
    }

    bool operator==(DListReverseIterator other) const
    {
        return node_ == other.node_;
    }

    bool operator!=(DListReverseIterator other) const
    {
        return !(*this == other);
    }

private:
    T *node_ = nullptr;
    friend class DList;
};

/**
 * Intrusive doubly list
 */
class DList {
public:
    using Iterator = DListIterator<DListNode>;
    using ConstIterator = DListIterator<const DListNode>;
    using ReverseIterator = DListReverseIterator<DListNode>;
    using ConstReverseIterator = DListReverseIterator<const DListNode>;

    DList()
    {
        clear();
    }

    ~DList() = default;

    NO_COPY_SEMANTIC(DList);
    NO_MOVE_SEMANTIC(DList);

    size_t size() const
    {
        return size_;
    }

    bool empty() const
    {
        return size_ == 0;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator begin()
    {
        return Iterator(head_.next);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator begin() const
    {
        return ConstIterator(head_.next);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ReverseIterator rbegin()
    {
        return ReverseIterator(head_.prev);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator rbegin() const
    {
        return ConstReverseIterator(head_.prev);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator end()
    {
        return Iterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator end() const
    {
        return ConstIterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ReverseIterator rend()
    {
        return ReverseIterator(&head_);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator rend() const
    {
        return ConstReverseIterator(&head_);
    }

    Iterator insert(Iterator position, DListNode *new_node)
    {
        ++size_;
        new_node->next = position.node_;
        new_node->prev = position.node_->prev;
        position.node_->prev->next = new_node;
        position.node_->prev = new_node;
        return Iterator(new_node);
    }

    Iterator push_back(DListNode *new_node)
    {
        return insert(end(), new_node);
    }

    Iterator erase(DListNode *node)
    {
        ASSERT(size_ > 0);
        --size_;
        node->next->prev = node->prev;
        node->prev->next = node->next;
        return Iterator(node->next);
    }

    Iterator erase(Iterator position)
    {
        return erase(position.node_);
    }

    void clear()
    {
        head_.prev = &head_;
        head_.next = &head_;
        size_ = 0;
    }

    template <typename Predicate>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void remove_if(Predicate pred)
    {
        Iterator it = begin();
        while (it != end()) {
            if (pred(&(*it))) {
                it = erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    DListNode head_;
    size_t size_ = 0;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_LIST_H_
