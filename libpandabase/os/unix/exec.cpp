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

#include "os/exec.h"

#include <cstring>
#include <unistd.h>
#include "os/unix/failure_retry.h"
#include "sys/wait.h"

namespace panda::os::exec {

Expected<int, Error> Exec(Span<const char *> args)
{
    ASSERT(!args.Empty());
    ASSERT(args[args.Size() - 1] == nullptr && "The last argument must be nullptr");

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        execv(args[0], const_cast<char **>(args.Data()));
        _exit(1);
    }

    if (pid < 0) {
        return Unexpected(Error(errno));
    }

    int status = -1;
    pid_t res_pid = PANDA_FAILURE_RETRY(waitpid(pid, &status, 0));
    if (res_pid != pid) {
        return Unexpected(Error(errno));
    }
    if (WIFEXITED(status)) {         // NOLINT(hicpp-signed-bitwise)
        return WEXITSTATUS(status);  // NOLINT(hicpp-signed-bitwise)
    }
    return Unexpected(Error("Process finished improperly"));
}

}  // namespace panda::os::exec
