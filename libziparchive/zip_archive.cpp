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

#include "zip_archive.h"
#include "os/file.h"
#include "utils/logger.h"

#include <securec.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <cstdio>
#include <memory>

namespace panda {

constexpr size_t ZIP_MAGIC_MASK = 0xff;
constexpr size_t ZIP_MAGIC_OFFSET = 8U;

bool IsZipMagic(uint32_t magic)
{
    return (('P' == ((magic >> 0U) & ZIP_MAGIC_MASK)) && ('K' == ((magic >> ZIP_MAGIC_OFFSET) & ZIP_MAGIC_MASK)));
}

int32_t OpenArchive(const char *zip_filename, ZipArchiveHandle *handle)
{
    if (handle == nullptr || memset_s(*handle, sizeof(ZipArchive), 0, sizeof(ZipArchive)) != EOK) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }

    if (mz_zip_reader_init_file(*handle, zip_filename, 0) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_init_file() failed!";
        return -1;
    }

    return 0;
}

int32_t OpenArchiveFILE(FILE *fp, ZipArchiveHandle *handle)
{
    if (handle == nullptr || memset_s(*handle, sizeof(ZipArchive), 0, sizeof(ZipArchive)) != EOK) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }

    auto file = std::make_unique<os::file::File>(fileno(fp));
    if (file == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "Failed to get get file\n";
        return -1;
    }

    auto res = file->GetFileSize();
    if (!res) {
        LOG(ERROR, ZIPARCHIVE) << "Failed to get size of panda file\n";
        return -1;
    }

    size_t file_size = res.Value();
    if (mz_zip_reader_init_cfile(*handle, fp, file_size, 0) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_init_cfile() failed!\n";
        return -1;
    }

    return 0;
}

bool CloseArchive(ZipArchiveHandle *handle)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return false;
    }
    return mz_zip_reader_end(*handle) != 0;
}

int32_t FindEntry(ZipArchiveHandle *handle, EntryFileStat *entry, const char *entryname, const char *pcomment)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }
    int32_t index = LocateFileIndex(handle, entryname, pcomment);
    if (index == -1) {
        LOG(INFO, ZIPARCHIVE) << "LocateFileIndex() failed!";
        return -1;
    }

    if (!StatFileWithIndex(handle, entry, static_cast<unsigned int>(index))) {
        LOG(INFO, ZIPARCHIVE) << "StatFileWithIndex() failed!";
        return -1;
    }

    return 0;
}

int32_t GetFileCount(ZipArchiveHandle *handle)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }
    return static_cast<int>(mz_zip_reader_get_num_files(*handle));
}

int32_t LocateFileIndex(ZipArchiveHandle *handle, const char *filename, const char *comment)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }
    return static_cast<int32_t>(mz_zip_reader_locate_file(*handle, filename, comment, MZ_ZIP_FLAG_CASE_SENSITIVE));
}

bool StatFileWithIndex(ZipArchiveHandle *handle, EntryFileStat *entry, unsigned int index)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return false;
    }
    if (mz_zip_reader_file_stat(*handle, index, &(entry->file_stat)) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_file_stat() failed!";
        return false;
    }

    if (mz_zip_reader_file_ofs(*handle, &(entry->file_stat), &(entry->offset)) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_file_offset() failed!";
        return false;
    }

    return true;
}

bool IsFileDirectory(ZipArchiveHandle *handle, unsigned int index)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return false;
    }
    return mz_zip_reader_is_file_a_directory(*handle, index) != 0;
}

int32_t ExtractToMemory(ZipArchiveHandle *handle, EntryFileStat *entry, void *buf, size_t buf_size)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return -1;
    }
    if (mz_zip_reader_extract_to_mem(*handle, entry->GetIndex(), buf, buf_size, 0) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_extract_to_mem() failed!\n";
        return -1;
    }
    return 0;
}

void *ExtractToHeap(ZipArchiveHandle *handle, const char *filename, size_t *puncomp_size)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return nullptr;
    }
    void *heap_buf = mz_zip_reader_extract_file_to_heap(*handle, filename, puncomp_size, 0);
    if (heap_buf == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_reader_extract_file_to_heap() failed!\n";
        return nullptr;
    }
    return heap_buf;
}

void FreeHeap(void *heapbuf)
{
    mz_free(heapbuf);
}

int32_t CreateOrAddFileIntoZip(const char *zip_filename, const char *filename, const void *pbuf, size_t buf_size,
                               const void *pcomment, mz_uint16 comment_size)
{
    if (mz_zip_add_mem_to_archive_file_in_place(zip_filename, filename, pbuf, buf_size, pcomment, comment_size,
                                                MZ_BEST_COMPRESSION) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_add_mem_to_archive_file_in_place failed!\n";
        return -1;
    }
    return 0;
}

int32_t CreateOrAddUncompressedFileIntoZip(const char *zip_filename, const char *filename, const void *pbuf,
                                           size_t buf_size, const void *pcomment, mz_uint16 comment_size)
{
    if (mz_zip_add_mem_to_archive_file_in_place(zip_filename, filename, pbuf, buf_size, pcomment, comment_size,
                                                MZ_NO_COMPRESSION) == 0) {
        LOG(ERROR, ZIPARCHIVE) << "mz_zip_add_mem_to_archive_file_in_place failed!\n";
        return -1;
    }
    return 0;
}

bool GetArchiveFileEntry(FILE *inputfile, const char *archive_filename, EntryFileStat *entry)
{
    panda::ZipArchive archive_holder;
    panda::ZipArchiveHandle handle = &archive_holder;
    fseek(inputfile, 0, SEEK_SET);
    auto open_error = panda::OpenArchiveFILE(inputfile, &handle);
    if (open_error != 0) {
        LOG(ERROR, ZIPARCHIVE) << "Can't open archive\n";
        return false;
    }

    auto find_error = panda::FindEntry(&handle, entry, archive_filename);
    if (!panda::CloseArchive(&handle)) {
        LOG(ERROR, ZIPARCHIVE) << "CloseArchive failed!";
        return false;
    }
    fseek(inputfile, 0, SEEK_SET);
    if (find_error != 0) {
        LOG(INFO, ZIPARCHIVE) << "Can't find entry with name '" << archive_filename << "'";
        return false;
    }
    return true;
}

}  // namespace panda
