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

#include "runtime/string_table.h"

#include "runtime/include/runtime.h"
#include "runtime/mem/object_helpers.h"

namespace panda {

coretypes::String *StringTable::GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length, LanguageContext ctx)
{
    bool can_be_compressed = coretypes::String::CanBeCompressedMUtf8(mutf8_data);
    auto *str = internal_table_.GetString(mutf8_data, utf16_length, can_be_compressed, ctx);
    if (str == nullptr) {
        str = table_.GetOrInternString(mutf8_data, utf16_length, can_be_compressed, ctx);
    }
    return str;
}

coretypes::String *StringTable::GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                  LanguageContext ctx)
{
    auto *str = internal_table_.GetString(utf16_data, utf16_length, ctx);
    if (str == nullptr) {
        str = table_.GetOrInternString(utf16_data, utf16_length, ctx);
    }
    return str;
}

coretypes::String *StringTable::GetOrInternString(coretypes::String *string, LanguageContext ctx)
{
    auto *str = internal_table_.GetString(string, ctx);
    if (str == nullptr) {
        str = table_.GetOrInternString(string, ctx);
    }
    return str;
}

coretypes::String *StringTable::GetOrInternInternalString(const panda_file::File &pf, panda_file::File::EntityId id,
                                                          LanguageContext ctx)
{
    auto data = pf.GetStringData(id);
    coretypes::String *str = table_.GetString(data.data, data.utf16_length, data.is_ascii, ctx);
    if (str != nullptr) {
        return str;
    }
    return internal_table_.GetOrInternString(pf, id, ctx);
}

void StringTable::Sweep(const GCObjectVisitor &gc_object_visitor)
{
    table_.Sweep(gc_object_visitor);
}

bool StringTable::UpdateMoved()
{
    return table_.UpdateMoved();
}

size_t StringTable::Size()
{
    return internal_table_.Size() + table_.Size();
}

coretypes::String *StringTable::Table::GetString(const uint8_t *utf8_data, uint32_t utf16_length,
                                                 bool can_be_compressed, [[maybe_unused]] LanguageContext ctx)
{
    uint32_t hash_code = coretypes::String::ComputeHashcodeMutf8(utf8_data, utf16_length, can_be_compressed);
    os::memory::ReadLockHolder holder(table_lock_);
    for (auto it = table_.find(hash_code); it != table_.end(); it++) {
        auto found_string = it->second;
        if (coretypes::String::StringsAreEqualMUtf8(found_string, utf8_data, utf16_length, can_be_compressed)) {
            return found_string;
        }
    }
    return nullptr;
}

coretypes::String *StringTable::Table::GetString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                 [[maybe_unused]] LanguageContext ctx)
{
    uint32_t hash_code = coretypes::String::ComputeHashcodeUtf16(const_cast<uint16_t *>(utf16_data), utf16_length);
    os::memory::ReadLockHolder holder(table_lock_);
    for (auto it = table_.find(hash_code); it != table_.end(); it++) {
        auto found_string = it->second;
        if (coretypes::String::StringsAreEqualUtf16(found_string, utf16_data, utf16_length)) {
            return found_string;
        }
    }
    return nullptr;
}

coretypes::String *StringTable::Table::GetString([[maybe_unused]] coretypes::String *string,
                                                 [[maybe_unused]] LanguageContext ctx)
{
    os::memory::ReadLockHolder holder(table_lock_);
    auto hash = string->GetHashcode();
    for (auto it = table_.find(hash); it != table_.end(); it++) {
        auto found_string = it->second;
        if (coretypes::String::StringsAreEqual(found_string, string)) {
            return found_string;
        }
    }
    return nullptr;
}

void StringTable::Table::ForceInternString(coretypes::String *string, [[maybe_unused]] LanguageContext ctx)
{
    os::memory::WriteLockHolder holder(table_lock_);
    table_.insert(std::pair<uint32_t, coretypes::String *>(string->GetHashcode(), string));
}

coretypes::String *StringTable::Table::InternString(coretypes::String *string, [[maybe_unused]] LanguageContext ctx)
{
    uint32_t hash_code = string->GetHashcode();
    os::memory::WriteLockHolder holder(table_lock_);
    // Check string is not present before actually creating and inserting
    for (auto it = table_.find(hash_code); it != table_.end(); it++) {
        auto found_string = it->second;
        if (coretypes::String::StringsAreEqual(found_string, string)) {
            return found_string;
        }
    }
    table_.insert(std::pair<uint32_t, coretypes::String *>(hash_code, string));
    return string;
}

