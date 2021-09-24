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

namespace panda::test {

using std::unique_ptr;

TEST(LIBZIPARCHIVE, ZipTest)
{
    using uint16 = unsigned short;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__ZipTest__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1, s_pComment,
                                     static_cast<uint16>(strlen(s_pComment)));
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Add a directory entry for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                 static_cast<uint16>(strlen("no comment")));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data, strlen(data) + 1,
                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
               file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
               static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i));

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);
}

TEST(LIBZIPARCHIVE, UnzipWithHeapTest)
{
    using uint16 = unsigned short;
    using uint = unsigned int;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__UnzipWithHeapTest__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1, s_pComment,
                                     static_cast<uint16>(strlen(s_pComment)));
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Add a directory entry for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                 static_cast<uint16>(strlen("no comment")));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data, strlen(data) + 1,
                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
               file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
               static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i));

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);

    // ************* Unzip *************
    // Now verify the compressed data
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(ret, 0);
        return;
    }

    void *p;
    size_t uncomp_size;
    for (i = 0; i < N; i++) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);

        // Try to extract all the files to the heap.
        p = ExtractToHeap(&zip_archive_handler, archive_filename, &uncomp_size);
        if (!p) {
            printf("ExtractToHeap() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        // Make sure the extraction really succeeded.
        size_t dlen = strlen(data);
        if ((uncomp_size != (dlen + 1)) || (memcmp(p, data, dlen))) {
            printf("ExtractToHeap() failed to extract the proper data\n");
            FreeHeap(p);
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Successfully extracted file \"%s\", size %u\n", archive_filename, static_cast<uint>(uncomp_size));

        // We're done.
        FreeHeap(p);
    }

    (void)sprintf_s(archive_filename, 64, "directory/indirectory.txt");
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);

    // Try to extract all the files to the heap.
    p = ExtractToHeap(&zip_archive_handler, archive_filename, &uncomp_size);
    if (!p) {
        printf("ExtractToHeap() failed!\n");
        CloseArchive(&zip_archive_handler);
        ASSERT_EQ(1, 0);
        return;
    }

    // Make sure the extraction really succeeded.
    size_t dlen = strlen(data);
    if ((uncomp_size != (dlen + 1)) || (memcmp(p, data, dlen))) {
        printf("ExtractToHeap() failed to extract the proper data\n");
        FreeHeap(p);
        CloseArchive(&zip_archive_handler);
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Successfully extracted file \"%s\", size %u\n", archive_filename, static_cast<uint>(uncomp_size));

    // We're done.
    FreeHeap(p);

    CloseArchive(&zip_archive_handler);

    printf("Success.\n");
}

TEST(LIBZIPARCHIVE, UnzipWithMemBufferTest)
{
    using uint16 = unsigned short;
    using uint = unsigned int;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__UnzipWithMemBufferTest__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1, s_pComment,
                                     static_cast<uint16>(strlen(s_pComment)));
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Add a directory entry for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                 static_cast<uint16>(strlen("no comment")));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data, strlen(data) + 1,
                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
               file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
               static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i));

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);

    // ************* Unzip *************
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("Can't open archive\n");
        ASSERT_EQ(ret, 0);
        return;
    }

    i = 1;
    (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);

    // Find entry info.
    const char *myname = "1.txt";
    const char *mycomment = "This is a comment";

    EntryFileStat *entry = new EntryFileStat;
    ret = FindEntry(&zip_archive_handler, entry, myname, mycomment);
    if (ret != 0) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("FindEntry() can't find entry: %s!\n", myname);
        ASSERT_EQ(1, 0);
        return;
    }
    uint32_t uncompressed_length = entry->GetUncompressedSize();
    if (entry->GetUncompressedSize() == 0) {
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Entry file has zero length! Readed bad data!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
           entry->GetFileName(), entry->GetComment(), uncompressed_length,
           static_cast<uint>(entry->GetCompressedSize()),
           IsFileDirectory(&zip_archive_handler, static_cast<uint>(entry->GetIndex())));

    // Extract to mem buffer accroding to entry info.
    uint32_t page_size = os::mem::GetPageSize();
    uint32_t min_pages = uncompressed_length / page_size;
    uint32_t size_to_mmap = uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
    // we will use mem in memcmp, so donnot poision it!
    void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
    if (mem == nullptr) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't mmap anonymous!\n");
        ASSERT_EQ(1, 0);
        return;
    }
    ret = ExtractToMemory(&zip_archive_handler, entry, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
    if (ret != 0) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't extract!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Make sure the extraction really succeeded.
    size_t dlen = strlen(data);
    if ((uncompressed_length != (dlen + 1)) || (memcmp(mem, data, dlen))) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("ExtractToMemory() failed to extract the proper data\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Successfully extracted file \"%s\", size %u\n", myname, static_cast<uint>(uncompressed_length));

    // CloseArchive and release resource.
    os::mem::UnmapRaw(mem, size_to_mmap);
    panda::CloseArchive(&zip_archive_handler);
    delete entry;

    printf("Success.\n");
}

