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

#ifndef PANDA_RUNTIME_PROFILESAVER_PROFILE_SAVER_H_
#define PANDA_RUNTIME_PROFILESAVER_PROFILE_SAVER_H_

#include <pthread.h>

#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "libpandabase/macros.h"
#include "libpandabase/os/mutex.h"
#include "libpandabase/utils/logger.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/profilesaver/profile_dump_info.h"

namespace panda {

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects, readability-identifier-naming)
static os::memory::Mutex profile_saver_lock_;

/*
 * Note well! we take singleton pattern, multi-threading scenario should be taken more serious considerations.
 *
 * e.g. Global Mutex Management or corresponding alternatives should better than
 * global mutex profile_saver_lock_.
 *
 */

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class ProfileSaver {
public:
    /*
     * start the profile saver deamon thread
     *
     * output_filename records the profile name, code_paths stores all the locations contain pandafile(aka *.aex)
     * app_data_dir contains the location of application package
     */
    static void Start(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                      const PandaString &app_data_dir);

    /*
     * stop the profile saver deamon thread.
     *
     * if dump_info == true, dumps the debug information
     */
    static void Stop(bool dump_info);

    /*
     * whether profile saver instance exists.
     */
    static bool IsStarted();

    ~ProfileSaver() = default;

private:
    ProfileSaver(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                 const PandaString &app_dir);

    void AddTrackedLocations(const PandaString &output_filename, const PandaVector<PandaString> &code_paths,
                             const PandaString &app_data_dir);

    /*
     * Callback for pthread_create, we attach/detach this thread to Runtime here
     */
    static void *RunProfileSaverThread(void *arg);

    /*
     * Run loop for the saver.
     */
    void Run();

    /*
     * Returns true if the saver is shutting down (ProfileSaver::Stop() has been called, will set this flag).
     */
    bool ShuttingDown();

    /*
     * Dump functions, we leave it stub and for test until now.
     */
    void DumpInfo();

    /*
     * Fetches the current resolved classes and methods from the ClassLinker and stores them in the profile_cache_.
     */
    void TranverseAndCacheResolvedClassAndMethods();

    /*
     * Retrieves the cached ProfileDumpInfo for the given profile filename.
     * If no entry exists, a new empty one will be created, added to the cache and then returned.
     */
    ProfileDumpInfo *GetOrAddCachedProfiledInfo(const PandaString &filename);

    /*
     * Processes the existing profiling info from the jit code cache(if exists) and returns
     * true if it needed to be saved back to disk.
     */
    void MergeAndDumpProfileData();

    static ProfileSaver *instance;

    static std::thread profiler_saver_daemon_thread;

    PandaMap<PandaString, PandaSet<PandaString>> tracked_pandafile_base_locations_;

    PandaMap<PandaString, ProfileDumpInfo> profile_cache_;

    PandaSet<PandaString> app_data_dirs_;

    bool shutting_down_ GUARDED_BY(profile_saver_lock_);

    struct CntStats {
    public:
        uint64_t GetMethodCount()
        {
            return last_save_number_of_methods_;
        }

        void SetMethodCount(uint64_t methodcnt)
        {
            last_save_number_of_methods_ = methodcnt;
        }

        uint64_t GetClassCount()
        {
            return last_save_number_of_classes_;
        }

        void SetClassCount(uint64_t classcnt)
        {
            last_save_number_of_classes_ = classcnt;
        }

    private:
        uint64_t last_save_number_of_methods_ {0};
        uint64_t last_save_number_of_classes_ {0};
    };

    PandaMap<PandaString, CntStats> statcache;  // NOLINT(readability-identifier-naming)

    /*
     * Retrieves the cached CntStats for the given profile filename.
     * If no entry exists, a new empty one will be created, added to the cache and then returned.
     */
    CntStats *GetOrAddCachedProfiledStatsInfo(const PandaString &filename);

    NO_COPY_SEMANTIC(ProfileSaver);
    NO_MOVE_SEMANTIC(ProfileSaver);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_PROFILESAVER_PROFILE_SAVER_H_