coretypes::String *StringTable::Table::GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length,
                                                         bool can_be_compressed, LanguageContext ctx)
{
    coretypes::String *result = GetString(mutf8_data, utf16_length, can_be_compressed, ctx);
    if (result != nullptr) {
        return result;
    }

    // Even if this string is not inserted, it should get removed during GC
    result = coretypes::String::CreateFromMUtf8(mutf8_data, utf16_length, can_be_compressed, ctx,
                                                Runtime::GetCurrent()->GetPandaVM());

    result = InternString(result, ctx);

    return result;
}

coretypes::String *StringTable::Table::GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                         LanguageContext ctx)
{
    coretypes::String *result = GetString(utf16_data, utf16_length, ctx);
    if (result != nullptr) {
        return result;
    }

    // Even if this string is not inserted, it should get removed during GC
    result = coretypes::String::CreateFromUtf16(utf16_data, utf16_length, ctx, Runtime::GetCurrent()->GetPandaVM());

    result = InternString(result, ctx);

    return result;
}

coretypes::String *StringTable::Table::GetOrInternString(coretypes::String *string, LanguageContext ctx)
{
    coretypes::String *result = GetString(string, ctx);
    if (result != nullptr) {
        return result;
    }
    result = InternString(string, ctx);
    return result;
}

bool StringTable::Table::UpdateMoved()
{
    os::memory::WriteLockHolder holder(table_lock_);
    LOG(DEBUG, GC) << "=== StringTable Update moved. BEGIN ===";
    LOG(DEBUG, GC) << "Iterate over: " << table_.size() << " elements in string table";
    bool updated = false;
    for (auto it = table_.begin(), end = table_.end(); it != end;) {
        auto *object = it->second;
        if (object->IsForwarded()) {
            ObjectHeader *fwd_string = panda::mem::GetForwardAddress(object);
            it->second = static_cast<coretypes::String *>(fwd_string);
            LOG(DEBUG, GC) << "StringTable: forward " << std::hex << object << " -> " << fwd_string;
            updated = true;
        }
        ++it;
    }
    LOG(DEBUG, GC) << "=== StringTable Update moved. END ===";
    return updated;
}

void StringTable::Table::Sweep(const GCObjectVisitor &gc_object_visitor)
{
    os::memory::WriteLockHolder holder(table_lock_);
    LOG(DEBUG, GC) << "=== StringTable Sweep. BEGIN ===";
    LOG(DEBUG, GC) << "StringTable iterate over: " << table_.size() << " elements in string table";
    for (auto it = table_.begin(), end = table_.end(); it != end;) {
        auto *object = it->second;
        if (object->IsForwarded()) {
            ASSERT(gc_object_visitor(object) != ObjectStatus::DEAD_OBJECT);
            ObjectHeader *fwd_string = panda::mem::GetForwardAddress(object);
            it->second = static_cast<coretypes::String *>(fwd_string);
            ++it;
            LOG(DEBUG, GC) << "StringTable: forward " << std::hex << object << " -> " << fwd_string;
        } else if (gc_object_visitor(object) == ObjectStatus::DEAD_OBJECT) {
            LOG(DEBUG, GC) << "StringTable: delete string " << std::hex << object
                           << ", val = " << ConvertToString(object);
            table_.erase(it++);
        } else {
            ++it;
        }
    }
    LOG(DEBUG, GC) << "StringTable size after sweep = " << table_.size();
    LOG(DEBUG, GC) << "=== StringTable Sweep. END ===";
}

size_t StringTable::Table::Size()
{
    os::memory::ReadLockHolder holder(table_lock_);
    return table_.size();
}

coretypes::String *StringTable::InternalTable::GetOrInternString(const uint8_t *mutf8_data, uint32_t utf16_length,
                                                                 bool can_be_compressed, LanguageContext ctx)
{
    coretypes::String *result = GetString(mutf8_data, utf16_length, can_be_compressed, ctx);
    if (result != nullptr) {
        return result;
    }

    result = coretypes::String::CreateFromMUtf8(mutf8_data, utf16_length, can_be_compressed, ctx,
                                                Runtime::GetCurrent()->GetPandaVM(), false);
    return InternStringNonMovable(result, ctx);
}

