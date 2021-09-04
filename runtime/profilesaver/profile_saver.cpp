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

#include "runtime/profilesaver/profile_saver.h"

#include <cerrno>
#include <chrono>
#include <thread>

#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"
#include "runtime/jit/jit.h"
#include "trace/trace.h"

namespace panda {

ProfileSaver *ProfileSaver::instance = nullptr;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
std::thread ProfileSaver::profiler_saver_daemon_thread;

static bool CheckLocationForCompilation(const PandaString &location)
{
    return !location.empty();
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ProfileSaver::ProfileSaver(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                           const PandaString &app_dir)
    : shutting_down_(false)
{
    AddTrackedLocations(output_filename, code_paths, app_dir);
}

// NB! it is the caller's responsibility to pass suitable output_filename, code_paths as well as app_data_dir.
void ProfileSaver::AddTrackedLocations(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                                       const PandaString &app_data_dir)
{
    auto it = tracked_pandafile_base_locations_.find(output_filename);
    if (it == tracked_pandafile_base_locations_.end()) {
        tracked_pandafile_base_locations_.insert(
            std::make_pair(output_filename, PandaSet<PandaString>(code_paths.begin(), code_paths.end())));
        if (!app_data_dir.empty()) {
            app_data_dirs_.insert(app_data_dir);
        }
    } else {
        if (UNLIKELY(app_data_dirs_.count(app_data_dir) <= 0)) {
            LOG(INFO, RUNTIME) << "Cannot find app dir, bad output filename";
            return;
        }
        it->second.insert(code_paths.begin(), code_paths.end());
    }
}

void ProfileSaver::Start(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                         const PandaString &app_data_dir)
{
    if (Runtime::GetCurrent() == nullptr) {
        LOG(ERROR, RUNTIME) << "Runtime is nullptr";
        return;
    }

    if (!Runtime::GetCurrent()->SaveProfileInfo()) {
        LOG(ERROR, RUNTIME) << "ProfileSaver is forbidden";
        return;
    }

    if (output_filename.empty()) {
        LOG(ERROR, RUNTIME) << "Invalid output filename";
        return;
    }

    PandaVector<PandaString> code_paths_to_profile;
    for (const PandaString &location : code_paths) {
        if (CheckLocationForCompilation(location)) {
            code_paths_to_profile.push_back(location);
        }
    }

    if (code_paths_to_profile.empty()) {
        LOG(INFO, RUNTIME) << "No code paths should be profiled.";
        return;
    }

    if (UNLIKELY(instance != nullptr)) {
        LOG(INFO, RUNTIME) << "Profile Saver Singleton already exists";
        instance->AddTrackedLocations(output_filename, code_paths_to_profile, app_data_dir);
        return;
    }

    LOG(INFO, RUNTIME) << "Starting dumping profile saver output file" << output_filename;

    instance = new ProfileSaver(output_filename, code_paths_to_profile, app_data_dir);
    profiler_saver_daemon_thread = std::thread(ProfileSaver::RunProfileSaverThread, reinterpret_cast<void *>(instance));
}

void ProfileSaver::Stop(bool dump_info)
{
    {
        os::memory::LockHolder lock(profile_saver_lock_);

        if (instance == nullptr) {
            LOG(ERROR, RUNTIME) << "Tried to stop a profile saver which was not started";
            return;
        }

        if (instance->shutting_down_) {
            LOG(ERROR, RUNTIME) << "Tried to stop the profile saver twice";
            return;
        }

        instance->shutting_down_ = true;

        if (dump_info) {
            instance->DumpInfo();
        }
    }

    // Wait for the saver thread to stop.
    // NB! we must release profile_saver_lock_ here.
    profiler_saver_daemon_thread.join();

    // Kill the singleton ...
    delete instance;
    instance = nullptr;
}

bool ProfileSaver::IsStarted()
{
    return instance != nullptr;
}

void ProfileSaver::DumpInfo()
{
    LOG(INFO, RUNTIME) << "ProfileSaver stopped" << '\n';
}

void *ProfileSaver::RunProfileSaverThread(void *arg)
{
    // NOLINTNEXTLINE(hicpp-use-auto, modernize-use-auto)
    ProfileSaver *profile_saver = reinterpret_cast<ProfileSaver *>(arg);
    profile_saver->Run();

    LOG(INFO, RUNTIME) << "Profile saver shutdown";
    return nullptr;
}

void ProfileSaver::Run()
{
    while (!ShuttingDown()) {
        LOG(INFO, RUNTIME) << "Step1: Time Sleeping >>>>>>> ";
        uint32_t sleepytime = Runtime::GetOptions().GetProfilesaverSleepingTimeMs();
        constexpr uint32_t TIME_SECONDS_IN_MS = 1000;
        for (int i = 0; i < static_cast<int>(sleepytime / TIME_SECONDS_IN_MS); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (ShuttingDown()) {
                break;
            }
        }

        LOG(INFO, RUNTIME) << "Step2: tranverse the resolved class and methods >>>>>>> ";
        TranverseAndCacheResolvedClassAndMethods();

        LOG(INFO, RUNTIME) << "Step3: merge current profile file and save it back >>>>>>> ";
        MergeAndDumpProfileData();
    }
}

bool ProfileSaver::ShuttingDown()
{
    os::memory::LockHolder lock(profile_saver_lock_);
    return shutting_down_;
}

void ProfileSaver::TranverseAndCacheResolvedClassAndMethods()
{
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
    PandaSet<ExtractedResolvedClasses> resolved_classes;
    PandaVector<ExtractedMethod> methods;
    auto call_back = [&resolved_classes, &methods](Class *klass) -> bool {
        const panda_file::File *pandafile = klass->GetPandaFile();
        panda_file::File::EntityId classfieldid = klass->GetFileId();

        if (pandafile == nullptr) {
            LOG(INFO, RUNTIME) << "panda file is nullptr";
            return false;
        }
        LOG(INFO, RUNTIME) << "      pandafile name = " << pandafile->GetFilename()
                           << " classname = " << klass->GetName();

        Span<Method> tmp_methods = klass->GetMethods();
        LOG(INFO, RUNTIME) << "      methods size = " << tmp_methods.size();
        for (int i = 0; i < static_cast<int>(tmp_methods.Size()); ++i) {
            Method &method = tmp_methods[i];
            if (!method.IsNative()) {
                if (method.GetHotnessCounter() >= K_MIN_PROFILE_THRESHOLD) {
                    ASSERT(method.GetPandaFile() != nullptr);
                    LOG(INFO, RUNTIME) << "      method pandafile name = " << method.GetPandaFile()->GetFilename();
                    methods.emplace_back(ExtractedMethod(method.GetPandaFile(), method.GetFileId()));
                }
            }
        }

        ExtractedResolvedClasses tmp_resolved_classes(ConvertToString(pandafile->GetFilename()),
                                                      pandafile->GetHeader()->checksum);
        LOG(INFO, RUNTIME) << "      Add class " << klass->GetName();
        auto it = resolved_classes.find(tmp_resolved_classes);
        if (it != resolved_classes.end()) {
            it->AddClass(classfieldid.GetOffset());
        } else {
            tmp_resolved_classes.AddClass(classfieldid.GetOffset());
            resolved_classes.insert(tmp_resolved_classes);
        }

        return true;
    };

    // NB: classliner == nullptr
    if (Runtime::GetCurrent()->GetClassLinker() == nullptr) {
        LOG(INFO, RUNTIME) << "classliner is nullptr";
        return;
    }

    LOG(INFO, RUNTIME) << "  Step2.1: tranverse the resolved class and methods";
    Runtime::GetCurrent()->GetClassLinker()->EnumerateClasses(call_back);
    LOG(INFO, RUNTIME) << "  Step2.2: starting tracking all the pandafile locations and flush the cache";

    for (const auto &it : tracked_pandafile_base_locations_) {
        const PandaString &filename = it.first;
        const PandaSet<PandaString> &locations = it.second;

        PandaSet<ExtractedResolvedClasses> resolved_classes_for_location;
        PandaVector<ExtractedMethod> methods_for_location;

        LOG(INFO, RUNTIME) << "      all the locations are:";
        for (auto const &iter : locations) {
            LOG(INFO, RUNTIME) << iter << " ";
        }

        LOG(INFO, RUNTIME) << "      Methods name : ";
        for (const ExtractedMethod &ref : methods) {
            LOG(INFO, RUNTIME) << "      " << ref.panda_file_->GetFilename();
            if (locations.find(ConvertToString(ref.panda_file_->GetFilename())) != locations.end()) {
                LOG(INFO, RUNTIME) << "      bingo method!";
                methods_for_location.push_back(ref);
            }
        }
        LOG(INFO, RUNTIME) << std::endl;
        LOG(INFO, RUNTIME) << "      Classes name";

        for (const ExtractedResolvedClasses &classes : resolved_classes) {
            LOG(INFO, RUNTIME) << "      " << classes.GetPandaFileLocation();
            if (locations.find(classes.GetPandaFileLocation()) != locations.end()) {
                LOG(INFO, RUNTIME) << "      bingo class!";
                resolved_classes_for_location.insert(classes);
            }
        }

        ProfileDumpInfo *info = GetOrAddCachedProfiledInfo(filename);
        LOG(INFO, RUNTIME) << "      Adding Bingo Methods and Classes";
        info->AddMethodsAndClasses(methods_for_location, resolved_classes_for_location);
    }
}

ProfileDumpInfo *ProfileSaver::GetOrAddCachedProfiledInfo(const PandaString &filename)
{
    auto info_it = profile_cache_.find(filename);
    if (info_it == profile_cache_.end()) {
        LOG(INFO, RUNTIME) << "      bingo profile_cache_!";
        auto ret = profile_cache_.insert(std::make_pair(filename, ProfileDumpInfo()));
        ASSERT(ret.second);
        info_it = ret.first;
    }
    return &(info_it->second);
}

ProfileSaver::CntStats *ProfileSaver::GetOrAddCachedProfiledStatsInfo(const PandaString &filename)
{
    auto info_it = statcache.find(filename);
    if (info_it == statcache.end()) {
        LOG(INFO, RUNTIME) << "      bingo StatsInfo_cache_!";
        auto ret = statcache.insert(std::make_pair(filename, CntStats()));
        ASSERT(ret.second);
        info_it = ret.first;
    }
    return &(info_it->second);
}

void ProfileSaver::MergeAndDumpProfileData()
{
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
    for (const auto &it : tracked_pandafile_base_locations_) {
        if (ShuttingDown()) {
            return;
        }
        const PandaString &filename = it.first;
        LOG(INFO, RUNTIME) << "  Step3.1 starting merging and save the following file ***";
        LOG(INFO, RUNTIME) << "      filename = " << filename;

        ProfileDumpInfo *cached_info = GetOrAddCachedProfiledInfo(filename);
        CntStats *cached_stat = GetOrAddCachedProfiledStatsInfo(filename);
        ASSERT(cached_info->GetNumberOfMethods() >= cached_stat->GetMethodCount());
        ASSERT(cached_info->GetNumberOfResolvedClasses() >= cached_stat->GetClassCount());
        uint64_t delta_number_of_methods = cached_info->GetNumberOfMethods() - cached_stat->GetMethodCount();
        uint64_t delta_number_of_classes = cached_info->GetNumberOfResolvedClasses() - cached_stat->GetClassCount();
        uint64_t numthreshold = Runtime::GetOptions().GetProfilesaverDeltaNumberThreshold();
        if (delta_number_of_methods < numthreshold && delta_number_of_classes < numthreshold) {
            LOG(INFO, RUNTIME) << "      number of delta number/class not enough";
            continue;
        }

        uint64_t bytes_written;
        if (cached_info->MergeAndSave(filename, &bytes_written, true)) {
            cached_stat->SetMethodCount(cached_info->GetNumberOfMethods());
            cached_stat->SetClassCount(cached_info->GetNumberOfResolvedClasses());
        } else {
            LOG(INFO, RUNTIME) << "Could not save profiling info to " << filename;
        }
    }
}

}  // namespace panda
