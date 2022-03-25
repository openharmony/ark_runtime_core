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

#include "file_item_container.h"

#include "file_format_version.h"
#include "macros.h"

namespace panda::panda_file {

class ItemDeduper {
public:
    template <class T>
    T *Deduplicate(T *item)
    {
        static_assert(std::is_base_of_v<BaseItem, T>);

        ItemData item_data(item);
        auto it = items_.find(item_data);
        if (it == items_.cend()) {
            items_.insert(item_data);
            return item;
        }

        auto result = static_cast<T *>(it->GetItem());
        if (item != result) {
            item->SetNeedsEmit(false);
        }

        return result;
    }

    size_t GetUniqueCount() const
    {
        return items_.size();
    }

private:
    class ItemWriter : public Writer {
    public:
        ItemWriter(std::vector<uint8_t> *buf, size_t offset) : buf_(buf), offset_(offset) {}
        ~ItemWriter() override = default;

        NO_COPY_SEMANTIC(ItemWriter);
        NO_MOVE_SEMANTIC(ItemWriter);

        bool WriteByte(uint8_t byte) override
        {
            buf_->push_back(byte);
            ++offset_;
            return true;
        }

        bool WriteBytes(const std::vector<uint8_t> &bytes) override
        {
            buf_->insert(buf_->end(), bytes.cbegin(), bytes.cend());
            offset_ += bytes.size();
            return true;
        }

        size_t GetOffset() const override
        {
            return offset_;
        }

    private:
        std::vector<uint8_t> *buf_;
        size_t offset_;
    };

    class ItemData {
    public:
        explicit ItemData(BaseItem *item) : item_(item)
        {
            Initialize();
        }

        ~ItemData() = default;

        DEFAULT_COPY_SEMANTIC(ItemData);
        NO_MOVE_SEMANTIC(ItemData);

        BaseItem *GetItem() const
        {
            return item_;
        }

        uint32_t GetHash() const
        {
            ASSERT(IsInitialized());
            return hash_;
        }

        bool operator==(const ItemData &item_data) const noexcept
        {
            ASSERT(IsInitialized());
            return data_ == item_data.data_;
        }

    private:
        bool IsInitialized() const
        {
            return !data_.empty();
        }

        void Initialize()
        {
            ASSERT(item_->NeedsEmit());

            ItemWriter writer(&data_, item_->GetOffset());
            [[maybe_unused]] auto res = item_->Write(&writer);
            ASSERT(res);
            ASSERT(data_.size() == item_->GetSize());

            hash_ = GetHash32(data_.data(), data_.size());
        }

        BaseItem *item_;
        uint32_t hash_ {0};
        std::vector<uint8_t> data_;
    };

    struct ItemHash {
        size_t operator()(const ItemData &item_data) const noexcept
        {
            return item_data.GetHash();
        }
    };

