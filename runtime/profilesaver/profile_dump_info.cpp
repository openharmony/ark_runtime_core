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

#include "runtime/profilesaver/profile_dump_info.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <cerrno>
#include <climits>
#include <cstring>

#include "libpandabase/os/unix/failure_retry.h"
#include "trace/trace.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

namespace panda {
static constexpr size_t K_BITS_PER_BYTE = 8;

static constexpr size_t K_LINE_HEADER_SIZE = 3 * sizeof(uint32_t) + sizeof(uint16_t);
static constexpr size_t K_METHOD_BYTES = 4;
static constexpr size_t K_CLASS_BYTES = 4;

const uint8_t ProfileDumpInfo::kProfileMagic[] = {'p', 'r', 'o', 'f', '\0'};  // NOLINT
const uint8_t ProfileDumpInfo::kProfileVersion[] = {'0', '1', '\0'};          // NOLINT

static constexpr uint16_t K_MAX_FILE_KEY_LENGTH = PATH_MAX;  // NOLINT

static bool WriteBuffer(int fd, const uint8_t *buffer, size_t byte_count)
{  // NOLINT
    while (byte_count > 0) {
        int bytes_written = write(fd, buffer, byte_count);  // real place to write
        if (bytes_written == -1) {
            return false;
        }
        byte_count -= bytes_written;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        buffer += bytes_written;
    }
    return true;
}

static void AddStringToBuffer(PandaVector<uint8_t> *buffer, const PandaString &value)
{  // NOLINT
    buffer->insert(buffer->end(), value.begin(), value.end());
}

template <typename T>
static void AddUintToBuffer(PandaVector<uint8_t> *buffer, T value)
{
    for (size_t i = 0; i < sizeof(T); i++) {
        buffer->push_back((value >> (i * K_BITS_PER_BYTE)) & 0xff);  // NOLINT
    }
}

/*
 * Tests for EOF by trying to read 1 byte from the descriptor.
 * Returns:
 *   0 if the descriptor is at the EOF,
 *  -1 if there was an IO error
 *   1 if the descriptor has more content to read
 */
// NOLINTNEXTLINE(readability-identifier-naming)
static int testEOF(int fd)
{
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    uint8_t buffer[1];
    return read(fd, buffer, 1);
}

int64_t GetFileSizeBytes(const PandaString &filename)
{
    struct stat stat_buf {
    };
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

ProfileDumpInfo::ProfileLoadSatus ProfileDumpInfo::SerializerBuffer::FillFromFd(int fd, const PandaString &source,
                                                                                PandaString *error)
{
    size_t byte_count = ptr_end_ - ptr_current_;
    uint8_t *buffer = ptr_current_;
    while (byte_count > 0) {
        int bytes_read = read(fd, buffer, byte_count);
        if (bytes_read == 0) {  // NOLINT
            *error += "Profile EOF reached prematurely for " + source;
            return PROFILE_LOAD_BAD_DATA;
        } else if (bytes_read < 0) {  // NOLINT
            *error += "Profile IO error for " + source + ConvertToString(os::Error(errno).ToString());
            return PROFILE_LOAD_IO_ERROR;
        }
        byte_count -= bytes_read;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        buffer += bytes_read;
    }
    return PROFILE_LOAD_SUCCESS;
}

template <typename T>
T ProfileDumpInfo::SerializerBuffer::ReadUintAndAdvance()
{
    static_assert(std::is_unsigned<T>::value, "Type is not unsigned");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ASSERT(ptr_current_ + sizeof(T) <= ptr_end_);
    T value = 0;
    for (size_t i = 0; i < sizeof(T); i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        value += ptr_current_[i] << (i * K_BITS_PER_BYTE);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ptr_current_ += sizeof(T);
    return value;
}

bool ProfileDumpInfo::SerializerBuffer::CompareAndAdvance(const uint8_t *data, size_t data_size)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ptr_current_ + data_size > ptr_end_) {
        return false;
    }
    if (memcmp(ptr_current_, data, data_size) == 0) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ptr_current_ += data_size;
        return true;
    }
    return false;
}

