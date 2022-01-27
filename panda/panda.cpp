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

#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <vector>

#include "include/runtime.h"
#include "include/thread.h"
#include "include/thread_scopes.h"

#include "libpandabase/os/mutex.h"
#include "libpandabase/os/native_stack.h"
#include "generated/base_options.h"

#include "mem/mem_stats.h"

#include "runtime/include/locks.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/class.h"

#include "utils/logger.h"
#include "utils/pandargs.h"
#include "utils/span.h"

#include "verification/job_queue/job_queue.h"
#include "verification/job_queue/cache.h"

namespace panda {
const panda_file::File *GetPandaFile(const ClassLinker &class_linker, std::string_view file_name)
{
    const panda_file::File *res = nullptr;
    class_linker.EnumerateBootPandaFiles([&res, file_name](const panda_file::File &pf) {
        if (pf.GetFilename() == file_name) {
            res = &pf;
            return false;
        }
        return true;
    });
    return res;
}

bool VerifierProcessFile(const panda::verifier::VerificationOptions &opts, const std::string &file_name,
                         const std::string &entrypoint)
{
    if (!opts.Mode.OnlyVerify) {
        return true;
    }

    auto &runtime = *Runtime::GetCurrent();
    auto &class_linker = *runtime.GetClassLinker();

    bool result = true;
    if (opts.Mode.VerifyAllRuntimeLibraryMethods) {
        // We need AccessToManagedObjectsScope for verification since it can allocate objects
        ScopedManagedCodeThread managed_obj_thread(MTManagedThread::GetCurrent());
        class_linker.EnumerateClasses([&result](const Class *klass) {
            for (auto &method : klass->GetMethods()) {
                result = method.Verify();
                if (!result) {
                    return false;
                }
            }
            return true;
        });
    }
    if (!result) {
        return false;
    }

    if (opts.Mode.VerifyOnlyEntryPoint) {
        auto resolved = runtime.ResolveEntryPoint(entrypoint);

        result = static_cast<bool>(resolved);

        if (!result) {
            LOG(ERROR, VERIFIER) << "Error: Cannot resolve method '" << entrypoint << "'";
        } else {
            // We need AccessToManagedObjectsScope for verification since it can allocate objects
            ScopedManagedCodeThread managed_obj_thread(MTManagedThread::GetCurrent());
            Method &method = *resolved.Value();
            result = method.Verify();
        }
    } else {
        auto file = GetPandaFile(*runtime.GetClassLinker(), file_name);
        ASSERT(file != nullptr);

        auto &klass_linker = *runtime.GetClassLinker();

        auto extracted = Runtime::GetCurrent()->ExtractLanguageContext(file, entrypoint);
        result = static_cast<bool>(extracted);
        if (!result) {
            LOG(ERROR, VERIFIER) << "Error: Cannot extract language context for entry point: " << entrypoint;
            return false;
        }

        LanguageContext ctx = extracted.Value();
        bool is_default_context = true;

        for (auto id : file->GetClasses()) {
            Class *klass = nullptr;
            {
                // We need AccessToManagedObjectsScope for GetClass since it can allocate objects
                ScopedManagedCodeThread managed_obj_thread(MTManagedThread::GetCurrent());
                klass = klass_linker.GetExtension(ctx)->GetClass(*file, panda_file::File::EntityId {id});
            }

            if (klass != nullptr) {
                if (is_default_context) {
                    ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
                    is_default_context = true;
                }
                for (auto &method : klass->GetMethods()) {
                    // We need AccessToManagedObjectsScope for verify since it can allocate objects
                    ScopedManagedCodeThread managed_obj_thread(MTManagedThread::GetCurrent());
                    result = method.Verify();
                    if (!result) {
                        break;
                    }
                }
            }
            if (!result) {
                break;
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

int Main(const int argc, const char **argv)
{
    auto start_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    BlockSignals();
    Span<const char *> sp(argv, argc);
    RuntimeOptions runtime_options(sp[0]);
    base_options::Options base_options(sp[0]);

    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<bool> options("options", false, "Print compiler and runtime options");
    // Tail arguments
    panda::PandArg<std::string> file("file", "", "path to pandafile");
    panda::PandArg<std::string> entrypoint("entrypoint", "", "full name of entrypoint function or method");
    panda::PandArgParser pa_parser;

    runtime_options.AddOptions(&pa_parser);
    base_options.AddOptions(&pa_parser);

    pa_parser.Add(&help);
    pa_parser.Add(&options);
    pa_parser.PushBackTail(&file);
    pa_parser.PushBackTail(&entrypoint);
    pa_parser.EnableTail();
    pa_parser.EnableRemainder();

    if (!pa_parser.Parse(argc, argv) || file.GetValue().empty() || entrypoint.GetValue().empty() || help.GetValue()) {
        std::cerr << pa_parser.GetErrorString() << std::endl;
        std::cerr << "Usage: "
                  << "panda"
                  << " [OPTIONS] [file] [entrypoint] -- [arguments]" << std::endl;
        std::cerr << std::endl;
        std::cerr << "optional arguments:" << std::endl;
        std::cerr << pa_parser.GetHelpString() << std::endl;
        return 1;
    }

    Logger::Initialize(base_options);

    arg_list_t arguments = pa_parser.GetRemainder();

    if (runtime_options.IsStartupTime()) {
        std::cout << "\n"
                  << "Startup start time: " << start_time << std::endl;
    }

    auto runtime_options_err = runtime_options.Validate();
    if (runtime_options_err) {
        std::cerr << "Error: " << runtime_options_err.value().GetMessage() << std::endl;
        return 1;
    }

    auto boot_panda_files = runtime_options.GetBootPandaFiles();

    if (runtime_options.GetPandaFiles().empty()) {
        boot_panda_files.push_back(file.GetValue());
    } else {
        auto panda_files = runtime_options.GetPandaFiles();
        auto found_iter = std::find_if(panda_files.begin(), panda_files.end(),
                                       [&](auto &file_name) { return file_name == file.GetValue(); });
        if (found_iter == panda_files.end()) {
            panda_files.push_back(file.GetValue());
            runtime_options.SetPandaFiles(panda_files);
        }
    }

    runtime_options.SetBootPandaFiles(boot_panda_files);

    if (!Runtime::Create(runtime_options)) {
        std::cerr << "Error: cannot create runtime" << std::endl;
        return -1;
    }

    if (options.GetValue()) {
        std::cout << pa_parser.GetRegularArgs() << std::endl;
    }

    std::string file_name = file.GetValue();
    std::string entry = entrypoint.GetValue();

    auto &runtime = *Runtime::GetCurrent();
    auto &verif_opts = runtime.GetVerificationOptions();

    int ret = 0;

    if (verif_opts.Enable) {
        runtime.GetClassLinker()->EnumerateBootPandaFiles([](const panda_file::File &pf) {
            verifier::JobQueue::GetCache().FastAPI().ProcessFile(&pf);
            return true;
        });
        bool result = VerifierProcessFile(verif_opts, file_name, entry);
        if (!result && !verif_opts.Mode.VerifierDoesNotFail) {
            ret = -1;
        }
    }

    if (ret == 0 && (!verif_opts.Enable || !verif_opts.Mode.OnlyVerify)) {
        auto res = runtime.ExecutePandaFile(file_name, entry, arguments);
        if (!res) {
            std::cerr << "Cannot execute panda file '" << file_name << "' with entry '" << entry << "'" << std::endl;
            ret = -1;
        } else {
            ret = res.Value();
        }
    }
    if (runtime_options.IsPrintMemoryStatistics()) {
        std::cout << Runtime::GetCurrent()->GetMemoryStatistics();
    }
    if (runtime_options.IsPrintGcStatistics()) {
        std::cout << Runtime::GetCurrent()->GetFinalStatistics();
    }
    if (!Runtime::Destroy()) {
        std::cerr << "Error: cannot destroy runtime" << std::endl;
        return -1;
    }
    pa_parser.DisableTail();
    return ret;
}
}  // namespace panda

int main(int argc, const char **argv)
{
    return panda::Main(argc, argv);
}
