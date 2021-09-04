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

#ifndef PANDA_RUNTIME_PROFILESAVER_PROFILE_DUMP_INFO_H_
#define PANDA_RUNTIME_PROFILESAVER_PROFILE_DUMP_INFO_H_

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "libpandabase/utils/logger.h"
#include "libpandafile/file.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/mem/panda_string.h"

// NB! suppose that, panda file can always provide such profile file format data!
// NB! we use serializer saving way which means the profile file is just binary file!
// profile header
//      magic
//      version
//      checksum?(ommit)
//      #lines
// Line1:
//      profileline header
//          file location
//          #method
//          #class
//          checksum
//      methods index/id(#method)
//      class index/id(#class)
// LineN:
//      ...

namespace panda {

/*
 * Any newly added information, we have to change the following info naturely, especially
 * ExtractedResolvedClasses
 */
struct ExtractedMethod {
    ExtractedMethod(const panda_file::File *file, panda_file::File::EntityId file_id)
        : panda_file_(file), file_id_(file_id)
    {
    }
    const panda_file::File *panda_file_;  // NOLINT(misc-non-private-member-variables-in-classes)
    panda_file::File::EntityId file_id_;  // NOLINT(misc-non-private-member-variables-in-classes)
};

struct ExtractedResolvedClasses {
public:
    // NOLINTNEXTLINE(modernize-pass-by-value)
    ExtractedResolvedClasses(const PandaString &location, uint32_t checksum)
        : panda_file_location_(location), panda_file_checksum_(checksum)
    {
    }

    int Compare(const ExtractedResolvedClasses &other) const
    {
        if (panda_file_checksum_ != other.panda_file_checksum_) {
            return static_cast<int>(panda_file_checksum_ - other.panda_file_checksum_);
        }
        return panda_file_location_.compare(other.panda_file_location_);
    }

    template <class InputIt>
    void AddClasses(InputIt begin, InputIt end) const
    {
        classes_.insert(begin, end);
    }

    void AddClass(uint32_t classindex) const
    {
        classes_.insert(classindex);
    }

    // NOLINTNEXTLINE(readability-const-return-type)
    const PandaString GetPandaFileLocation() const
    {
        return panda_file_location_;
    }

    uint32_t GetPandaFileChecksum() const
    {
        return panda_file_checksum_;
    }

    const PandaUnorderedSet<uint32_t> &GetClasses() const
    {
        return classes_;
    }

private:
    const PandaString panda_file_location_;  // NOLINT(readability-identifier-naming)
    const uint32_t panda_file_checksum_;     // NOLINT(readability-identifier-naming)
    // Array of resolved class def indexes. we leave this as extention
    mutable PandaUnorderedSet<uint32_t> classes_;
};

// we define this for PandaSet find() function
inline bool operator<(const ExtractedResolvedClasses &a, const ExtractedResolvedClasses &b)
{
    return a.Compare(b) < 0;
}

class ProfileDumpInfo {
public:
    // Content of profile header
    static const uint8_t kProfileMagic[];    // NOLINT(modernize-avoid-c-arrays, readability-identifier-naming)
    static const uint8_t kProfileVersion[];  // NOLINT(modernize-avoid-c-arrays, readability-identifier-naming)

    /*
     * Saves the profile data to the given file descriptor.
     */
    bool Save(int fd);

    /*
     * Loads profile information from the given file descriptor.
     */
    bool Load(int fd);

    /*
     * Merge the data from another ProfileDumpInfo into the current object.
     */
    bool MergeWith(const ProfileDumpInfo &other);

    /*
     * Add the given methods and classes to the current profile object
     */
    bool AddMethodsAndClasses(const PandaVector<ExtractedMethod> &methods,
                              const PandaSet<ExtractedResolvedClasses> &resolved_classes);

    /*
     * Loads and merges profile information from the given file into the current cache
     * object and tries to save it back to disk.
     *
     * If `force` is true then the save will be forced regardless of bad data or mismatched version.
     */
    bool MergeAndSave(const PandaString &filename, uint64_t *bytes_written, bool force);

    /*
     * Returns the number of methods that were profiled.
     */
    uint64_t GetNumberOfMethods() const;

    /*
     * Returns the number of resolved classes that were profiled.
     */
    uint64_t GetNumberOfResolvedClasses() const;

    /*
     * Returns true if the method reference is present in the profiling info.
     */
    bool ContainsMethod(const ExtractedMethod &method_ref) const;