    std::unordered_set<ItemData, ItemHash> items_;
};

template <class T, class C, class I, class E, class... Args>
static T *GetOrInsert(C &map, I &items, const E &key, bool is_foreign, Args &&... args)
{
    auto it = map.find(key);
    if (it != map.cend()) {
        auto *item = it->second;
        if (item->IsForeign() == is_foreign) {
            return static_cast<T *>(item);
        }

        UNREACHABLE();
        return nullptr;
    }

    items.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    auto *item = static_cast<T *>(items.back().get());

    [[maybe_unused]] auto res = map.insert({key, item});
    ASSERT(res.second);
    return item;
}

ClassItem *ItemContainer::GetOrCreateClassItem(const std::string &str)
{
    return GetOrInsert<ClassItem>(class_map_, items_, str, false, str);
}

ForeignClassItem *ItemContainer::GetOrCreateForeignClassItem(const std::string &str)
{
    return GetOrInsert<ForeignClassItem>(class_map_, foreign_items_, str, true, str);
}

StringItem *ItemContainer::GetOrCreateStringItem(const std::string &str)
{
    auto it = class_map_.find(str);
    if (it != class_map_.cend()) {
        return it->second->GetNameItem();
    }

    return GetOrInsert<StringItem>(string_map_, items_, str, false, str);
}

LiteralArrayItem *ItemContainer::GetOrCreateLiteralArrayItem(const std::string &id)
{
    return GetOrInsert<LiteralArrayItem>(literalarray_map_, items_, id, false);
}

ScalarValueItem *ItemContainer::GetOrCreateIntegerValueItem(uint32_t v)
{
    return GetOrInsert<ScalarValueItem>(int_value_map_, items_, v, false, v);
}

ScalarValueItem *ItemContainer::GetOrCreateLongValueItem(uint64_t v)
{
    return GetOrInsert<ScalarValueItem>(long_value_map_, items_, v, false, v);
}

ScalarValueItem *ItemContainer::GetOrCreateFloatValueItem(float v)
{
    return GetOrInsert<ScalarValueItem>(float_value_map_, items_, bit_cast<uint32_t>(v), false, v);
}

ScalarValueItem *ItemContainer::GetOrCreateDoubleValueItem(double v)
{
    return GetOrInsert<ScalarValueItem>(double_value_map_, items_, bit_cast<uint64_t>(v), false, v);
}

ScalarValueItem *ItemContainer::GetOrCreateIdValueItem(BaseItem *v)
{
    return GetOrInsert<ScalarValueItem>(id_value_map_, items_, v, false, v);
}

ProtoItem *ItemContainer::GetOrCreateProtoItem(TypeItem *ret_type, const std::vector<MethodParamItem> &params)
{
    ProtoKey key(ret_type, params);
    return GetOrInsert<ProtoItem>(proto_map_, items_, key, false, ret_type, params);
}

LineNumberProgramItem *ItemContainer::CreateLineNumberProgramItem()
{
    items_.push_back(std::make_unique<LineNumberProgramItem>());
    auto *item = static_cast<LineNumberProgramItem *>(items_.back().get());
    [[maybe_unused]] auto res = line_number_program_index_item_.Add(item);
    ASSERT(res);
    return item;
}

void ItemContainer::DeduplicateLineNumberProgram(DebugInfoItem *item, ItemDeduper *deduper)
{
    auto *line_number_program = item->GetLineNumberProgram();
    auto *deduplicated = deduper->Deduplicate(line_number_program);
    if (deduplicated != line_number_program) {
        item->SetLineNumberProgram(deduplicated);
        line_number_program_index_item_.Remove(line_number_program);
        line_number_program_index_item_.IncRefCount(deduplicated);
    }
}

void ItemContainer::DeduplicateDebugInfo(MethodItem *method, ItemDeduper *debug_info_deduper,
                                         ItemDeduper *line_number_program_deduper)
{
    auto *debug_item = method->GetDebugInfo();
    if (debug_item == nullptr) {
        return;
    }

    DeduplicateLineNumberProgram(debug_item, line_number_program_deduper);

    auto *deduplicated = debug_info_deduper->Deduplicate(debug_item);
    if (deduplicated != debug_item) {
        method->SetDebugInfo(deduplicated);
        line_number_program_index_item_.DecRefCount(debug_item->GetLineNumberProgram());
    }
}

static void DeduplicateCode(MethodItem *method, ItemDeduper *code_deduper)
{
    auto *code_item = method->GetCode();
    if (code_item == nullptr) {
        return;
    }

    auto *deduplicated = code_deduper->Deduplicate(code_item);
    if (deduplicated != code_item) {
        method->SetCode(deduplicated);
    }
}

void ItemContainer::DeduplicateCodeAndDebugInfo()
{
    ItemDeduper line_number_program_deduper;
    ItemDeduper debug_deduper;
    ItemDeduper code_deduper;

    for (auto &p : class_map_) {
        auto *item = p.second;
        if (item->IsForeign()) {
            continue;
        }

        auto *class_item = static_cast<ClassItem *>(item);

        class_item->VisitMethods(
            [this, &debug_deduper, &line_number_program_deduper, &code_deduper](BaseItem *param_item) {
                auto *method_item = static_cast<MethodItem *>(param_item);
                DeduplicateDebugInfo(method_item, &debug_deduper, &line_number_program_deduper);
                DeduplicateCode(method_item, &code_deduper);
                return true;
            });
    }
}

static void DeduplicateAnnotationValue(AnnotationItem *annotation_item, ItemDeduper *deduper)
{
    auto *elems = annotation_item->GetElements();
    const auto &tags = annotation_item->GetTags();

    for (size_t i = 0; i < elems->size(); i++) {
        auto tag = tags[i];

        // Try to dedupe only ArrayValueItems
        switch (tag.GetItem()) {
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case '@':
                break;
            default:
                continue;
        }

        auto &elem = (*elems)[i];
        auto *value = elem.GetValue();
        auto *deduplicated = deduper->Deduplicate(value);
        if (deduplicated != value) {
            elem.SetValue(deduplicated);
        }
    }
}

static void DeduplicateAnnotations(std::vector<AnnotationItem *> *items, ItemDeduper *annotation_deduper,
                                   ItemDeduper *value_deduper)
{
    for (auto &item : *items) {
        DeduplicateAnnotationValue(item, value_deduper);
        auto *deduplicated = annotation_deduper->Deduplicate(item);
        if (deduplicated != item) {
            item = deduplicated;
        }
    }
}

void ItemContainer::DeduplicateAnnotations()
{
    ItemDeduper value_deduper;
    ItemDeduper annotation_deduper;

    for (auto &p : class_map_) {
        auto *item = p.second;
        if (item->IsForeign()) {
            continue;
        }

        auto *class_item = static_cast<ClassItem *>(item);

        panda_file::DeduplicateAnnotations(class_item->GetRuntimeAnnotations(), &annotation_deduper, &value_deduper);
        panda_file::DeduplicateAnnotations(class_item->GetAnnotations(), &annotation_deduper, &value_deduper);
        panda_file::DeduplicateAnnotations(class_item->GetRuntimeTypeAnnotations(), &annotation_deduper,
                                           &value_deduper);
        panda_file::DeduplicateAnnotations(class_item->GetTypeAnnotations(), &annotation_deduper, &value_deduper);

        class_item->VisitMethods([&annotation_deduper, &value_deduper](BaseItem *param_item) {
            auto *method_item = static_cast<MethodItem *>(param_item);
            panda_file::DeduplicateAnnotations(method_item->GetRuntimeAnnotations(), &annotation_deduper,
                                               &value_deduper);
            panda_file::DeduplicateAnnotations(method_item->GetAnnotations(), &annotation_deduper, &value_deduper);
            panda_file::DeduplicateAnnotations(method_item->GetRuntimeTypeAnnotations(), &annotation_deduper,
                                               &value_deduper);
            panda_file::DeduplicateAnnotations(method_item->GetTypeAnnotations(), &annotation_deduper, &value_deduper);
            return true;
        });

        class_item->VisitFields([&annotation_deduper, &value_deduper](BaseItem *param_item) {
            auto *field_item = static_cast<FieldItem *>(param_item);
            panda_file::DeduplicateAnnotations(field_item->GetRuntimeAnnotations(), &annotation_deduper,
                                               &value_deduper);
            panda_file::DeduplicateAnnotations(field_item->GetAnnotations(), &annotation_deduper, &value_deduper);
            panda_file::DeduplicateAnnotations(field_item->GetRuntimeTypeAnnotations(), &annotation_deduper,
                                               &value_deduper);
            panda_file::DeduplicateAnnotations(field_item->GetTypeAnnotations(), &annotation_deduper, &value_deduper);
            return true;
        });
    }
}

void ItemContainer::DeduplicateItems()
{
    ComputeLayout();
    DeduplicateCodeAndDebugInfo();
    DeduplicateAnnotations();
}

uint32_t ItemContainer::ComputeLayout()
{
    uint32_t num_classes = class_map_.size();
    uint32_t num_literalarrays = literalarray_map_.size();
    uint32_t class_idx_offset = sizeof(File::Header);
    uint32_t cur_offset = class_idx_offset + (num_classes + num_literalarrays) * ID_SIZE;

    UpdateOrderIndexes();

    RebuildIndexSection();
    RebuildLineNumberProgramIndex();

    index_section_item_.SetOffset(cur_offset);
    index_section_item_.ComputeLayout();
    cur_offset += index_section_item_.GetSize();

    for (auto &item : foreign_items_) {
        cur_offset = RoundUp(cur_offset, item->Alignment());
        item->SetOffset(cur_offset);
        item->ComputeLayout();
        cur_offset += item->GetSize();
    }

    for (auto &item : items_) {
        if (!item->NeedsEmit()) {
            continue;
        }

        cur_offset = RoundUp(cur_offset, item->Alignment());
        item->SetOffset(cur_offset);
        item->ComputeLayout();
        cur_offset += item->GetSize();
    }

    // Line number program should be last because it's size is known only after deduplication
    cur_offset = RoundUp(cur_offset, line_number_program_index_item_.Alignment());
    line_number_program_index_item_.SetOffset(cur_offset);
    line_number_program_index_item_.ComputeLayout();
    cur_offset += line_number_program_index_item_.GetSize();

    end_.SetOffset(cur_offset);

    return cur_offset;
}

void ItemContainer::RebuildLineNumberProgramIndex()
{
    line_number_program_index_item_.Reset();
    line_number_program_index_item_.UpdateItems(nullptr, nullptr);
}

void ItemContainer::RebuildIndexSection()
{
    index_section_item_.Reset();

    for (auto &item : foreign_items_) {
        ProcessIndexDependecies(item.get());
    }

    for (auto &item : items_) {
        if (!item->NeedsEmit()) {
            continue;
        }

        ProcessIndexDependecies(item.get());
    }

    if (!index_section_item_.IsEmpty()) {
        index_section_item_.GetCurrentHeader()->SetEnd(&end_);
    }

    index_section_item_.UpdateItems();
}

void ItemContainer::UpdateOrderIndexes()
{
    size_t idx = 0;

    for (auto &item : foreign_items_) {
        item->SetOrderIndex(idx++);
        item->Visit([&idx](BaseItem *param_item) {
            param_item->SetOrderIndex(idx++);
            return true;
        });
    }

    for (auto &item : items_) {
        if (!item->NeedsEmit()) {
            continue;
        }

        item->SetOrderIndex(idx++);
        item->Visit([&idx](BaseItem *param_item) {
            param_item->SetOrderIndex(idx++);
            return true;
        });
    }

    end_.SetOrderIndex(idx++);
}

void ItemContainer::ProcessIndexDependecies(BaseItem *item)
{
    auto deps = item->GetIndexDependencies();

    item->Visit([&deps](BaseItem *param_item) {
        const auto &item_deps = param_item->GetIndexDependencies();
        deps.insert(deps.end(), item_deps.cbegin(), item_deps.cend());
        return true;
    });

    if (index_section_item_.IsEmpty()) {
        index_section_item_.AddHeader();
        index_section_item_.GetCurrentHeader()->SetStart(item);
    }

    if (index_section_item_.GetCurrentHeader()->Add(deps)) {
        return;
    }

    index_section_item_.GetCurrentHeader()->SetEnd(item);
    index_section_item_.AddHeader();
    index_section_item_.GetCurrentHeader()->SetStart(item);

    if (!index_section_item_.GetCurrentHeader()->Add(deps)) {
        LOG(FATAL, PANDAFILE) << "Cannot add " << deps.size() << " items to index";
    }
}

bool ItemContainer::WriteHeaderIndexInfo(Writer *writer)
{
    if (!writer->Write<uint32_t>(class_map_.size())) {
        return false;
    }

    if (!writer->Write<uint32_t>(sizeof(File::Header))) {
        return false;
    }

    if (!writer->Write<uint32_t>(line_number_program_index_item_.GetNumItems())) {
        return false;
    }

    if (!writer->Write<uint32_t>(line_number_program_index_item_.GetOffset())) {
        return false;
    }

    if (!writer->Write<uint32_t>(literalarray_map_.size())) {
        return false;
    }

    uint32_t literalarray_idx_offset = sizeof(File::Header) + class_map_.size() * ID_SIZE;
    if (!writer->Write<uint32_t>(literalarray_idx_offset)) {
        return false;
    }

    if (!writer->Write<uint32_t>(index_section_item_.GetNumHeaders())) {
        return false;
    }

    size_t index_section_off = literalarray_idx_offset + literalarray_map_.size() * ID_SIZE;
    return writer->Write<uint32_t>(index_section_off);
}

bool ItemContainer::WriteHeader(Writer *writer, ssize_t *checksum_offset)
{
    uint32_t file_size = ComputeLayout();

    std::vector<uint8_t> magic;
    magic.assign(File::MAGIC.cbegin(), File::MAGIC.cend());
    if (!writer->WriteBytes(magic)) {
        return false;
    }

    *checksum_offset = writer->GetOffset();
    uint32_t checksum = 0;
    if (!writer->Write(checksum)) {
        return false;
    }
    writer->CountChecksum(true);

    std::vector<uint8_t> versionVec(std::begin(version), std::end(version));
    if (!writer->WriteBytes(versionVec)) {
        return false;
    }

    if (!writer->Write(file_size)) {
        return false;
    }

    uint32_t foreign_offset = GetForeignOffset();
    if (!writer->Write(foreign_offset)) {
        return false;
    }

    uint32_t foreign_size = GetForeignSize();
    if (!writer->Write(foreign_size)) {
        return false;
    }

    return WriteHeaderIndexInfo(writer);
}

bool ItemContainer::Write(Writer *writer)
{
    DeduplicateItems();

    ssize_t checksum_offset = -1;
    if (!WriteHeader(writer, &checksum_offset)) {
        return false;
    }
    ASSERT(checksum_offset != -1);

    // Write class idx

    for (auto &entry : class_map_) {
        if (!writer->Write(entry.second->GetOffset())) {
            return false;
        }
    }

    // Write literalArray idx

    for (auto &entry : literalarray_map_) {
        if (!writer->Write(entry.second->GetOffset())) {
            return false;
        }
    }

    // Write index section

    if (!index_section_item_.Write(writer)) {
        return false;
    }

    for (auto &item : foreign_items_) {
        if (!writer->Align(item->Alignment())) {
            return false;
        }

        if (!item->Write(writer)) {
            return false;
        }
    }

    for (auto &item : items_) {
        if (!item->NeedsEmit()) {
            continue;
        }

        if (!writer->Align(item->Alignment())) {
            return false;
        }

        if (!item->Write(writer)) {
            return false;
        }
    }

    if (!writer->Align(line_number_program_index_item_.Alignment())) {
        return false;
    }

    // Write line number program idx

    if (!line_number_program_index_item_.Write(writer)) {
        return false;
    }

    writer->CountChecksum(false);
    writer->WriteChecksum(checksum_offset);

    return true;
}

std::map<std::string, size_t> ItemContainer::GetStat()
{
    std::map<std::string, size_t> stat;

    DeduplicateItems();
    ComputeLayout();

    stat["header_item"] = sizeof(File::Header);
    stat["class_idx_item"] = class_map_.size() * ID_SIZE;
    stat["line_number_program_idx_item"] = line_number_program_index_item_.GetNumItems() * ID_SIZE;
    stat["literalarray_idx"] = literalarray_map_.size() * ID_SIZE;

    stat["index_section_item"] = index_section_item_.GetSize();
    stat["foreign_item"] = GetForeignSize();

    size_t num_ins = 0;
    size_t codesize = 0;
    for (auto &item : items_) {
        if (!item->NeedsEmit()) {
            continue;
        }

        const auto &name = item->GetName();
        size_t size = item->GetSize();
        auto it = stat.find(name);
        if (it != stat.cend()) {
            stat[name] += size;
        } else if (size != 0) {
            stat[name] = size;
        }
        if (name == "code_item") {
            num_ins += static_cast<CodeItem *>(item.get())->GetNumInstructions();
            codesize += static_cast<CodeItem *>(item.get())->GetCodeSize();
        }
    }
    stat["instructions_number"] = num_ins;
    stat["codesize"] = codesize;

    return stat;
}

void ItemContainer::DumpItemsStat(std::ostream &os) const
{
    struct Stat {
        size_t n;
        size_t total_size;
    };

    std::map<std::string, Stat> stat;

    auto collect_stat = [&stat](const std::vector<std::unique_ptr<BaseItem>> &items) {
        for (auto &item : items) {
            if (!item->NeedsEmit()) {
                continue;
            }

            const auto &name = item->GetName();
            size_t size = item->GetSize();
            auto it = stat.find(name);
            if (it != stat.cend()) {
                stat[name].n += 1;
                stat[name].total_size += size;
            } else if (size != 0) {
                stat[name] = {1, size};
            }
        }
    };

    collect_stat(foreign_items_);
    collect_stat(items_);

    for (auto &[name, elem] : stat) {
        os << name << ":" << std::endl;
        os << "    n          = " << elem.n << std::endl;
        os << "    total size = " << elem.total_size << std::endl;
    }
}

size_t ItemContainer::GetForeignOffset() const
{
    if (foreign_items_.empty()) {
        return 0;
    }

    return foreign_items_.front()->GetOffset();
}

size_t ItemContainer::GetForeignSize() const
{
    if (foreign_items_.empty()) {
        return 0;
    }

    size_t begin = foreign_items_.front()->GetOffset();
    size_t end = foreign_items_.back()->GetOffset() + foreign_items_.back()->GetSize();

    return end - begin;
}

bool ItemContainer::IndexHeaderItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    ASSERT(start_ != nullptr);
    ASSERT(start_->GetOffset() != 0);
    ASSERT(end_ != nullptr);
    ASSERT(end_->GetOffset() != 0);

