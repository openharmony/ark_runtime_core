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

#include "zip_archive.h"
#include "os/file.h"
#include "utils/logger.h"

#include <securec.h>

namespace panda {

constexpr size_t ZIP_MAGIC_MASK = 0xff;
constexpr size_t ZIP_MAGIC_OFFSET = 8U;

bool IsZipMagic(uint32_t magic)
{
    return (('P' == ((magic >> 0U) & ZIP_MAGIC_MASK)) && ('K' == ((magic >> ZIP_MAGIC_OFFSET) & ZIP_MAGIC_MASK)));
}

int OpenArchive(ZipArchiveHandle &handle, const char *path)
{
    handle = unzOpen(path);
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "OpenArchive failed, filename is " << path;
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int OpenArchiveFile(ZipArchiveHandle &handle, FILE *fp)
{
    handle = unzOpenFile(fp);
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "OpenArchive failed from FILE *fp";
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int CloseArchive(ZipArchiveHandle &handle)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return ZIPARCHIVE_ERR;
    }
    int err = unzClose(handle);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "unzClose with error: " << err;
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int CloseArchiveFile(ZipArchiveHandle &handle)
{
    if (handle == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "ZipArchiveHandle handle should not be nullptr";
        return ZIPARCHIVE_ERR;
    }
    int err = unzCloseFile(handle);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "unzCloseFile with error: " << err;
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int GetGlobalFileInfo(ZipArchiveHandle &handle, GlobalStat *gstat)
{
    int err = unzGetGlobalInfo(handle, &gstat->ginfo);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "GetGlobalFileInfo with error: " << err;
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int GoToNextFile(ZipArchiveHandle &handle)
{
    int err = unzGoToNextFile(handle);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "GoToNextFile with error: " << err;
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int LocateFile(ZipArchiveHandle &handle, const char *filename)
{
    int err = unzLocateFile2(handle, filename, 0);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << filename << " is not found in the zipfile";
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int GetCurrentFileInfo(ZipArchiveHandle &handle, EntryFileStat *entry)
{
    int err = unzGetCurrentFileInfo(handle, &entry->file_stat, nullptr, 0, nullptr, 0, nullptr, 0);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "unzGetCurrentFileInfo failed!";
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int OpenCurrentFile(ZipArchiveHandle &handle)
{
    int err = unzOpenCurrentFile(handle);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "OpenCurrentFile failed!";
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

void GetCurrentFileOffset(ZipArchiveHandle &handle, EntryFileStat *entry)
{
    entry->offset = static_cast<uint32_t>(unzGetCurrentFileZStreamPos64(handle));
}

int CloseCurrentFile(ZipArchiveHandle &handle)
{
    int err = unzCloseCurrentFile(handle);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "CloseCurrentFile failed!";
        return ZIPARCHIVE_ERR;
    }
    return ZIPARCHIVE_OK;
}

int ExtractToMemory(ZipArchiveHandle &handle, void *buf, size_t buf_size)
{
    int size = unzReadCurrentFile(handle, buf, buf_size);
    if (size < 0) {
        LOG(ERROR, ZIPARCHIVE) << "ExtractToMemory failed!";
        return ZIPARCHIVE_ERR;
    }
    LOG(INFO, ZIPARCHIVE) << "ExtractToMemory size is " << size;
    return ZIPARCHIVE_OK;
}

int CreateOrAddFileIntoZip(const char *zipname, const char *filename, const void *pbuf, size_t buf_size, int append,
                           int level)
{
    zipFile zfile = nullptr;
    zfile = zipOpen(zipname, append);
    if (zfile == nullptr) {
        LOG(ERROR, ZIPARCHIVE) << "CreateArchive failed, zipname is " << zipname;
        return ZIPARCHIVE_ERR;
    }
    int success = ZIPARCHIVE_OK;
    int err = zipOpenNewFileInZip(zfile, filename, nullptr, nullptr, 0, nullptr, 0, nullptr,
                                  (level != 0) ? Z_DEFLATED : 0, level);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "zipOpenNewFileInZip failed!, zipname is" << zipname << ", filename is " << filename;
        return ZIPARCHIVE_ERR;
    }
    err = zipWriteInFileInZip(zfile, pbuf, buf_size);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "zipWriteInFileInZip failed!, zipname is" << zipname << ", filename is " << filename;
        success = ZIPARCHIVE_ERR;
    }
    err = zipCloseFileInZip(zfile);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "zipCloseFileInZip failed!, zipname is" << zipname << ", filename is " << filename;
    }
    err = zipClose(zfile, nullptr);
    if (err != UNZ_OK) {
        LOG(ERROR, ZIPARCHIVE) << "CloseArcive failed!, zipname is" << zipname;
    }
    return success;
}
}  // namespace panda
