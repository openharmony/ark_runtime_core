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

#include "include/runtime.h"
#include "include/thread.h"
#include "include/thread_scopes.h"
#include "runtime/include/locks.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/class.h"
#include "utils/pandargs.h"
#include "mem/mem_stats.h"
#include "libpandabase/os/mutex.h"
#include "libpandabase/os/native_stack.h"
#include "libpandafile/class_data_accessor.h"

#include "verification/job_queue/job_queue.h"
#include "verification/cache/results_cache.h"
#include "generated/base_options.h"

#include "utils/span.h"

#include "utils/logger.h"

#include <limits>
#include <iostream>
#include <vector>
#include <chrono>
#include <ctime>
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)

namespace panda {

const panda_file::File *GetPandaFile(ClassLinker *class_linker, std::string_view filename)
{
    const panda_file::File *res = nullptr;
    class_linker->EnumerateBootPandaFiles([&res, filename](const panda_file::File &pf) {
        if (pf.GetFilename() == filename) {
            res = &pf;
            return false;
        }
        return true;
    });
    return res;
}

bool VerifierProcessFile(const std::string &filename)
{
    bool result = true;
    auto &runtime = *Runtime::GetCurrent();
    auto &class_linker = *runtime.GetClassLinker();

    auto file = panda_file::OpenPandaFile(filename);
    if (UNLIKELY(file == nullptr)) {
        return false;
    }

    LanguageContext ctx = runtime.GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    bool is_default_context = true;

    for (auto id : file->GetClasses()) {
        Class *klass = nullptr;
        auto class_id = panda_file::File::EntityId {id};
        if (!file->IsExternal(class_id)) {
            panda_file::ClassDataAccessor cda(*file, class_id);
            ctx = runtime.GetLanguageContext(&cda);
        }
        {
            // we need AccessToManagedObjectsScope for GetClass since it can allocate objects
            ScopedManagedCodeThread sj(MTManagedThread::GetCurrent());
            klass = class_linker.GetExtension(ctx)->GetClass(*file, class_id);
        }

        if (klass != nullptr) {
            auto *panda_file = klass->GetPandaFile();
            bool is_system_class = panda_file == nullptr || verifier::JobQueue::IsSystemFile(panda_file);
            if (is_system_class) {
                continue;
            }
            if (is_default_context) {
                ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
                is_default_context = true;
            }
            for (auto &method : klass->GetMethods()) {
                // we need AccessToManagedObjectsScope for Verify() since it can allocate objects
                ScopedManagedCodeThread sj(MTManagedThread::GetCurrent());
                result = method.Verify();
                if (!result) {
                    return result;
                }
            }
        }
    }

    return result;
}

void BlockSignals()
{
#if defined(PANDA_TARGET_UNIX)
    sigset_t set;
    if (sigemptyset(&set) == -1) {
        LOG(ERROR, RUNTIME) << "sigemptyset failed";
        return;
    }
#ifdef PANDA_TARGET_MOBILE
    int rc = 0;
    rc += sigaddset(&set, SIGPIPE);
    rc += sigaddset(&set, SIGQUIT);
    rc += sigaddset(&set, SIGUSR1);
    rc += sigaddset(&set, SIGUSR2);
    if (rc < 0) {
        LOG(ERROR, RUNTIME) << "sigaddset failed";
        return;
    }
#endif  // PANDA_TARGET_MOBILE

    if (os::native_stack::g_PandaThreadSigmask(SIG_BLOCK, &set, nullptr) != 0) {
        LOG(ERROR, RUNTIME) << "PandaThreadSigmask failed";
    }
#endif  // PANDA_TARGET_UNIX
}

#ifdef ENABLE_VERIFY
int Main(int argc, const char **argv)
{
    BlockSignals();
    Span<const char *> sp(argv, argc);
    RuntimeOptions runtime_options(sp[0]);

    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<bool> options("options", false, "Print compiler and runtime options");
    // tail arguments
    panda::PandArg<std::string> file("file", "", "path to pandafile");
    panda::PandArgParser pa_parser;

    runtime_options.AddOptions(&pa_parser);

    pa_parser.Add(&help);
    pa_parser.Add(&options);
    pa_parser.PushBackTail(&file);
    pa_parser.EnableTail();
    pa_parser.EnableRemainder();
    pa_parser.Parse(argc, argv);

    auto boot_panda_files = runtime_options.GetBootPandaFiles();
    boot_panda_files.push_back(file.GetValue());
    runtime_options.SetBootPandaFiles(boot_panda_files);

    if (!Runtime::Create(runtime_options)) {
        return -1;
    }

    int ret = 0;
    if (options.GetValue()) {
        std::cout << pa_parser.GetRegularArgs() << std::endl;
    }

    std::string filename = file.GetValue();
    bool result = VerifierProcessFile(filename);
    if (!result) {
        ret = -1;
    }

    Runtime::GetCurrent()->GetVerificationOptions().Cache.UpdateOnExit = false;
    if (!Runtime::Destroy()) {
        return -1;
    }
    pa_parser.DisableTail();
    return ret;
}
#else
int Main([[maybe_unused]] int argc, [[maybe_unused]] const char **argv)
{
    BlockSignals();
    return 0;
}
#endif  // ENABLE_VERIFY

}  // namespace panda

int main(int argc, const char **argv)
{
    return panda::Main(argc, argv);
}
