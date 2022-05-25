/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "openuncompressedarchive_fuzzer.h"
#include "libpandafile/file.h"
#include "libziparchive/zip_archive.h"

namespace OHOS {
void CloseAndRemoveZipFile(panda::ZipArchiveHandle &handle, FILE *fp, const char *filename)
{
    panda::CloseArchiveFile(handle);
    (void)fclose(fp);
    (void)remove(filename);
}

void OpenUncompressedArchiveFuzzTest(const uint8_t *data, size_t size)
{
    // Create zip file
    const char *zip_filename = "__OpenUncompressedArchiveFuzzTest.zip";
    const char *filename = panda::panda_file::ARCHIVE_FILENAME;
    int ret = panda::CreateOrAddFileIntoZip(zip_filename, filename, data, size, APPEND_STATUS_CREATE, Z_NO_COMPRESSION);
    if (ret != 0) {
        (void)remove(zip_filename);
        return;
    }

    // Acquire entry
#ifdef PANDA_TARGET_WINDOWS
    constexpr char const *mode = "rb";
#else
    constexpr char const *mode = "rbe";
#endif
    FILE *fp = fopen(zip_filename, mode);
    if (fp == nullptr) {
        (void)remove(zip_filename);
        return;
    }
    panda::ZipArchiveHandle zipfile = nullptr;
    if (panda::OpenArchiveFile(zipfile, fp) != panda::ZIPARCHIVE_OK) {
        (void)fclose(fp);
        (void)remove(zip_filename);
        return;
    }
    if (panda::LocateFile(zipfile, filename) != panda::ZIPARCHIVE_OK) {
        CloseAndRemoveZipFile(zipfile, fp, zip_filename);
        return;
    }
    panda::EntryFileStat entry;
    if (panda::GetCurrentFileInfo(zipfile, &entry) != panda::ZIPARCHIVE_OK) {
        CloseAndRemoveZipFile(zipfile, fp, zip_filename);
        return;
    }
    if (panda::OpenCurrentFile(zipfile) != panda::ZIPARCHIVE_OK) {
        panda::CloseCurrentFile(zipfile);
        CloseAndRemoveZipFile(zipfile, fp, zip_filename);
        return;
    }
    panda::GetCurrentFileOffset(zipfile, &entry);
    // Call OpenUncompressedArchive
    {
        panda::panda_file::File::OpenUncompressedArchive(fileno(fp), zip_filename, entry.GetUncompressedSize(),
                                                         entry.GetOffset());
    }
    panda::CloseCurrentFile(zipfile);
    CloseAndRemoveZipFile(zipfile, fp, zip_filename);
}
}  // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::OpenUncompressedArchiveFuzzTest(data, size);
    return 0;
}