TEST(LIBZIPARCHIVE, UnzipForPandafileTest)
{
    using uint16 = unsigned short;
    using uint = unsigned int;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    /*
     * creating an empty pandafile
     */
    pandasm::Parser p;

    auto source = R"()";

    std::string src_filename = "src.pa";
    auto res = p.Parse(source, src_filename);
    ASSERT_EQ(p.ShowError().err, pandasm::Error::ErrorType::ERR_NONE);

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::vector<uint8_t> pf_data {};
    const auto header_ptr = reinterpret_cast<const uint8_t *>(pf->GetHeader());
    pf_data.assign(header_ptr, header_ptr + sizeof(panda_file::File::Header));

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__UnzipForPandafileTest__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1, s_pComment,
                                     static_cast<uint16>(strlen(s_pComment)));
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Add a directory entry for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                 static_cast<uint16>(strlen("no comment")));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data, strlen(data) + 1,
                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a pandafile into zip for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "classes.aex", pf_data.data(), pf_data.size(), s_pComment,
                                 static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
               file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
               static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i));

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);

    // ************* Unzip *************
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("Can't open archive\n");
        ASSERT_EQ(ret, 0);
        return;
    }

    // Find entry info.
    const char *myname = "classes.aex";
    const char *mycomment = "This is a comment";

    EntryFileStat *entry = new EntryFileStat;
    ret = FindEntry(&zip_archive_handler, entry, myname, mycomment);
    if (ret != 0) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("FindEntry() can't find entry: %s!\n", myname);
        ASSERT_EQ(1, 0);
        return;
    }
    uint32_t uncompressed_length = entry->GetUncompressedSize();
    if (entry->GetUncompressedSize() == 0) {
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Entry file has zero length! Readed bad data!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
           entry->GetFileName(), entry->GetComment(), uncompressed_length,
           static_cast<uint>(entry->GetCompressedSize()),
           IsFileDirectory(&zip_archive_handler, static_cast<uint>(entry->GetIndex())));

    // Extract to mem buffer accroding to entry info.
    uint32_t page_size = os::mem::GetPageSize();
    uint32_t min_pages = uncompressed_length / page_size;
    uint32_t size_to_mmap = uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
    // we will using mem in memcmp, so donnot poision it!
    void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
    if (mem == nullptr) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't mmap anonymous!\n");
        ASSERT_EQ(1, 0);
        return;
    }
    ret = ExtractToMemory(&zip_archive_handler, entry, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
    if (ret != 0) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't extract!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Make sure the extraction really succeeded.
    if ((uncompressed_length != pf_data.size()) || (memcmp(mem, pf_data.data(), pf_data.size()))) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("ExtractToMemory() failed to extract the proper data\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Successfully extracted file \"%s\", size %u\n", myname, static_cast<uint>(uncompressed_length));

    // Testing with OpenFromMemory ...
    {
        os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(mem), size_to_mmap, os::mem::MmapDeleter);
        auto ofm_pf = panda_file::File::OpenFromMemory(std::move(ptr), myname);
        if (ofm_pf == nullptr) {
            panda::CloseArchive(&zip_archive_handler);
            delete entry;
            printf("OpenFromMemory() failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
        EXPECT_STREQ((ofm_pf->GetFilename()).c_str(), myname);
    }

    // CloseArchive and release resource.
    panda::CloseArchive(&zip_archive_handler);
    delete entry;

    printf("Success.\n");
}

TEST(LIBZIPARCHIVE, OpenArchiveFILE)
{
    using uint16 = unsigned short;
    using uint = unsigned int;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    /*
     * creating an empty pandafile
     */
    pandasm::Parser p;

    auto source = R"()";

    std::string src_filename = "src.pa";
    auto res = p.Parse(source, src_filename);
    ASSERT_EQ(p.ShowError().err, pandasm::Error::ErrorType::ERR_NONE);

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::vector<uint8_t> pf_data {};
    const auto header_ptr = reinterpret_cast<const uint8_t *>(pf->GetHeader());
    pf_data.assign(header_ptr, header_ptr + sizeof(panda_file::File::Header));

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__OpenArchiveFILE__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1, s_pComment,
                                     static_cast<uint16>(strlen(s_pComment)));
        if (ret != 0) {
            printf("CreateOrAddFileIntoZip failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
    }

    // Add a directory entry for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                 static_cast<uint16>(strlen("no comment")));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data, strlen(data) + 1,
                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Add a pandafile into zip for testing
    ret = CreateOrAddFileIntoZip(s_Test_archive_filename, "classes.aex", pf_data.data(), pf_data.size(), s_pComment,
                                 static_cast<uint16>(strlen(s_pComment)));
    if (ret != 0) {
        printf("CreateOrAddFileIntoZip failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    if (ret != 0) {
        printf("OpenArchive failed!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n",
               file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
               static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i));

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);

    // ************* Unzip *************
    FILE *myfile = fopen(s_Test_archive_filename, "rbe");
    ret = OpenArchiveFILE(myfile, &zip_archive_handler);
    if (ret != 0) {
        CloseArchive(&zip_archive_handler);
        printf("Can't open archive\n");
        ASSERT_EQ(ret, 0);
        return;
    }

    // Find entry info.
    const char *myname = "classes.aex";
    const char *mycomment = "This is a comment";

    EntryFileStat *entry = new EntryFileStat;
    ret = FindEntry(&zip_archive_handler, entry, myname, mycomment);
    if (ret != 0) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("FindEntry() can't find entry: %s!\n", myname);
        ASSERT_EQ(1, 0);
        return;
    }
    uint32_t uncompressed_length = entry->GetUncompressedSize();
    if (entry->GetUncompressedSize() == 0) {
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Entry file has zero length! Readed bad data!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf(
        "Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Compressed(): %d, "
        "Is Dir: %u, m_method %d \n",
        entry->GetFileName(), entry->GetComment(), uncompressed_length, static_cast<uint>(entry->GetCompressedSize()),
        entry->IsCompressed(), IsFileDirectory(&zip_archive_handler, static_cast<uint>(entry->GetIndex())),
        entry->file_stat.m_method);

    if (entry->IsCompressed() == 0) {
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("%s Compressed Entry file %s entry->IsCompressed() == 0!\n", s_Test_archive_filename, myname);
        ASSERT_EQ(1, 0);
        return;
    }

    // Extract to mem buffer accroding to entry info.
    uint32_t page_size = os::mem::GetPageSize();
    uint32_t min_pages = uncompressed_length / page_size;
    uint32_t size_to_mmap = uncompressed_length % page_size == 0 ? min_pages * page_size : (min_pages + 1) * page_size;
    // We will use mem in memcmp, so don not poision it.
    void *mem = os::mem::MapRWAnonymousRaw(size_to_mmap, false);
    if (mem == nullptr) {
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't mmap anonymous!\n");
        ASSERT_EQ(1, 0);
        return;
    }
    ret = ExtractToMemory(&zip_archive_handler, entry, reinterpret_cast<uint8_t *>(mem), size_to_mmap);
    if (ret != 0) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        panda::CloseArchive(&zip_archive_handler);
        delete entry;
        printf("Can't extract!\n");
        ASSERT_EQ(1, 0);
        return;
    }

    // Make sure the extraction really succeeded.
    if ((uncompressed_length != pf_data.size()) || (memcmp(mem, pf_data.data(), pf_data.size()))) {
        os::mem::UnmapRaw(mem, size_to_mmap);
        CloseArchive(&zip_archive_handler);
        delete entry;
        printf("ExtractToMemory() failed to extract the proper data\n");
        ASSERT_EQ(1, 0);
        return;
    }

    printf("Successfully extracted file \"%s\", size %u\n", myname, static_cast<uint>(uncompressed_length));

    // Testing with OpenFromMemory ...
    {
        os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(mem), size_to_mmap, os::mem::MmapDeleter);
        auto ofm_pf = panda_file::File::OpenFromMemory(std::move(ptr), myname);
        if (ofm_pf == nullptr) {
            fclose(myfile);
            panda::CloseArchive(&zip_archive_handler);
            delete entry;
            printf("OpenFromMemory() failed!\n");
            ASSERT_EQ(1, 0);
            return;
        }
        EXPECT_STREQ((ofm_pf->GetFilename()).c_str(), myname);
    }

    // CloseArchive and release resource.
    fclose(myfile);
    panda::CloseArchive(&zip_archive_handler);
    delete entry;

    printf("Success.\n");
}