bool ProfileDumpInfo::MergeWith(const ProfileDumpInfo &other)
{
    for (const auto &other_it : other.dump_info_) {
        auto info_it = dump_info_.find(other_it.first);
        if ((info_it != dump_info_.end()) && (info_it->second.checksum != other_it.second.checksum)) {
            LOG(INFO, RUNTIME) << "info_it->second.checksum" << info_it->second.checksum;
            LOG(INFO, RUNTIME) << "other_it->second.checksum" << other_it.second.checksum;
            LOG(INFO, RUNTIME) << "Checksum mismatch" << other_it.first;
            return false;
        }
    }
    LOG(INFO, RUNTIME) << "All checksums match";

    for (const auto &other_it : other.dump_info_) {
        const PandaString &other_profile_location = other_it.first;
        const ProfileLineData &other_profile_data = other_it.second;
        auto info_it = dump_info_.find(other_profile_location);
        if (info_it == dump_info_.end()) {
            auto ret =
                dump_info_.insert(std::make_pair(other_profile_location, ProfileLineData(other_profile_data.checksum)));
            ASSERT(ret.second);
            info_it = ret.first;
        }
        info_it->second.method_wrapper_set.insert(other_profile_data.method_wrapper_set.begin(),
                                                  other_profile_data.method_wrapper_set.end());
        info_it->second.class_wrapper_set.insert(other_profile_data.class_wrapper_set.begin(),
                                                 other_profile_data.class_wrapper_set.end());
    }
    return true;
}

bool ProfileDumpInfo::AddMethodsAndClasses(const PandaVector<ExtractedMethod> &methods,
                                           const PandaSet<ExtractedResolvedClasses> &resolved_classes)
{
    for (const ExtractedMethod &method : methods) {
        if (!AddMethodWrapper(ConvertToString(method.panda_file_->GetFilename()),
                              method.panda_file_->GetHeader()->checksum, MethodWrapper(method.file_id_.GetOffset()))) {
            return false;
        }
    }

    for (const ExtractedResolvedClasses &class_resolved : resolved_classes) {
        if (!AddResolvedClasses(class_resolved)) {
            return false;
        }
    }
    return true;
}

uint64_t ProfileDumpInfo::GetNumberOfMethods() const
{
    uint64_t total = 0;
    for (const auto &it : dump_info_) {
        total += it.second.method_wrapper_set.size();
    }
    return total;
}

uint64_t ProfileDumpInfo::GetNumberOfResolvedClasses() const
{
    uint64_t total = 0;
    for (const auto &it : dump_info_) {
        total += it.second.class_wrapper_set.size();
    }
    return total;
}

bool ProfileDumpInfo::ContainsMethod(const ExtractedMethod &method_ref) const
{
    auto info_it = dump_info_.find(ConvertToString(method_ref.panda_file_->GetFilename()));
    if (info_it != dump_info_.end()) {
        if (method_ref.panda_file_->GetHeader()->checksum != info_it->second.checksum) {
            return false;
        }
        const PandaSet<MethodWrapper> &methods = info_it->second.method_wrapper_set;
        return methods.find(MethodWrapper(method_ref.file_id_.GetOffset())) != methods.end();
    }
    return false;
}

bool ProfileDumpInfo::ContainsClass(const panda_file::File &pandafile, uint32_t class_def_idx) const
{
    auto info_it = dump_info_.find(ConvertToString(pandafile.GetFilename()));
    if (info_it != dump_info_.end()) {
        if (pandafile.GetHeader()->checksum != info_it->second.checksum) {
            return false;
        }
        const PandaSet<ClassWrapper> &classes = info_it->second.class_wrapper_set;
        return classes.find(ClassWrapper(class_def_idx)) != classes.end();
    }
    return false;
}

bool ProfileDumpInfo::AddMethodWrapper(const PandaString &panda_file_location, uint32_t checksum,
                                       const ProfileDumpInfo::MethodWrapper &method_to_add)
{
    ProfileLineData *const DATA = GetOrAddProfileLineData(panda_file_location, checksum);
    if (DATA == nullptr) {
        return false;
    }
    DATA->method_wrapper_set.insert(method_to_add);
    return true;
}

bool ProfileDumpInfo::AddClassWrapper(const PandaString &panda_file_location, uint32_t checksum,
                                      const ProfileDumpInfo::ClassWrapper &class_to_add)
{
    ProfileLineData *const DATA = GetOrAddProfileLineData(panda_file_location, checksum);
    if (DATA == nullptr) {
        return false;
    }
    DATA->class_wrapper_set.insert(class_to_add);
    return true;
}