    if (!writer->Write<uint32_t>(start_->GetOffset())) {
        return false;
    }

    if (!writer->Write<uint32_t>(end_->GetOffset())) {
        return false;
    }

    for (auto *index_item : indexes_) {
        if (!writer->Write<uint32_t>(index_item->GetNumItems())) {
            return false;
        }

        ASSERT(index_item->GetOffset() != 0);
        if (!writer->Write<uint32_t>(index_item->GetOffset())) {
            return false;
        }
    }

    return true;
}

bool ItemContainer::IndexHeaderItem::Add(const std::list<IndexedItem *> &items)
{
    std::list<IndexedItem *> added_items;

    for (auto *item : items) {
        auto type = item->GetIndexType();
        ASSERT(type != IndexType::NONE);

        auto *index_item = IndexGetIndexByType(type);

        if (index_item->Has(item)) {
            continue;
        }

        if (!index_item->Add(item)) {
            Remove(added_items);
            return false;
        }

        added_items.push_back(item);
    }

    return true;
}

void ItemContainer::IndexHeaderItem::Remove(const std::list<IndexedItem *> &items)
{
    for (auto *item : items) {
        auto type = item->GetIndexType();
        ASSERT(type != IndexType::NONE);

        auto *index_item = IndexGetIndexByType(type);
        index_item->Remove(item);
    }
}