TEST(LIBZIPARCHIVE, OpenUncompressedArchiveFILE)
{
    using uint16 = unsigned short;
    using uint = unsigned int;

    // The string to compress.
    static const char *s_pTest_str =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras feugiat et odio ac sollicitudin. Maecenas "
        "lobortis ultrices eros sed pharetra. Phasellus in tortor rhoncus, aliquam augue ac, gravida elit. Sed "
        "molestie dolor a vulputate tincidunt. Proin a tellus quam. Suspendisse id feugiat elit, non ornare lacus. "
        "Mauris arcu ex, pretium quis dolor ut, porta iaculis eros. Vestibulum sagittis placerat diam, vitae efficitur "
        "turpis ultrices sit amet. Etiam elementum bibendum congue. In sit amet dolor ultricies, suscipit arcu ac, "
        "molestie urna. Mauris ultrices volutpat massa quis ultrices. Suspendisse rutrum lectus sit amet metus "
        "laoreet, non porta sapien venenatis. Fusce ut massa et purus elementum lacinia. Sed tempus bibendum pretium.";

    // The comment
    static const char *s_pComment = "This is a comment";

    /*
     * creating an empty pandafile
     */
    pandasm::Parser p;

    auto source = R"()";

    std::string src_filename = "src.pa";
    auto res = p.Parse(source, src_filename);
    ASSERT_EQ(p.ShowError().err, pandasm::Error::ErrorType::ERR_NONE);

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::vector<uint8_t> pf_data {};
    const auto header_ptr = reinterpret_cast<const uint8_t *>(pf->GetHeader());
    pf_data.assign(header_ptr, header_ptr + sizeof(panda_file::File::Header));

    // The zip filename
    static const char *s_Test_archive_filename = "__LIBZIPARCHIVE__OpenUncompressedArchiveFILE__.zip";

    int i, ret;
    const int N = 3;
    char data[2048];
    char archive_filename[64];
    assert((strlen(s_pTest_str) + 64) < sizeof(data));

    ZipArchive object;  // on stack, but this is tmp obj as we will CloseArchive this.
    ZipArchiveHandle zip_archive_handler = &object;

    // ZipArchiveHandle zip_archive_handler = new ZipArchive;

    // Delete the test archive, so it doesn't keep growing as we run this test
    remove(s_Test_archive_filename);

    // ************* Create Zip file *************
    // Append a bunch of text files to the test archive
    for (i = (N - 1); i >= 0; --i) {
        (void)sprintf_s(archive_filename, 64, "%u.txt", i);
        (void)sprintf_s(data, 2048, "%u %s %u", (N - 1) - i, s_pTest_str, i);
        ret = CreateOrAddUncompressedFileIntoZip(s_Test_archive_filename, archive_filename, data, strlen(data) + 1,
                                                 s_pComment, static_cast<uint16>(strlen(s_pComment)));
        ASSERT_EQ(ret, 0) << "CreateOrAddUncompressedFileIntoZip failed!";
    }

    // Add a directory entry for testing
    ret = CreateOrAddUncompressedFileIntoZip(s_Test_archive_filename, "directory/", NULL, 0, "no comment",
                                             static_cast<uint16>(strlen("no comment")));
    ASSERT_EQ(ret, 0) << "CreateOrAddUncompressedFileIntoZip failed!";

    // Add a file into directory entry for testing
    (void)sprintf_s(data, 2048, "%u %s %u", N, s_pTest_str, N);
    ret = CreateOrAddUncompressedFileIntoZip(s_Test_archive_filename, "directory/indirectory.txt", data,
                                             strlen(data) + 1, s_pComment, static_cast<uint16>(strlen(s_pComment)));
    ASSERT_EQ(ret, 0) << "CreateOrAddUncompressedFileIntoZip failed!";

    // Add a pandafile into zip for testing
    ret = CreateOrAddUncompressedFileIntoZip(s_Test_archive_filename, "classes.aex", pf_data.data(), pf_data.size(),
                                             s_pComment, static_cast<uint16>(strlen(s_pComment)));
    ASSERT_EQ(ret, 0) << "CreateOrAddUncompressedFileIntoZip failed!";

    // Now try to open the archive.
    ret = OpenArchive(s_Test_archive_filename, &zip_archive_handler);
    ASSERT_EQ(ret, 0) << "OpenArchive failed!\n";

    // Get and print information about each file in the archive.
    for (i = 0; i < GetFileCount(&zip_archive_handler); i++) {
        EntryFileStat file_stat;
        if (!StatFileWithIndex(&zip_archive_handler, &file_stat, i)) {
            printf("StatFileWithIndex() failed!\n");
            CloseArchive(&zip_archive_handler);
            ASSERT_EQ(1, 0);
            return;
        }

        printf(
            "Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u, Offset: %u\n",
            file_stat.GetFileName(), file_stat.GetComment(), file_stat.GetUncompressedSize(),
            static_cast<uint>(file_stat.GetCompressedSize()), IsFileDirectory(&zip_archive_handler, i),
            file_stat.offset);

        if (!strcmp(file_stat.GetFileName(), "directory/")) {
            if (!IsFileDirectory(&zip_archive_handler, i)) {
                printf("IsFileDirectory() didn't return the expected results!\n");
                CloseArchive(&zip_archive_handler);
                ASSERT_EQ(1, 0);
                return;
            }
        }
    }

    // Close the archive, freeing any resources it was using
    CloseArchive(&zip_archive_handler);

    // ************* Unzip *************
    FILE *tmpfile = fopen(s_Test_archive_filename, "rbe");
    ret = OpenArchiveFILE(tmpfile, &zip_archive_handler);
    ASSERT_EQ(ret, 0) << "Can't open archive\n";

    // Find entry info.
    const char *myname = "classes.aex";
    const char *mycomment = "This is a comment";

    unique_ptr<EntryFileStat> fileentry = std::make_unique<EntryFileStat>();
    EntryFileStat *entry = fileentry.get();
    unique_ptr<FILE, void (*)(FILE *)> myfile(tmpfile, [](FILE *file) { fclose(file); });
    unique_ptr<ZipArchiveHandle, bool (*)(ZipArchiveHandle *)> zfile(&zip_archive_handler, CloseArchive);
    ret = FindEntry(&zip_archive_handler, entry, myname, mycomment);

    ASSERT_EQ(ret, 0) << "FindEntry() can't find entry: " << myname << "!" << std::endl;

    std::cout << "Filename: \"" << entry->GetFileName() << "\", Comment: \"" << entry->GetComment()
              << "\", Uncompressed size: " << entry->GetUncompressedSize()
              << ", Compressed size: " << entry->GetCompressedSize() << ", IsCompressed(): " << entry->IsCompressed()
              << ", Is Dir: " << IsFileDirectory(&zip_archive_handler, static_cast<uint>(entry->GetIndex()))
              << ", archive offset: " << entry->GetOffset() << std::endl;

    ASSERT_NE(entry->GetUncompressedSize(), 0);

    ASSERT_EQ(entry->GetUncompressedSize(), entry->GetCompressedSize())
        << "Uncompressed archive:" << myname << "GetUncompressedSize():" << entry->GetUncompressedSize()
        << " GetCompressedSize():" << entry->GetCompressedSize() << std::endl;
    ASSERT_NE(entry->GetOffset(), 0) << "Uncompressed archive:" << myname << " entry->GetOffset()=0" << std::endl;

    ASSERT_NE(entry->IsCompressed(), 1) << s_Test_archive_filename << " Compressed Entry file " << myname
                                        << " entry->IsCompressed() == 0!" << std::endl;

    zfile.reset();
    CloseArchive(&zip_archive_handler);

    std::string_view filename(s_Test_archive_filename);

    std::cout << "HandleArchivee filename: " << filename.data() << std::endl;
    std::unique_ptr<const panda_file::File> file = panda_file::HandleArchive(tmpfile, filename);
    ASSERT_NE(file, nullptr) << "Fail to map " << s_Test_archive_filename << " archive " << entry->GetFileName()
                             << " GetOffset()=" << entry->GetOffset() << std::endl;

    ASSERT_EQ(pf_data.size(), entry->GetUncompressedSize())
        << "pf_data.size()=" << pf_data.size() << " GetUncompressedSize=" << entry->GetUncompressedSize() << std::endl;

    for (uint32_t j = 0; j < entry->GetUncompressedSize(); j++) {
        ASSERT_EQ(pf_data[j], *(file->GetBase() + j))
            << "Fail to map " << s_Test_archive_filename << " archive " << entry->GetFileName() << ", pf[" << j
            << "]=" << pf_data[j] << ", file[" << j << "]=" << *(file->GetBase() + j) << std::endl;
    }

    printf("Success.\n");
}

}  // namespace panda::test
