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

#ifndef PANDA_LIBZIPARCHIVE_ZIP_ARCHIVE_H_
#define PANDA_LIBZIPARCHIVE_ZIP_ARCHIVE_H_

#include "miniz.h"

namespace panda {

using ZipArchive = mz_zip_archive;
using ZipArchiveHandle = mz_zip_archive *;

struct EntryFileStat {
public:
    const char *GetFileName() const
    {
        return file_stat.m_filename;
    }

    const char *GetComment() const
    {
        return file_stat.m_comment;
    }

    uint32_t GetUncompressedSize() const
    {
        return (uint32_t)file_stat.m_uncomp_size;
    }

    uint32_t GetCompressedSize() const
    {
        return (uint32_t)file_stat.m_comp_size;
    }

    uint32_t GetIndex() const
    {
        return file_stat.m_file_index;
    }

    inline uint32_t GetOffset() const
    {
        return offset;
    }

    inline bool IsCompressed()
    {
        return mz_zip_reader_file_is_compressed(&file_stat);
    }

    mz_zip_archive_file_stat file_stat;
    uint32_t offset;
};

/*
 * Judge whether magic is zip magic.
 */
bool IsZipMagic(uint32_t magic);

/*
 * Open a Zip archive, and set handle for the file.
 * This handle must be released by calling CloseArchive with this handle.
 * CloseArchive will close the file zip_filename opened.
 *
 * Returns 0 on success, and -1 on failure.
 */
int32_t OpenArchive(const char *zip_filename, ZipArchiveHandle *handle);

/*
 * Open a Zip archive FILE, and set handle for the file.
 * This handle must be released by calling CloseArchive with this handle.
 * CloseArchive will not close the fp. It is the caller's responsibility.
 *
 * Returns 0 on success, and -1 on failure.
 */
int32_t OpenArchiveFILE(FILE *fp, ZipArchiveHandle *handle);

/*
 * Close archive, releasing internal resources associated with it.
 */
bool CloseArchive(ZipArchiveHandle *handle);

/*
 * Find an entry in the Zip archive by entryname as well as comment name.
 * comment name defaultly is nullptr.
 *
 * stat must point to a writeable memory location (e.g. new/malloc).
 *
 * Return 0 if an entry is found, and populate stat with information about this entry,
 * Return -1 otherwise.
 */
int32_t FindEntry(ZipArchiveHandle *handle, EntryFileStat *entry, const char *entryname,
                  const char *pcomment = nullptr);

/*
 * Return the total number of files in the archive.
 */
int32_t GetFileCount(ZipArchiveHandle *handle);

/*
 * Attempt to locate a file in the archive's central directory.
 * Return the index if file be found, otherwise -1.
 */
int32_t LocateFileIndex(ZipArchiveHandle *handle, const char *filename, const char *comment = nullptr);

/*
 * Return detailed information about an archive file entry according to index.
 */
bool StatFileWithIndex(ZipArchiveHandle *handle, EntryFileStat *entry, unsigned int index);

/*
 * Return true if the archive file entry index is a directory, otherwise false
 */
bool IsFileDirectory(ZipArchiveHandle *handle, unsigned int index);

/*
 * Uncompress a given zip entry to the memory buf of size |buf_size|.
 * This size is expected to be equal or larger than the uncompressed length of the zip entry.
 * Returns 0 on success and -1 on failure.
 */
int32_t ExtractToMemory(ZipArchiveHandle *handle, EntryFileStat *entry, void *buf, size_t buf_size);

/*
 * Uncompress a given filename in zip to heap memory.
 * The corresponding uncompressed_size is returned in puncomp_size by pointer.
 * Returns the heap address on success and nullptr on failure.
 */
void *ExtractToHeap(ZipArchiveHandle *handle, const char *filename, size_t *puncomp_size);

/*
 * Release a block allocated from the heap.
 */
void FreeHeap(void *heapbuf);

/*
 * Add a new file (in memory) to the archive. Note this is an IN-PLACE operation,
 * so if it fails your archive is probably hosed (its central directory may not be complete) but it should be
 * recoverable using zip -F or -FF. So use this with caution.
 */
int32_t CreateOrAddFileIntoZip(const char *zip_filename, const char *filename, const void *pbuf, size_t buf_size,
                               const void *pcomment = nullptr, mz_uint16 comment_size = 0);

/*
 * Add a new file (in memory) to the archive without compression. Note this is an IN-PLACE operation,
 * so if it fails your archive is probably hosed (its central directory may not be complete) but it should be
 * recoverable using zip -F or -FF. So use this with caution.
 */
int32_t CreateOrAddUncompressedFileIntoZip(const char *zip_filename, const char *filename, const void *pbuf,
                                           size_t buf_size, const void *pcomment, mz_uint16 comment_size);

/*
 * GetArchiveFileEntry from FILE* inputfile
 */
bool GetArchiveFileEntry(FILE *inputfile, const char *archive_filename, EntryFileStat *entry);

}  // namespace panda

#endif  // PANDA_LIBZIPARCHIVE_ZIP_ARCHIVE_H_