bool ItemContainer::IndexItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    for (auto *item : index_) {
        if (!writer->Write<uint32_t>(item->GetOffset())) {
            return false;
        }
    }

    return true;
}

std::string ItemContainer::IndexItem::GetName() const
{
    switch (type_) {
        case IndexType::CLASS:
            return "class_index_item";
        case IndexType::METHOD:
            return "method_index_item";
        case IndexType::FIELD:
            return "field_index_item";
        case IndexType::PROTO:
            return "proto_index_item";
        case IndexType::LINE_NUMBER_PROG:
            return "line_number_program_index_item";
        default:
            break;
    }

    UNREACHABLE();
}

bool ItemContainer::IndexItem::Add(IndexedItem *item)
{
    auto size = index_.size();
    ASSERT(size <= max_index_);

    if (size == max_index_) {
        return false;
    }

    auto res = index_.insert(item);
    ASSERT(res.second);

    return res.second;
}

void ItemContainer::IndexSectionItem::AddHeader()
{
    std::vector<IndexItem *> index_items;
    for (size_t i = 0; i < INDEX_COUNT_16; i++) {
        auto type = static_cast<IndexType>(i);
        indexes_.emplace_back(type, MAX_INDEX_16);
        index_items.push_back(&indexes_.back());
    }
    headers_.emplace_back(index_items);
}