coretypes::String *StringTable::InternalTable::GetOrInternString(const uint16_t *utf16_data, uint32_t utf16_length,
                                                                 LanguageContext ctx)
{
    coretypes::String *result = GetString(utf16_data, utf16_length, ctx);
    if (result != nullptr) {
        return result;
    }

    result =
        coretypes::String::CreateFromUtf16(utf16_data, utf16_length, ctx, Runtime::GetCurrent()->GetPandaVM(), false);
    return InternStringNonMovable(result, ctx);
}

coretypes::String *StringTable::InternalTable::GetOrInternString(const panda_file::File &pf,
                                                                 panda_file::File::EntityId id, LanguageContext ctx)
{
    auto data = pf.GetStringData(id);
    coretypes::String *result = GetString(data.data, data.utf16_length, data.is_ascii, ctx);
    if (result != nullptr) {
        return result;
    }
    result = coretypes::String::CreateFromMUtf8(data.data, data.utf16_length, data.is_ascii, ctx,
                                                Runtime::GetCurrent()->GetPandaVM(), false);
    result = InternStringNonMovable(result, ctx);

    // Update cache.
    os::memory::WriteLockHolder lock(maps_lock_);
    auto it = maps_.find(&pf);
    if (it != maps_.end()) {
        (it->second)[id] = result;
    } else {
        PandaUnorderedMap<panda_file::File::EntityId, coretypes::String *, EntityIdEqual> map;
        map[id] = result;
        maps_[&pf] = std::move(map);
    }
    return result;
}

coretypes::String *StringTable::InternalTable::GetStringFast(const panda_file::File &pf, panda_file::File::EntityId id)
{
    os::memory::ReadLockHolder lock(maps_lock_);
    auto it = maps_.find(&pf);
    if (it != maps_.end()) {
        auto id_it = it->second.find(id);
        if (id_it != it->second.end()) {
            return id_it->second;
        }
    }
    return nullptr;
}

void StringTable::InternalTable::VisitRoots(const StringVisitor &visitor, mem::VisitGCRootFlags flags)
{
    ASSERT(BitCount(flags & (mem::VisitGCRootFlags::ACCESS_ROOT_ALL | mem::VisitGCRootFlags::ACCESS_ROOT_ONLY_NEW)) ==
           1);

    ASSERT(BitCount(flags & (mem::VisitGCRootFlags::START_RECORDING_NEW_ROOT |
                             mem::VisitGCRootFlags::END_RECORDING_NEW_ROOT)) <= 1);
    // need to set flags before we iterate, cause concurrent allocation should be in proper table
    if ((flags & mem::VisitGCRootFlags::START_RECORDING_NEW_ROOT) != 0) {
        os::memory::WriteLockHolder holder(table_lock_);
        record_new_string_ = true;
    } else if ((flags & mem::VisitGCRootFlags::END_RECORDING_NEW_ROOT) != 0) {
        os::memory::WriteLockHolder holder(table_lock_);
        record_new_string_ = false;
    }

    if ((flags & mem::VisitGCRootFlags::ACCESS_ROOT_ALL) != 0) {
        os::memory::ReadLockHolder lock(table_lock_);
        for (const auto &v : table_) {
            visitor(v.second);
        }
    } else if ((flags & mem::VisitGCRootFlags::ACCESS_ROOT_ONLY_NEW) != 0) {
        os::memory::ReadLockHolder lock(table_lock_);
        for (const auto str : new_string_table_) {
            visitor(str);
        }
    } else {
        LOG(FATAL, RUNTIME) << "Unknown VisitGCRootFlags: " << static_cast<uint32_t>(flags);
    }
    if ((flags & mem::VisitGCRootFlags::END_RECORDING_NEW_ROOT) != 0) {
        os::memory::WriteLockHolder holder(table_lock_);
        new_string_table_.clear();
    }
}

coretypes::String *StringTable::InternalTable::InternStringNonMovable(coretypes::String *string, LanguageContext ctx)
{
    auto *result = InternString(string, ctx);
    os::memory::WriteLockHolder holder(table_lock_);
    if (record_new_string_) {
        new_string_table_.push_back(result);
    }
    return result;
}

}  // namespace panda