bool ProfileDumpInfo::AddResolvedClasses(const ExtractedResolvedClasses &classes)
{                                                                            // NOLINT(readability-identifier-naming)
    const PandaString panda_file_location = classes.GetPandaFileLocation();  // NOLINT(readability-identifier-naming)
    const uint32_t checksum = classes.GetPandaFileChecksum();                // NOLINT(readability-identifier-naming)
    ProfileLineData *const DATA = GetOrAddProfileLineData(panda_file_location, checksum);
    if (DATA == nullptr) {
        return false;
    }
    for (auto const &i : classes.GetClasses()) {
        DATA->class_wrapper_set.insert(ClassWrapper(i));
    }
    return true;
}

ProfileDumpInfo::ProfileLineData *ProfileDumpInfo::GetOrAddProfileLineData(const PandaString &panda_file_location,
                                                                           uint32_t checksum)
{
    auto info_it = dump_info_.find(panda_file_location);
    if (info_it == dump_info_.end()) {
        auto ret = dump_info_.insert(std::make_pair(panda_file_location, ProfileLineData(checksum)));
        ASSERT(ret.second);
        info_it = ret.first;
    }
    if (info_it->second.checksum != checksum) {
        LOG(INFO, RUNTIME) << "Checksum mismatch" << panda_file_location;
        return nullptr;
    }
    return &(info_it->second);
}

bool ProfileDumpInfo::Save(int fd)
{
    ASSERT(fd >= 0);
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);

    static constexpr size_t K_MAX_BUFFER_SIZE = 8 * 1024;
    PandaVector<uint8_t> buffer;  // each element 1 byte

    WriteBuffer(fd, kProfileMagic, sizeof(kProfileMagic));
    WriteBuffer(fd, kProfileVersion, sizeof(kProfileVersion));
    AddUintToBuffer(&buffer, static_cast<uint32_t>(dump_info_.size()));

    for (const auto &it : dump_info_) {
        if (buffer.size() > K_MAX_BUFFER_SIZE) {
            if (!WriteBuffer(fd, buffer.data(), buffer.size())) {
                return false;
            }
            buffer.clear();
        }
        const PandaString &file_location = it.first;
        const ProfileLineData &file_data = it.second;

        if (file_location.size() >= K_MAX_FILE_KEY_LENGTH) {
            LOG(INFO, RUNTIME) << "PandaFileKey exceeds allocated limit";
            return false;
        }

        size_t required_capacity = buffer.size() + K_LINE_HEADER_SIZE + file_location.size() +
                                   K_METHOD_BYTES * file_data.method_wrapper_set.size() +
                                   K_CLASS_BYTES * file_data.class_wrapper_set.size();
        buffer.reserve(required_capacity);

        ASSERT(file_location.size() <= std::numeric_limits<uint16_t>::max());
        ASSERT(file_data.method_wrapper_set.size() <= std::numeric_limits<uint32_t>::max());
        ASSERT(file_data.class_wrapper_set.size() <= std::numeric_limits<uint32_t>::max());

        AddUintToBuffer(&buffer, static_cast<uint16_t>(file_location.size()));
        AddUintToBuffer(&buffer, static_cast<uint32_t>(file_data.method_wrapper_set.size()));
        AddUintToBuffer(&buffer, static_cast<uint32_t>(file_data.class_wrapper_set.size()));
        AddUintToBuffer(&buffer, file_data.checksum);
        AddStringToBuffer(&buffer, file_location);

        if (UNLIKELY(file_data.empty())) {
            LOG(INFO, RUNTIME) << "EMPTY FILE DATA, WERIED!";
        }

        for (auto method_it : file_data.method_wrapper_set) {
            AddUintToBuffer(&buffer, method_it.method_id_);
        }
        for (auto class_it : file_data.class_wrapper_set) {
            AddUintToBuffer(&buffer, class_it.class_id_);
        }
        ASSERT(required_capacity == buffer.size());
    }
    return WriteBuffer(fd, buffer.data(), buffer.size());
}

bool ProfileDumpInfo::Load(int fd)
{
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
    PandaString error;
    ProfileLoadSatus status = LoadInternal(fd, &error);

    if (status == PROFILE_LOAD_SUCCESS) {
        return true;
    }
    LOG(INFO, RUNTIME) << "Error when reading profile " << error;
    return false;
}

