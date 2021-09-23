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

#ifndef PANDA_LIBPANDAFILE_FILE_H_
#define PANDA_LIBPANDAFILE_FILE_H_

#include "os/mem.h"
#include "utils/span.h"
#include "utils/utf.h"

#include <cstdint>

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace panda::panda_file {

class PandaCache;

class File {
public:
    using Index = uint16_t;
    using Index32 = uint32_t;

    static constexpr size_t MAGIC_SIZE = 8;
    static constexpr size_t VERSION_SIZE = 4;
    static const std::array<uint8_t, MAGIC_SIZE> MAGIC;

    struct Header {
        std::array<uint8_t, MAGIC_SIZE> magic;
        uint32_t checksum;
        std::array<uint8_t, VERSION_SIZE> version;
        uint32_t file_size;
        uint32_t foreign_off;
        uint32_t foreign_size;
        uint32_t num_classes;
        uint32_t class_idx_off;
        uint32_t num_lnps;
        uint32_t lnp_idx_off;
        uint32_t num_literalarrays;
        uint32_t literalarray_idx_off;
        uint32_t num_indexes;
        uint32_t index_section_off;
    };

    struct IndexHeader {
        uint32_t start;
        uint32_t end;
        uint32_t class_idx_size;
        uint32_t class_idx_off;
        uint32_t method_idx_size;
        uint32_t method_idx_off;
        uint32_t field_idx_size;
        uint32_t field_idx_off;
        uint32_t proto_idx_size;
        uint32_t proto_idx_off;
    };

    struct StringData {
        StringData(uint32_t len, const uint8_t *d) : utf16_length(len), data(d), is_ascii(false) {}
        StringData() = default;
        uint32_t utf16_length;  // NOLINT(misc-non-private-member-variables-in-classes)
        const uint8_t *data;    // NOLINT(misc-non-private-member-variables-in-classes)
        bool is_ascii;          // NOLINT(misc-non-private-member-variables-in-classes)
    };

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
    class EntityId {
    public:
        explicit constexpr EntityId(uint32_t offset) : offset_(offset) {}

        EntityId() = default;

        ~EntityId() = default;

        bool IsValid() const
        {
            return offset_ > sizeof(Header);
        }

        uint32_t GetOffset() const
        {
            return offset_;
        }

        static constexpr size_t GetSize()
        {
            return sizeof(uint32_t);
        }

        friend bool operator==(const EntityId &l, const EntityId &r)
        {
            return l.offset_ == r.offset_;
        }

        friend std::ostream &operator<<(std::ostream &stream, const EntityId &id)
        {
            return stream << id.offset_;
        }

    private:
        uint32_t offset_ {0};
    };

    enum OpenMode { READ_ONLY, READ_WRITE };

    StringData GetStringData(EntityId id) const;
    EntityId GetLiteralArraysId() const;

    EntityId GetClassId(const uint8_t *mutf8_name) const;

    const Header *GetHeader() const
    {
        return reinterpret_cast<const Header *>(GetBase());
    }

    const uint8_t *GetBase() const
    {
        return reinterpret_cast<const uint8_t *>(base_.Get());
    }

    const os::mem::ConstBytePtr &GetPtr() const
    {
        return base_;
    }

    bool IsExternal(EntityId id) const
    {
        const Header *header = GetHeader();
        uint32_t foreign_begin = header->foreign_off;
        uint32_t foreign_end = foreign_begin + header->foreign_size;
        return id.GetOffset() >= foreign_begin && id.GetOffset() < foreign_end;
    }

    EntityId GetIdFromPointer(const uint8_t *ptr) const
    {
        return EntityId(ptr - GetBase());
    }

    Span<const uint8_t> GetSpanFromId(EntityId id) const
    {
        const Header *header = GetHeader();
        Span file(GetBase(), header->file_size);
        return file.Last(file.size() - id.GetOffset());
    }

    Span<const uint32_t> GetClasses() const
    {
        const Header *header = GetHeader();
        Span file(GetBase(), header->file_size);
        Span class_idx_data = file.SubSpan(header->class_idx_off, header->num_classes * sizeof(uint32_t));
        return Span(reinterpret_cast<const uint32_t *>(class_idx_data.data()), header->num_classes);
    }

    Span<const uint32_t> GetLiteralArrays() const
    {
        const Header *header = GetHeader();
        Span file(GetBase(), header->file_size);
        Span litarr_idx_data = file.SubSpan(header->literalarray_idx_off, header->num_literalarrays * sizeof(uint32_t));
        return Span(reinterpret_cast<const uint32_t *>(litarr_idx_data.data()), header->num_literalarrays);
    }

    Span<const IndexHeader> GetIndexHeaders() const
    {
        const Header *header = GetHeader();
        Span file(GetBase(), header->file_size);
        auto sp = file.SubSpan(header->index_section_off, header->num_indexes * sizeof(IndexHeader));
        return Span(reinterpret_cast<const IndexHeader *>(sp.data()), header->num_indexes);
    }

    const IndexHeader *GetIndexHeader(EntityId id) const
    {
        auto headers = GetIndexHeaders();
        auto offset = id.GetOffset();
        for (const auto &header : headers) {
            if (header.start <= offset && offset < header.end) {
                return &header;
            }
        }
        return nullptr;
    }

    Span<const EntityId> GetClassIndex(EntityId id) const
    {
        auto *header = GetHeader();
        auto *index_header = GetIndexHeader(id);
        ASSERT(index_header != nullptr);
        Span file(GetBase(), header->file_size);
        auto sp = file.SubSpan(index_header->class_idx_off, index_header->class_idx_size * EntityId::GetSize());
        return Span(reinterpret_cast<const EntityId *>(sp.data()), index_header->class_idx_size);
    }

    Span<const EntityId> GetMethodIndex(EntityId id) const
    {
        auto *header = GetHeader();
        auto *index_header = GetIndexHeader(id);
        ASSERT(index_header != nullptr);
        Span file(GetBase(), header->file_size);
        auto sp = file.SubSpan(index_header->method_idx_off, index_header->method_idx_size * EntityId::GetSize());
        return Span(reinterpret_cast<const EntityId *>(sp.data()), index_header->method_idx_size);
    }

    Span<const EntityId> GetFieldIndex(EntityId id) const
    {
        auto *header = GetHeader();
        auto *index_header = GetIndexHeader(id);
        ASSERT(index_header != nullptr);
        Span file(GetBase(), header->file_size);
        auto sp = file.SubSpan(index_header->field_idx_off, index_header->field_idx_size * EntityId::GetSize());
        return Span(reinterpret_cast<const EntityId *>(sp.data()), index_header->field_idx_size);
    }

    Span<const EntityId> GetProtoIndex(EntityId id) const
    {
        auto *header = GetHeader();
        auto *index_header = GetIndexHeader(id);
        ASSERT(index_header != nullptr);
        Span file(GetBase(), header->file_size);
        auto sp = file.SubSpan(index_header->proto_idx_off, index_header->proto_idx_size * EntityId::GetSize());
        return Span(reinterpret_cast<const EntityId *>(sp.data()), index_header->proto_idx_size);
    }

    Span<const EntityId> GetLineNumberProgramIndex() const
    {
        const Header *header = GetHeader();
        Span file(GetBase(), header->file_size);
        Span lnp_idx_data = file.SubSpan(header->lnp_idx_off, header->num_lnps * EntityId::GetSize());
        return Span(reinterpret_cast<const EntityId *>(lnp_idx_data.data()), header->num_lnps);
    }

    EntityId ResolveClassIndex(EntityId id, Index idx) const
    {
        auto index = GetClassIndex(id);
        return index[idx];
    }

    EntityId ResolveMethodIndex(EntityId id, Index idx) const
    {
        auto index = GetMethodIndex(id);
        return index[idx];
    }

    EntityId ResolveFieldIndex(EntityId id, Index idx) const
    {
        auto index = GetFieldIndex(id);
        return index[idx];
    }

    EntityId ResolveProtoIndex(EntityId id, Index idx) const
    {
        auto index = GetProtoIndex(id);
        return index[idx];
    }

    EntityId ResolveLineNumberProgramIndex(Index32 idx) const
    {
        auto index = GetLineNumberProgramIndex();
        return index[idx];
    }

    const std::string &GetFilename() const
    {
        return FILENAME;
    }

    PandaCache *GetPandaCache() const
    {
        return panda_cache_.get();
    }

    uint32_t GetFilenameHash() const
    {
        return FILENAME_HASH;
    }

    uint64_t GetUniqId() const
    {
        return UNIQ_ID;
    }

    static uint32_t CalcFilenameHash(const std::string &filename);

    static std::unique_ptr<const File> Open(std::string_view filename, OpenMode open_mode = READ_ONLY);

    static std::unique_ptr<const File> OpenFromMemory(os::mem::ConstBytePtr &&ptr);

    static std::unique_ptr<const File> OpenFromMemory(os::mem::ConstBytePtr &&ptr, std::string_view filename);

    static std::unique_ptr<const File> OpenUncompressedArchive(int fd, const std::string_view &filename, size_t size,
                                                               uint32_t offset, OpenMode open_mode = READ_ONLY);

    ~File();

    NO_COPY_SEMANTIC(File);
    NO_MOVE_SEMANTIC(File);

private:
    File(std::string filename, os::mem::ConstBytePtr &&base);

    const std::string FILENAME;
    const uint32_t FILENAME_HASH;
    os::mem::ConstBytePtr base_;
    std::unique_ptr<PandaCache> panda_cache_;
    const uint64_t UNIQ_ID;
};

inline bool operator==(const File::StringData &string_data1, const File::StringData &string_data2)
{
    if (string_data1.utf16_length != string_data2.utf16_length) {
        return false;
    }

    return utf::IsEqual(string_data1.data, string_data2.data);
}

inline bool operator!=(const File::StringData &string_data1, const File::StringData &string_data2)
{
    return !(string_data1 == string_data2);
}

/*
 * OpenPandaFileOrZip from location which specicify the name.
 */
std::unique_ptr<const File> OpenPandaFileOrZip(std::string_view location,
                                               panda_file::File::OpenMode open_mode = panda_file::File::READ_ONLY);

/*
 * OpenPandaFileFromMemory from file buffer.
 */
std::unique_ptr<const File> OpenPandaFileFromMemory(const void *buffer, size_t size);

/*
 * OpenPandaFile from location which specicify the name.
 */
std::unique_ptr<const File> OpenPandaFile(std::string_view location, std::string_view archive_filename = "",
                                          panda_file::File::OpenMode open_mode = panda_file::File::READ_ONLY);

/*
 * OpenPandaFileFromZip from location which specicify the name.
 */
std::unique_ptr<const panda_file::File> OpenPandaFileFromZip(std::string_view location);

/*
 * Open Archive file.
 */
std::unique_ptr<const panda_file::File> HandleArchive(
    FILE *fp, std::string_view location, std::string_view archive_filename = "",
    panda_file::File::OpenMode open_mode = panda_file::File::READ_ONLY);

/*
 * OpenPandaFileFromZip from FILE* inputfile
 */
std::unique_ptr<const panda_file::File> OpenPandaFileFromZipFILE(FILE *inputfile, std::string_view location,
                                                                 std::string_view archive_filename);

/*
 * Check ptr point valid panda file: magic
 */
bool CheckHeader(const os::mem::ConstBytePtr &ptr, const std::string_view &filename = "");

// NOLINTNEXTLINE(readability-identifier-naming)
extern const char *ARCHIVE_FILENAME;

// NOLINTNEXTLINE(readability-identifier-naming)
extern const char *ARCHIVE_FILENAME_ABC;

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_FILE_H_
