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

#include <cstdint>
#include "unzip.h"
#include "zip.h"

namespace panda {

constexpr int ZIPARCHIVE_OK = 0;
constexpr int ZIPARCHIVE_ERR = 1;

using ZipArchiveHandle = unzFile;

struct EntryFileStat {
public:
    uint32_t GetUncompressedSize() const
    {
        return (uint32_t)file_stat.uncompressed_size;
    }

    uint32_t GetCompressedSize() const
    {
        return (uint32_t)file_stat.compressed_size;
    }

    inline uint32_t GetOffset() const
    {
        return offset;
    }

    inline bool IsCompressed() const
    {
        return file_stat.compression_method != 0;
    }

    unz_file_info file_stat;
    uint32_t offset;
};

struct GlobalStat {
public:
    uint32_t GetNumberOfEntry() const
    {
        return (uint32_t)ginfo.number_entry;
    }
    unz_global_info ginfo;
};

/*
 * Judge whether magic is zip magic.
 */
bool IsZipMagic(uint32_t magic);

/*
 * Open a Zip archive from filename path, and sets handle for the file.
 * This handle must be released by calling CloseArchive with this handle.
 * CloseArchive will close the file opened.
 *
 * Returns 0 on success, and 1 on failure.
 */
int OpenArchive(ZipArchiveHandle &handle, const char *path);

/*
 * Close archive opened with OpenArchive, releasing internal resources associated with it.
 */
int CloseArchive(ZipArchiveHandle &handle);

/*
 * Open a Zip archive from opened file FILE* fp, and sets handle for the file.
 * This handle must be released by calling CloseArchiveFile with this handle.
 * CloseArchiveFile will not close the fp. It is the caller's responsibility.
 *
 * Returns 0 on success, and 1 on failure.
 */
int OpenArchiveFile(ZipArchiveHandle &handle, FILE *fp);

/*
 * Close archive opened with OpenArchiveFile, releasing internal resources associated with it.
 *
 * Returns 0 on success, and 1 on failure.
 */
int CloseArchiveFile(ZipArchiveHandle &handle);

/*
 * Write the info about the ZipFile into *gstat structure.
 *
 * Returns 0 on success, and 1 on failure.
 */
int GetGlobalFileInfo(ZipArchiveHandle &handle, GlobalStat *gstat);

/*
 * Set the current file of the zipfile to the next file.
 *
 * Returns 0 on success, and 1 on failure.
 */
int GoToNextFile(ZipArchiveHandle &handle);

/*
 * Try locate the file filename in the zipfile.
 *
 * Returns 0 on success, and 1 on failure.
 */
int LocateFile(ZipArchiveHandle &handle, const char *filename);

/*
 * Get Info about the current file within ZipFile and write info into the *entry structure.
 * No preparation of the structure is needed
 *
 * Returns 0 on success, and 1 on failure.
 */
int GetCurrentFileInfo(ZipArchiveHandle &handle, EntryFileStat *entry);

/*
 * Open for reading data the current file in the zipfile.
 * This handle must be released by calling CloseCurrentFile with this handle.
 *
 * Returns 0 on success, and 1 on failure.
 */
int OpenCurrentFile(ZipArchiveHandle &handle);

/*
 * Get the current file offset opened with OpenCurrentFile. The offset will be stored into entry->offset.
 */
void GetCurrentFileOffset(ZipArchiveHandle &handle, EntryFileStat *entry);

/*
 * Close the file in zip opened with unzOpenCurrentFile
 *
 * Returns 0 on success, and 1 on failure.
 */
int CloseCurrentFile(ZipArchiveHandle &handle);

/*
 * Uncompress a given zip archive represented with handle to buf of size |buf_size|.
 * This size is expected to be equal or larger than the uncompressed length of the zip entry.
 *
 * Returns 0 on success and 1 on failure.
 */
int ExtractToMemory(ZipArchiveHandle &handle, void *buf, size_t buf_size);

/*
 * Add a new file filename(resident in memory pbuf which has size of size |buf_size|) to the archive zipname,
 * append takes value from APPEND_STATUS_CREATE(which will create the archive zipname for first time) and
 * APPEND_STATUS_ADDINZIP(which willappend filename into exsisted zip archive zipname).
 * level takes value from Z_BEST_COMPRESSION(which will deflate the pbuf with best compression effect) and
 * Z_NO_COMPRESSION(which will store the pbuf into zipname without compression).
 *
 * Returns 0 on success and 1 on failure.
 */
int CreateOrAddFileIntoZip(const char *zipname, const char *filename, const void *pbuf, size_t buf_size,
                           int append = APPEND_STATUS_CREATE, int level = Z_BEST_COMPRESSION);
}  // namespace panda

#endif  // PANDA_LIBZIPARCHIVE_ZIP_ARCHIVE_H_