size_t ItemContainer::IndexSectionItem::CalculateSize() const
{
    size_t size = headers_.size() * sizeof(File::IndexHeader);
    for (auto &index_item : indexes_) {
        size += index_item.GetSize();
    }
    return size;
}

void ItemContainer::IndexSectionItem::ComputeLayout()
{
    size_t offset = GetOffset();

    for (auto &header : headers_) {
        header.SetOffset(offset);
        header.ComputeLayout();
        offset += header.GetSize();
    }

    for (auto &index : indexes_) {
        index.SetOffset(offset);
        index.ComputeLayout();
        offset += index.GetSize();
    }
}

bool ItemContainer::IndexSectionItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    for (auto &header : headers_) {
        if (!header.Write(writer)) {
            return false;
        }
    }

    for (auto &index : indexes_) {
        if (!index.Write(writer)) {
            return false;
        }
    }

    return true;
}

ItemContainer::ProtoKey::ProtoKey(TypeItem *ret_type, const std::vector<MethodParamItem> &params)
{
    Add(ret_type);
    for (const auto &param : params) {
        Add(param.GetType());
    }
    size_t shorty_hash = std::hash<std::string>()(shorty_);
    size_t ret_type_hash = std::hash<TypeItem *>()(ret_type);
    // combine hashes of shorty and ref_types
    hash_ = panda::merge_hashes(shorty_hash, ret_type_hash);
    // combine hashes of all param types
    for (const auto &item : params) {
        size_t param_type_hash = std::hash<TypeItem *>()(item.GetType());
        hash_ = panda::merge_hashes(hash_, param_type_hash);
    }
}

void ItemContainer::ProtoKey::Add(TypeItem *item)
{
    auto type = item->GetType();
    shorty_.append(Type::GetSignatureByTypeId(type));
    if (type.IsReference()) {
        ref_types_.push_back(item);
    }
}

}  // namespace panda::panda_file
