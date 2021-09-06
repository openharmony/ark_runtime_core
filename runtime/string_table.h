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

#ifndef PANDA_RUNTIME_STRING_TABLE_H_
#define PANDA_RUNTIME_STRING_TABLE_H_

#include <cstdint>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mutex.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/language_context.h"
#include "runtime/include/mem/panda_containers.h"

namespace panda {

namespace mem::test {
class MultithreadedInternStringTableTest;
}  // namespace mem::test

class StringTable {
public:
    explicit StringTable(mem::InternalAllocatorPtr allocator) : internal_table_(allocator), table_(allocator) {}
    StringTable() = default;
    virtual ~StringTable() = default;
    virtual coretypes::String *GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length, LanguageContext ctx);
    virtual coretypes::String *GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                 LanguageContext ctx);
    coretypes::String *GetOrInternString(coretypes::String *string, LanguageContext ctx);

    coretypes::String *GetOrInternInternalString(const panda_file::File &pf, panda_file::File::EntityId id,
                                                 LanguageContext ctx);

    coretypes::String *GetInternalStringFast(const panda_file::File &pf, panda_file::File::EntityId id)
    {
        return internal_table_.GetStringFast(pf, id);
    }

    using StringVisitor = std::function<void(coretypes::String *)>;

    void VisitRoots(const StringVisitor &visitor, mem::VisitGCRootFlags flags = mem::VisitGCRootFlags::ACCESS_ROOT_ALL)
    {
        internal_table_.VisitRoots(visitor, flags);
    }

    virtual void Sweep(const GCObjectVisitor &gc_object_visitor);

    bool UpdateMoved();

    size_t Size();

protected:
    class Table {
    public:
        explicit Table(mem::InternalAllocatorPtr allocator) : table_(allocator->Adapter()) {}
        Table() = default;
        virtual ~Table() = default;

        virtual coretypes::String *GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length,
                                                     LanguageContext ctx);
        virtual coretypes::String *GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                     LanguageContext ctx);
        coretypes::String *GetOrInternString(coretypes::String *string, LanguageContext ctx);
        virtual void Sweep(const GCObjectVisitor &gc_object_visitor);

        bool UpdateMoved();

        size_t Size();

        coretypes::String *GetString(const uint8_t *utf8_data, uint32_t utf16_length, LanguageContext ctx);
        coretypes::String *GetString(const uint16_t *utf16_data, uint32_t utf16_length, LanguageContext ctx);
        coretypes::String *GetString(coretypes::String *string, LanguageContext ctx);

        coretypes::String *InternString(coretypes::String *string, LanguageContext ctx);
        void ForceInternString(coretypes::String *string, LanguageContext ctx);

    protected:
        // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
        PandaUnorderedMultiMap<uint32_t, coretypes::String *> table_ GUARDED_BY(table_lock_) {};
        os::memory::RWLock table_lock_;  // NOLINT(misc-non-private-member-variables-in-classes)
    private:
        NO_COPY_SEMANTIC(Table);
        NO_MOVE_SEMANTIC(Table);

        // Required to clear intern string in test
        friend class mem::test::MultithreadedInternStringTableTest;
    };

    class InternalTable : public Table {
    public:
        InternalTable() = default;
        explicit InternalTable(mem::InternalAllocatorPtr allocator)
            : Table(allocator), new_string_table_(allocator->Adapter())
        {
        }
        ~InternalTable() override = default;

        coretypes::String *GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length,
                                             LanguageContext ctx) override;

        coretypes::String *GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                             LanguageContext ctx) override;

        coretypes::String *GetOrInternString(const panda_file::File &pf, panda_file::File::EntityId id,
                                             LanguageContext ctx);

        coretypes::String *GetStringFast(const panda_file::File &pf, panda_file::File::EntityId id);

        void VisitRoots(const StringVisitor &visitor,
                        mem::VisitGCRootFlags flags = mem::VisitGCRootFlags::ACCESS_ROOT_ALL);

    protected:
        coretypes::String *InternStringNonMovable(coretypes::String *string, LanguageContext ctx);

    private:
        bool record_new_string_ {false};
        PandaVector<coretypes::String *> new_string_table_ GUARDED_BY(table_lock_) {};
        class EntityIdEqual {
        public:
            uint32_t operator()(const panda_file::File::EntityId &id) const
            {
                return id.GetOffset();
            }
        };
        PandaUnorderedMap<const panda_file::File *,
                          PandaUnorderedMap<panda_file::File::EntityId, coretypes::String *, EntityIdEqual>>
            maps_ GUARDED_BY(maps_lock_);

        os::memory::RWLock maps_lock_;

        NO_COPY_SEMANTIC(InternalTable);
        NO_MOVE_SEMANTIC(InternalTable);
    };

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    InternalTable internal_table_;  // Used for string in panda file.
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    Table table_;

private:
    NO_COPY_SEMANTIC(StringTable);
    NO_MOVE_SEMANTIC(StringTable);

    // Required to clear intern string in test
    friend class mem::test::MultithreadedInternStringTableTest;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_STRING_TABLE_H_
