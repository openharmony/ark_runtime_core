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

#ifndef PANDA_RUNTIME_INCLUDE_MEM_PANDA_CONTAINERS_H_
#define PANDA_RUNTIME_INCLUDE_MEM_PANDA_CONTAINERS_H_

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "runtime/mem/allocator_adapter.h"

namespace panda {

template <class T>
using PandaForwardList = std::forward_list<T, mem::AllocatorAdapter<T>>;
// Thread local version of PandaForwardList
template <class T>
using PandaForwardListTL = std::forward_list<T, mem::AllocatorAdapter<T, mem::AllocScope::LOCAL>>;

template <class T>
using PandaList = std::list<T, mem::AllocatorAdapter<T>>;
// Thread local version of PandaList
template <class T>
using PandaListTL = std::list<T, mem::AllocatorAdapter<T, mem::AllocScope::LOCAL>>;

template <class T>
using PandaDeque = std::deque<T, mem::AllocatorAdapter<T>>;
// Thread local version of PandaDeque
template <class T>
using PandaDequeTL = std::deque<T, mem::AllocatorAdapter<T, mem::AllocScope::LOCAL>>;

template <class T, class PandaContainer = PandaDeque<T>>
using PandaQueue = std::queue<T, PandaContainer>;
// Thread local version of PandaQueue
template <class T, class PandaContainer = PandaDequeTL<T>>
using PandaQueueTL = std::queue<T, PandaContainer>;

template <class T, class PandaContainer = PandaDeque<T>>
using PandaStack = std::stack<T, PandaContainer>;
// Thread local version of PandaStack
template <class T, class PandaContainer = PandaDequeTL<T>>
using PandaStackTL = std::stack<T, PandaContainer>;

template <class Key, class KeyLess = std::less<Key>>
using PandaSet = std::set<Key, KeyLess, mem::AllocatorAdapter<Key>>;
// Thread local version of PandaSet
template <class Key, class KeyLess = std::less<Key>>
using PandaSetTL = std::set<Key, KeyLess, mem::AllocatorAdapter<Key, mem::AllocScope::LOCAL>>;

template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedSet = std::unordered_set<Key, Hash, KeyEqual, mem::AllocatorAdapter<Key>>;
// Thread local version of PandaUnorderedSet
template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedSetTL = std::unordered_set<Key, Hash, KeyEqual, mem::AllocatorAdapter<Key, mem::AllocScope::LOCAL>>;

template <class T>
using PandaVector = std::vector<T, mem::AllocatorAdapter<T>>;
// Thread local version of PandaVector
template <class T>
using PandaVectorTL = std::vector<T, mem::AllocatorAdapter<T, mem::AllocScope::LOCAL>>;

template <class T, class Container = PandaVector<T>, class Compare = std::less<typename Container::value_type>>
using PandaPriorityQueue = std::priority_queue<T, Container, Compare>;
// Thread local version of PandaPriorityQueue
template <class T, class Container = PandaVectorTL<T>, class Compare = std::less<typename Container::value_type>>
using PandaPriorityQueueTL = std::priority_queue<T, Container, Compare>;

template <class Key, class T, class Compare = std::less<>>
using PandaMap = std::map<Key, T, Compare, mem::AllocatorAdapter<std::pair<const Key, T>>>;
// Thread local version of PandaMap
template <class Key, class T, class Compare = std::less<>>
using PandaMapTL = std::map<Key, T, Compare, mem::AllocatorAdapter<std::pair<const Key, T>, mem::AllocScope::LOCAL>>;

template <class Key, class T, class Compare = std::less<>>
using PandaMultiMap = std::multimap<Key, T, Compare, mem::AllocatorAdapter<std::pair<const Key, T>>>;
// Thread local version of PandaMultiMap
template <class Key, class T, class Compare = std::less<>>
using PandaMultiMapTL =
    std::multimap<Key, T, Compare, mem::AllocatorAdapter<std::pair<const Key, T>, mem::AllocScope::LOCAL>>;

template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedMap = std::unordered_map<Key, T, Hash, KeyEqual, mem::AllocatorAdapter<std::pair<const Key, T>>>;
// Thread local version of PandaUnorderedMap
template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedMapTL =
    std::unordered_map<Key, T, Hash, KeyEqual, mem::AllocatorAdapter<std::pair<const Key, T>, mem::AllocScope::LOCAL>>;

template <class Key, class Value, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedMultiMap =
    std::unordered_multimap<Key, Value, Hash, KeyEqual, mem::AllocatorAdapter<std::pair<const Key, Value>>>;
// Thread local version of PandaUnorderedMultiMap
template <class Key, class Value, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using PandaUnorderedMultiMapTL =
    std::unordered_multimap<Key, Value, Hash, KeyEqual,
                            mem::AllocatorAdapter<std::pair<const Key, Value>, mem::AllocScope::LOCAL>>;
}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_MEM_PANDA_CONTAINERS_H_
