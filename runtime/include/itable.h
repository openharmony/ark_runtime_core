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

#ifndef PANDA_RUNTIME_INCLUDE_ITABLE_H_
#define PANDA_RUNTIME_INCLUDE_ITABLE_H_

#include "libpandabase/utils/span.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/method.h"

namespace panda {

class Class;

class ITable {
public:
    class Entry {
    public:
        void SetInterface(Class *interface)
        {
            interface_ = interface;
        }

        Class *GetInterface() const
        {
            return interface_;
        }

        void SetMethods(Span<Method *> methods)
        {
            methods_ = methods;
        }

        Span<Method *> GetMethods() const
        {
            return methods_;
        }

        Entry Copy(mem::InternalAllocatorPtr allocator) const
        {
            Entry entry;
            entry.interface_ = interface_;
            if (methods_.data() != nullptr) {
                entry.methods_ = {allocator->AllocArray<Method *>(methods_.size()), methods_.size()};
                for (size_t idx = 0; idx < methods_.size(); idx++) {
                    entry.methods_[idx] = methods_[idx];
                }
            }
            return entry;
        }

    private:
        Class *interface_ {nullptr};
        Span<Method *> methods_ {nullptr, nullptr};
    };

    ITable() = default;

    explicit ITable(Span<Entry> elements) : elements_(elements) {}

    Span<Entry> Get()
    {
        return elements_;
    }

    Span<const Entry> Get() const
    {
        return Span<const Entry>(elements_.data(), elements_.size());
    }

    size_t Size() const
    {
        return elements_.size();
    }

    Entry &operator[](size_t i)
    {
        return elements_[i];
    }

    const Entry &operator[](size_t i) const
    {
        return elements_[i];
    }

    ~ITable() = default;

    DEFAULT_COPY_SEMANTIC(ITable);
    DEFAULT_MOVE_SEMANTIC(ITable);

private:
    Span<Entry> elements_ {nullptr, nullptr};
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_ITABLE_H_
