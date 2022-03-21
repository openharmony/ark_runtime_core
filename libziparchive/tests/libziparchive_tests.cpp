/**
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
#include "libpandafile/file.h"
#include "os/file.h"
#include "os/mem.h"

#include "assembly-emitter.h"
#include "assembly-parser.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include <memory>
#include <securec.h>

#include <climits>
#include <cstdlib>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace panda::test {

constexpr int ZIP_FILENAME_LEN = 64;
constexpr int ZIP_BUFFER_LEN = 2048;

static void GenerateZipfile(const char *data, const char *archivename, int N, char *buf, char *archive_filename, int &i,
                            int &ret, std::vector<uint8_t> &pf_data, int level = Z_BEST_COMPRESSION)
{
    // Delete the test archive, so it doesn't keep growing as we run this test
    (void)remove(archivename);

    // Create and append a directory entry for testing
    ret = CreateOrAddFileIntoZip(archivename, "directory/", NULL, 0, APPEND_STATUS_CREATE, level);
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip for directory failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, ZIP_FILENAME_LEN, "%d.txt", i);
        (void)sprintf_s(buf, ZIP_BUFFER_LEN, "%d %s %d", (N - 1) - i, data, i);
        ret = CreateOrAddFileIntoZip(archivename, archive_filename, buf, strlen(buf) + 1,
                                     APPEND_STATUS_ADDINZIP, level);
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip for %d.txt failed!\n", i);
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Append a file into directory entry for testing
    (void)sprintf_s(buf, ZIP_BUFFER_LEN, "%d %s %d", N, data, N);
    ret = CreateOrAddFileIntoZip(archivename, "directory/indirectory.txt", buf, strlen(buf) + 1,
                                 APPEND_STATUS_ADDINZIP, level);
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip for directory/indirectory.txt failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a pandafile into zip for testing
    ret = CreateOrAddFileIntoZip(archivename, "classes.abc", pf_data.data(), pf_data.size(),
                                 APPEND_STATUS_ADDINZIP, level);
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip for classes.abc failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }
}

static void UnzipFileCheckDirectory(const char *archivename, char *filename, int level = Z_BEST_COMPRESSION)
{
    (void)sprintf_s(filename, ZIP_FILENAME_LEN, "directory/");

    ZipArchiveHandle zipfile = nullptr;
    FILE *myfile = fopen(archivename, "rbe");

    if (OpenArchiveFile(zipfile, myfile) != 0) {
        (void)fclose(myfile);
        printf("OpenArchiveFILE error.\n");
        ASSERT_EQ(1, 0);
        return;
    }
    if (LocateFile(zipfile, filename) != 0) {
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
        printf("LocateFile error.\n");
        ASSERT_EQ(1, 0);
        return;
    }
    EntryFileStat entry = EntryFileStat();
    if (GetCurrentFileInfo(zipfile, &entry) != 0) {
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
        printf("GetCurrentFileInfo test error.\n");
        ASSERT_EQ(1, 0);
        return;
    }
    if (OpenCurrentFile(zipfile) != 0) {
        CloseCurrentFile(zipfile);
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
        printf("OpenCurrentFile test error.\n");
        ASSERT_EQ(1, 0);
        return;
    }

    GetCurrentFileOffset(zipfile, &entry);

    uint32_t uncompressed_length = entry.GetUncompressedSize();

    ASSERT_GT(entry.GetOffset(), 0);
    if (level == Z_NO_COMPRESSION) {
        ASSERT_EQ(entry.IsCompressed(), false);
    } else {
        ASSERT_EQ(entry.IsCompressed(), true);
    }

    printf("Filename: \"%s\", Uncompressed size: %u, Compressed size: %u, , Compressed(): %d, entry offset: %u\n",
           filename, static_cast<uint>(uncompressed_length), (uint)entry.GetCompressedSize(), entry.IsCompressed(),
           (uint)entry.GetOffset());

    CloseCurrentFile(zipfile);
    CloseArchiveFile(zipfile);
    (void)fclose(myfile);
}

static void UnzipFileCheckTxt(const char *archivename, char *filename, const char *data, int N, char *buf, int &ret,
                              int level = Z_BEST_COMPRESSION)
{
    for (int i = 0; i < N; i++) {
        (void)sprintf_s(filename, ZIP_FILENAME_LEN, "%d.txt", i);
        (void)sprintf_s(buf, ZIP_BUFFER_LEN, "%d %s %d", (N - 1) - i, data, i);

        ZipArchiveHandle zipfile = nullptr;
        FILE *myfile = fopen(archivename, "rbe");

        if (OpenArchiveFile(zipfile, myfile) != 0) {
            (void)fclose(myfile);
            printf("OpenArchiveFILE error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (LocateFile(zipfile, filename) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("LocateFile error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        EntryFileStat entry = EntryFileStat();
        if (GetCurrentFileInfo(zipfile, &entry) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("GetCurrentFileInfo test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (OpenCurrentFile(zipfile) != 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("OpenCurrentFile test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }

        GetCurrentFileOffset(zipfile, &entry);

        uint32_t uncompressed_length = entry.GetUncompressedSize();
        if (uncompressed_length == 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("Entry file has zero length! Readed bad data!\n");
            ASSERT_EQ(1, 0);
            return;
        }
        ASSERT_GT(entry.GetOffset(), 0);
        ASSERT_EQ(uncompressed_length, strlen(buf) + 1);
        if (level == Z_NO_COMPRESSION) {
            ASSERT_EQ(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), false);
        } else {
            ASSERT_GE(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), true);
        }

        printf("Filename: \"%s\", Uncompressed size: %u, Compressed size: %u, , Compressed(): %d, entry offset: %u\n",
               filename, static_cast<uint>(uncompressed_length), (uint)entry.GetCompressedSize(), entry.IsCompressed(),
               (uint)entry.GetOffset());

        {
            // Extract to mem buffer accroding to entry info.
            uint32_t page_size = os::mem::GetPageSize();
            uint32_t min_pages = uncompressed_length / page_size;
            uint32_t size_to_mmap =
                uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
            // we will use mem in memcmp, so donnot poision it!
            void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
            if (mem == nullptr) {
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't mmap anonymous!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            ret = ExtractToMemory(zipfile, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
            if (ret != 0) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't extract!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            // Make sure the extraction really succeeded.
            size_t dlen = strlen(buf);
            if (uncompressed_length != (dlen + 1)) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchive(zipfile);
                printf("ExtractToMemory() failed!, uncompressed_length is %u, original strlen is %u\n",
                       static_cast<uint>(uncompressed_length) - 1, static_cast<uint>(dlen));
                ASSERT_EQ(1, 0);
                return;
            }

            if (memcmp(mem, buf, dlen)) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchive(zipfile);
                printf("ExtractToMemory() memcmp failed!");
                ASSERT_EQ(1, 0);
                return;
            }

            printf("Successfully extracted file \"%s\" from \"%s\", size %u\n", filename, archivename,
                   static_cast<uint>(uncompressed_length));

            os::mem::UnmapRaw(mem, size_to_mmap);
        }

        CloseCurrentFile(zipfile);
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
    }
}

static void UnzipFileCheckPandaFile(const char *archivename, char *filename, std::vector<uint8_t> &pf_data, int &ret,
                                    int level = Z_BEST_COMPRESSION)
{
    {
        ZipArchiveHandle zipfile = nullptr;
        FILE *myfile = fopen(archivename, "rbe");

        if (OpenArchiveFile(zipfile, myfile) != 0) {
            (void)fclose(myfile);
            printf("OpenArchiveFILE error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (LocateFile(zipfile, filename) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("LocateFile error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        EntryFileStat entry = EntryFileStat();
        if (GetCurrentFileInfo(zipfile, &entry) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("GetCurrentFileInfo test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (OpenCurrentFile(zipfile) != 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("OpenCurrentFile test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }

        GetCurrentFileOffset(zipfile, &entry);

        uint32_t uncompressed_length = entry.GetUncompressedSize();
        if (uncompressed_length == 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("Entry file has zero length! Readed bad data!\n");
            ASSERT_EQ(1, 0);
            return;
        }
        ASSERT_GT(entry.GetOffset(), 0);
        ASSERT_EQ(uncompressed_length, pf_data.size());
        if (level == Z_NO_COMPRESSION) {
            ASSERT_EQ(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), false);
        } else {
            ASSERT_GE(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), true);
        }
        printf("Filename: \"%s\", Uncompressed size: %u, Compressed size: %u, , Compressed(): %d, entry offset: %u\n",
               filename, static_cast<uint>(uncompressed_length), (uint)entry.GetCompressedSize(), entry.IsCompressed(),
               (uint)entry.GetOffset());

        {
            // Extract to mem buffer accroding to entry info.
            uint32_t page_size = os::mem::GetPageSize();
            uint32_t min_pages = uncompressed_length / page_size;
            uint32_t size_to_mmap =
                uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
            // we will use mem in memcmp, so donnot poision it!
            void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
            if (mem == nullptr) {
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't mmap anonymous!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            ret = ExtractToMemory(zipfile, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
            if (ret != 0) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't extract!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            // Make sure the extraction really succeeded.
            if (uncompressed_length != pf_data.size()) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("ExtractToMemory() failed!, uncompressed_length is %u, original pf_data size is %u\n",
                       static_cast<uint>(uncompressed_length) - 1, (uint)pf_data.size());
                ASSERT_EQ(1, 0);
                return;
            }

            if (memcmp(mem, pf_data.data(), pf_data.size())) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("ExtractToMemory() memcmp failed!");
                ASSERT_EQ(1, 0);
                return;
            }

            printf("Successfully extracted file \"%s\" from \"%s\", size %u\n", filename, archivename,
                   static_cast<uint>(uncompressed_length));

            os::mem::UnmapRaw(mem, size_to_mmap);
        }
        CloseCurrentFile(zipfile);
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
    }
}

static void UnzipFileCheckInDirectory(const char *archivename, char *filename, const char *data, int N, char *buf,
                                      int &ret, int level = Z_BEST_COMPRESSION)
{
    {
        (void)sprintf_s(filename, ZIP_FILENAME_LEN, "directory/indirectory.txt");
        (void)sprintf_s(buf, ZIP_BUFFER_LEN, "%d %s %d", N, data, N);

        // Unzip Check
        ZipArchiveHandle zipfile = nullptr;
        FILE *myfile = fopen(archivename, "rbe");

        if (OpenArchiveFile(zipfile, myfile) != 0) {
            (void)fclose(myfile);
            printf("OpenArchiveFILE error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (LocateFile(zipfile, filename) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("LocateFile error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        EntryFileStat entry = EntryFileStat();
        if (GetCurrentFileInfo(zipfile, &entry) != 0) {
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("GetCurrentFileInfo test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }
        if (OpenCurrentFile(zipfile) != 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("OpenCurrentFile test error.\n");
            ASSERT_EQ(1, 0);
            return;
        }

        GetCurrentFileOffset(zipfile, &entry);

        uint32_t uncompressed_length = entry.GetUncompressedSize();
        if (uncompressed_length == 0) {
            CloseCurrentFile(zipfile);
            CloseArchiveFile(zipfile);
            (void)fclose(myfile);
            printf("Entry file has zero length! Readed bad data!\n");
            ASSERT_EQ(1, 0);
            return;
        }
        ASSERT_GT(entry.GetOffset(), 0);
        ASSERT_EQ(uncompressed_length, strlen(buf) + 1);
        if (level == Z_NO_COMPRESSION) {
            ASSERT_EQ(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), false);
        } else {
            ASSERT_GE(uncompressed_length, entry.GetCompressedSize());
            ASSERT_EQ(entry.IsCompressed(), true);
        }
        printf("Filename: \"%s\", Uncompressed size: %u, Compressed size: %u, , Compressed(): %d, entry offset: %u\n",
               filename, static_cast<uint>(uncompressed_length), (uint)entry.GetCompressedSize(), entry.IsCompressed(),
               (uint)entry.GetOffset());

        {
            // Extract to mem buffer accroding to entry info.
            uint32_t page_size = os::mem::GetPageSize();
            uint32_t min_pages = uncompressed_length / page_size;
            uint32_t size_to_mmap =
                uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
            // we will use mem in memcmp, so donnot poision it!
            void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
            if (mem == nullptr) {
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't mmap anonymous!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            ret = ExtractToMemory(zipfile, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
            if (ret != 0) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchiveFile(zipfile);
                (void)fclose(myfile);
                printf("Can't extract!\n");
                ASSERT_EQ(1, 0);
                return;
            }

            // Make sure the extraction really succeeded.
            size_t dlen = strlen(buf);
            if (uncompressed_length != (dlen + 1)) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchive(zipfile);
                printf("ExtractToMemory() failed!, uncompressed_length is %u, original strlen is %u\n",
                       static_cast<uint>(uncompressed_length) - 1, static_cast<uint>(dlen));
                ASSERT_EQ(1, 0);
                return;
            }

            if (memcmp(mem, buf, dlen)) {
                os::mem::UnmapRaw(mem, size_to_mmap);
                CloseCurrentFile(zipfile);
                CloseArchive(zipfile);
                printf("ExtractToMemory() memcmp failed!");
                ASSERT_EQ(1, 0);
                return;
            }

            printf("Successfully extracted file \"%s\" from \"%s\", size %u\n", filename, archivename,
                   static_cast<uint>(uncompressed_length));

            os::mem::UnmapRaw(mem, size_to_mmap);
        }

        CloseCurrentFile(zipfile);
        CloseArchiveFile(zipfile);
        (void)fclose(myfile);
    }
}

TEST(LIBZIPARCHIVE, ZipFile)
{
    static const char *data =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    /*
     * creating an empty pandafile
     */
    std::vector<uint8_t> pf_data {};
    {
        pandasm::Parser p;

        auto source = R"()";

        std::string src_filename = "src.pa";
        auto res = p.Parse(source, src_filename);
        ASSERT_EQ(p.ShowError().err, pandasm::Error::ErrorType::ERR_NONE);

        auto pf = pandasm::AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        const auto header_ptr = reinterpret_cast<const uint8_t *>(pf->GetHeader());
        pf_data.assign(header_ptr, header_ptr + sizeof(panda_file::File::Header));
    }

    static const char *archivename = "__LIBZIPARCHIVE__ZipFile__.zip";
    const int N = 3;
    char buf[ZIP_BUFFER_LEN];
    char archive_filename[ZIP_FILENAME_LEN];
    int i = 0;
    int ret = 0;

    GenerateZipfile(data, archivename, N, buf, archive_filename, i, ret, pf_data);

    // Quick Check
    ZipArchiveHandle zipfile = nullptr;
    if (OpenArchive(zipfile, archivename) != 0) {
        printf("OpenArchive error.\n");
        ASSERT_EQ(1, 0);
        return;
    }

    GlobalStat gi = GlobalStat();
    if (GetGlobalFileInfo(zipfile, &gi) != 0) {
        printf("GetGlobalFileInfo error.\n");
        ASSERT_EQ(1, 0);
        return;
    }
    for (i = 0; i < (int)gi.GetNumberOfEntry(); ++i) {
        EntryFileStat file_stat;
        if (GetCurrentFileInfo(zipfile, &file_stat) != 0) {
            CloseArchive(zipfile);
            printf("GetCurrentFileInfo error. Current index i = %d \n", i);
            ASSERT_EQ(1, 0);
            return;
        }
        printf("Index: \"%u\", Uncompressed size: %u, Compressed size: %u, Compressed(): %d\n", i,
               (uint)file_stat.GetUncompressedSize(), file_stat.GetCompressedSize(), file_stat.IsCompressed());
        if ((i + 1) < (int)gi.GetNumberOfEntry()) {
            if (GoToNextFile(zipfile) != 0) {
                CloseArchive(zipfile);
                printf("GoToNextFile error. Current index i = %d \n", i);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    CloseArchive(zipfile);
    (void)remove(archivename);
    printf("Success.\n");
}

TEST(LIBZIPARCHIVE, UnZipFile)
{
    static const char *data =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    /*
     * creating an empty pandafile
     */
    std::vector<uint8_t> pf_data {};
    {
        pandasm::Parser p;

        auto source = R"()";

        std::string src_filename = "src.pa";
        auto res = p.Parse(source, src_filename);
        ASSERT_EQ(p.ShowError().err, pandasm::Error::ErrorType::ERR_NONE);

        auto pf = pandasm::AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        const auto header_ptr = reinterpret_cast<const uint8_t *>(pf->GetHeader());
        pf_data.assign(header_ptr, header_ptr + sizeof(panda_file::File::Header));
    }

    // The zip filename
    static const char *archivename = "__LIBZIPARCHIVE__UnZipFile__.zip";
    const int N = 3;
    char buf[ZIP_BUFFER_LEN];
    char archive_filename[ZIP_FILENAME_LEN];
    char filename[ZIP_FILENAME_LEN];
    int i = 0;
    int ret = 0;

    GenerateZipfile(data, archivename, N, buf, archive_filename, i, ret, pf_data);

    UnzipFileCheckDirectory(archivename, filename);

    UnzipFileCheckTxt(archivename, filename, data, N, buf, ret);

    UnzipFileCheckInDirectory(archivename, filename, data, N, buf, ret);

    (void)sprintf_s(filename, ZIP_FILENAME_LEN, "classes.abc");
    UnzipFileCheckPandaFile(archivename, filename, pf_data, ret);

    (void)remove(archivename);
    printf("Success.\n");
}
}  // namespace panda::test