ProfileDumpInfo::ProfileLoadSatus ProfileDumpInfo::LoadInternal(int fd, PandaString *error)
{
    ASSERT(fd >= 0);
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);

    struct stat stat_buffer {
    };
    if (fstat(fd, &stat_buffer) != 0) {
        return PROFILE_LOAD_IO_ERROR;
    }

    if (stat_buffer.st_size == 0) {
        LOG(INFO, RUNTIME) << "empty file";
        return PROFILE_LOAD_EMPTYFILE;
    }

    uint32_t number_of_lines;
    ProfileLoadSatus status = ReadProfileHeader(fd, &number_of_lines, error);
    if (status != PROFILE_LOAD_SUCCESS) {
        return status;
    }
    LOG(INFO, RUNTIME) << "number of profile items = " << number_of_lines;

    while (number_of_lines > 0) {
        ProfileLineHeader line_header;
        status = ReadProfileLineHeader(fd, &line_header, error);
        if (status != PROFILE_LOAD_SUCCESS) {
            return status;
        }

        status = ReadProfileLine(fd, line_header, error);
        if (status != PROFILE_LOAD_SUCCESS) {
            return status;
        }
        number_of_lines--;
    }

    int result = testEOF(fd);
    if (result == 0) {
        return PROFILE_LOAD_SUCCESS;
    }

    if (result < 0) {
        return PROFILE_LOAD_IO_ERROR;
    }

    *error = "Unexpected content in the profile file";
    return PROFILE_LOAD_BAD_DATA;
}

ProfileDumpInfo::ProfileLoadSatus ProfileDumpInfo::ReadProfileHeader(int fd, uint32_t *number_of_lines,
                                                                     PandaString *error)
{
    const size_t K_MAGIC_VERSION_SIZE = sizeof(kProfileMagic) + sizeof(kProfileVersion) + sizeof(uint32_t);

    SerializerBuffer safe_buffer(K_MAGIC_VERSION_SIZE);

    ProfileLoadSatus status = safe_buffer.FillFromFd(fd, "ReadProfileHeader", error);
    if (status != PROFILE_LOAD_SUCCESS) {
        return status;
    }

    if (!safe_buffer.CompareAndAdvance(kProfileMagic, sizeof(kProfileMagic))) {
        *error = "Profile missing magic";
        return PROFILE_LOAD_VERSION_MISMATCH;
    }
    if (!safe_buffer.CompareAndAdvance(kProfileVersion, sizeof(kProfileVersion))) {
        *error = "Profile version mismatch";
        return PROFILE_LOAD_VERSION_MISMATCH;
    }

    *number_of_lines = safe_buffer.ReadUintAndAdvance<uint32_t>();
    return PROFILE_LOAD_SUCCESS;
}

ProfileDumpInfo::ProfileLoadSatus ProfileDumpInfo::ReadProfileLineHeader(int fd, ProfileLineHeader *line_header,
                                                                         PandaString *error)
{
    SerializerBuffer header_buffer(K_LINE_HEADER_SIZE);
    ProfileLoadSatus status = header_buffer.FillFromFd(fd, "ReadProfileLineHeader", error);
    if (status != PROFILE_LOAD_SUCCESS) {
        return status;
    }

    auto panda_location_size = header_buffer.ReadUintAndAdvance<uint16_t>();  // max chars in location, 4096 = 2 ^ 12
    line_header->method_set_size = header_buffer.ReadUintAndAdvance<uint32_t>();
    line_header->class_set_size = header_buffer.ReadUintAndAdvance<uint32_t>();
    line_header->checksum = header_buffer.ReadUintAndAdvance<uint32_t>();

    if (panda_location_size == 0 || panda_location_size > K_MAX_FILE_KEY_LENGTH) {
        *error = "PandaFileKey has an invalid size: " + std::to_string(panda_location_size);
        return PROFILE_LOAD_BAD_DATA;
    }

    SerializerBuffer location_buffer(panda_location_size);
    // Read the binary data: location string
    status = location_buffer.FillFromFd(fd, "ReadProfileLineHeader", error);
    if (status != PROFILE_LOAD_SUCCESS) {
        return status;
    }
    line_header->panda_file_location.assign(reinterpret_cast<char *>(location_buffer.Get()), panda_location_size);
    return PROFILE_LOAD_SUCCESS;
}