    /*
     * Returns true if the class is present in the profiling info.
     */
    bool ContainsClass(const panda_file::File &pandafile, uint32_t class_def_idx) const;

private:
    enum ProfileLoadSatus {
        PROFILE_LOAD_IO_ERROR,
        PROFILE_LOAD_VERSION_MISMATCH,
        PROFILE_LOAD_BAD_DATA,
        PROFILE_LOAD_EMPTYFILE,
        PROFILE_LOAD_SUCCESS
    };

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    struct ProfileLineHeader {
        PandaString panda_file_location;
        uint32_t method_set_size;
        uint32_t class_set_size;
        uint32_t checksum;
    };

    // A helper structure to make sure we don't read past our buffers in the loops.
    struct SerializerBuffer {
    public:
        explicit SerializerBuffer(size_t size)
        {
            // NOLINTNEXTLINE(modernize-avoid-c-arrays)
            storage_ = MakePandaUnique<uint8_t[]>(size);
            ptr_current_ = storage_.get();
            ptr_end_ = ptr_current_ + size;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic
        }

        ProfileLoadSatus FillFromFd(int fd, const PandaString &source, PandaString *error);

        template <typename T>
        T ReadUintAndAdvance();

        bool CompareAndAdvance(const uint8_t *data, size_t data_size);

        uint8_t *Get()
        {
            return storage_.get();
        }

    private:
        PandaUniquePtr<uint8_t[]> storage_;  // NOLINT(modernize-avoid-c-arrays)
        uint8_t *ptr_current_;
        uint8_t *ptr_end_;
    };

    struct MethodWrapper {
        explicit MethodWrapper(uint32_t index) : method_id_(index) {}
        uint32_t method_id_;  // NOLINT(misc-non-private-member-variables-in-classes)

        bool operator==(const MethodWrapper &other) const
        {
            return method_id_ == other.method_id_;
        }

        bool operator<(const MethodWrapper &other) const
        {
            return method_id_ < other.method_id_;
        }
    };

    struct ClassWrapper {
        explicit ClassWrapper(uint32_t index) : class_id_(index) {}
        uint32_t class_id_;  // NOLINT(misc-non-private-member-variables-in-classes)

        bool operator==(const ClassWrapper &other) const
        {
            return class_id_ == other.class_id_;
        }

        bool operator<(const ClassWrapper &other) const
        {
            return class_id_ < other.class_id_;
        }
    };

    struct ProfileLineData {
        explicit ProfileLineData(uint32_t file_checksum) : checksum(file_checksum) {}
        uint32_t checksum;                           // NOLINT(misc-non-private-member-variables-in-classes)
        PandaSet<MethodWrapper> method_wrapper_set;  // NOLINT(misc-non-private-member-variables-in-classes)
        PandaSet<ClassWrapper> class_wrapper_set;    // NOLINT(misc-non-private-member-variables-in-classes)

        bool operator==(const ProfileLineData &other) const
        {
            return checksum == other.checksum && method_wrapper_set == other.method_wrapper_set &&
                   class_wrapper_set == other.class_wrapper_set;
        }

        // NOLINTNEXTLINE(readability-identifier-naming)
        bool empty() const
        {
            return method_wrapper_set.empty() && class_wrapper_set.empty();
        }
    };

    ProfileLoadSatus LoadInternal(int fd, PandaString *error);
    ProfileLoadSatus ReadProfileHeader(int fd, uint32_t *number_of_lines, PandaString *error);
    ProfileLoadSatus ReadProfileLineHeader(int fd, ProfileLineHeader *line_header, PandaString *error);
    ProfileLoadSatus ReadProfileLine(int fd, const ProfileLineHeader &line_header, PandaString *error);
    // NOLINTNEXTLINE(google-runtime-references)
    bool ProcessLine(SerializerBuffer &line_buffer, uint32_t method_set_size, uint32_t class_set_size,
                     uint32_t checksum, const PandaString &panda_file_location);

    bool AddMethodWrapper(const PandaString &panda_file_location, uint32_t checksum,
                          const MethodWrapper &method_to_add);
    bool AddClassWrapper(const PandaString &panda_file_location, uint32_t checksum, const ClassWrapper &class_to_add);
    bool AddResolvedClasses(const ExtractedResolvedClasses &classes);
    ProfileLineData *GetOrAddProfileLineData(const PandaString &panda_file_location, uint32_t checksum);

    PandaMap<const PandaString, ProfileLineData> dump_info_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_PROFILESAVER_PROFILE_DUMP_INFO_H_