ProfileDumpInfo::ProfileLoadSatus ProfileDumpInfo::ReadProfileLine(int fd, const ProfileLineHeader &line_header,
                                                                   PandaString *error)
{
    static constexpr uint32_t K_MAX_NUMBER_OF_ENTRIES_TO_READ = 8000;  // ~8 kb
    uint32_t methods_left_to_read = line_header.method_set_size;
    uint32_t classes_left_to_read = line_header.class_set_size;

    while ((methods_left_to_read > 0) || (classes_left_to_read > 0)) {
        uint32_t methods_to_read = std::min(K_MAX_NUMBER_OF_ENTRIES_TO_READ, methods_left_to_read);
        uint32_t max_classes_to_read = K_MAX_NUMBER_OF_ENTRIES_TO_READ - methods_to_read;  // >=0
        uint32_t classes_to_read = std::min(max_classes_to_read, classes_left_to_read);

        size_t line_size = K_METHOD_BYTES * methods_to_read + K_CLASS_BYTES * classes_to_read;
        SerializerBuffer line_buffer(line_size);

        ProfileLoadSatus status = line_buffer.FillFromFd(fd, "ReadProfileLine", error);
        if (status != PROFILE_LOAD_SUCCESS) {
            return status;
        }
        if (!ProcessLine(line_buffer, methods_to_read, classes_to_read, line_header.checksum,
                         line_header.panda_file_location)) {
            *error = "Error when reading profile file line";
            return PROFILE_LOAD_BAD_DATA;
        }

        methods_left_to_read -= methods_to_read;
        classes_left_to_read -= classes_to_read;
    }
    return PROFILE_LOAD_SUCCESS;
}

// NOLINTNEXTLINE(google-runtime-references)
bool ProfileDumpInfo::ProcessLine(SerializerBuffer &line_buffer, uint32_t method_set_size, uint32_t class_set_size,
                                  uint32_t checksum, const PandaString &panda_file_location)
{
    for (uint32_t i = 0; i < method_set_size; i++) {
        // NB! Read the method info from buffer...
        auto method_idx = line_buffer.ReadUintAndAdvance<uint32_t>();
        if (!AddMethodWrapper(panda_file_location, checksum, MethodWrapper(method_idx))) {
            return false;
        }
    }

    for (uint32_t i = 0; i < class_set_size; i++) {
        auto class_def_idx = line_buffer.ReadUintAndAdvance<uint32_t>();
        if (!AddClassWrapper(panda_file_location, checksum, ClassWrapper(class_def_idx))) {
            return false;
        }
    }
    return true;
}

bool ProfileDumpInfo::MergeAndSave(const PandaString &filename, uint64_t *bytes_written, bool force)
{
    // NB! we using READWRITE mode to leave the creation job to framework layer.
    panda::os::unix::file::File myfile = panda::os::file::Open(filename, panda::os::file::Mode::READWRITE);
    if (!myfile.IsValid()) {
        LOG(ERROR, RUNTIME) << "Cannot open the profile file" << filename;
        return false;
    }
    panda::os::file::FileHolder fholder(myfile);
    int fd = myfile.GetFd();

    LOG(INFO, RUNTIME) << "  Step3.2: starting merging ***";
    PandaString error;
    ProfileDumpInfo file_dump_info;
    ProfileLoadSatus status = file_dump_info.LoadInternal(fd, &error);
    if (status == PROFILE_LOAD_SUCCESS || status == PROFILE_LOAD_EMPTYFILE) {
        if (MergeWith(file_dump_info)) {
            if (dump_info_ == file_dump_info.dump_info_) {
                if (bytes_written != nullptr) {
                    *bytes_written = 0;
                }
                LOG(INFO, RUNTIME) << "  No Saving as no change byte_written = 0";
                if (status != PROFILE_LOAD_EMPTYFILE) {
                    return true;
                }
            }
        } else {
            LOG(INFO, RUNTIME) << "  No Saving as Could not merge previous profile data from file " << filename;
            if (!force) {
                return false;
            }
        }
    } else if (force && ((status == PROFILE_LOAD_VERSION_MISMATCH) || (status == PROFILE_LOAD_BAD_DATA))) {
        LOG(INFO, RUNTIME) << "  Clearing bad or mismatch version profile data from file " << filename << ": " << error;
    } else {
        LOG(INFO, RUNTIME) << "  No Saving as Could not load profile data from file " << filename << ": " << error;
        return false;
    }

    LOG(INFO, RUNTIME) << "  Step3.3: starting Saving ***";
    LOG(INFO, RUNTIME) << "      clear file data firstly";
    if (!myfile.ClearData()) {
        LOG(INFO, RUNTIME) << "Could not clear profile file: " << filename;
        return false;
    }

    bool result = Save(fd);
    if (result) {
        if (bytes_written != nullptr) {
            LOG(INFO, RUNTIME) << "      Profile Saver Bingo! and bytes written = " << bytes_written;
            *bytes_written = GetFileSizeBytes(filename);
        }
    } else {
        LOG(ERROR, RUNTIME) << "Failed to save profile info to " << filename;
    }
    return result;
}

}  // namespace panda
