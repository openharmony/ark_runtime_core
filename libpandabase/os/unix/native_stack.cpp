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

#include "os/unix/native_stack.h"

#include "os/file.h"
#include <regex>
#include <dirent.h>
#include <mem/mem.h>

namespace panda::os::unix::native_stack {

static constexpr int MOVE_2 = 2;      // delete kernel stack's prefix
static constexpr int STACK_TYPE = 2;  // for call ANR type
static constexpr int FIND_TID = 10;   // decimal number

std::string GetNativeThreadNameForFile(pid_t tid)
{
    std::string result = "<unknown>";
    std::ostringstream comm_file;
    comm_file << "/proc/self/task/" << tid << "/comm";
    if (native_stack::ReadOsFile(comm_file.str(), &result)) {
        result.resize(result.size() - 1);
    }
    return result;
}

std::string BuildNumber(int count)
{
    std::ostringstream ostr;
    constexpr int STACK_FORMAT = 10;
    if (count >= 0 && count < STACK_FORMAT) {
        ostr << "#0" << count;
    }
    if (STACK_FORMAT <= count) {
        ostr << "#" << count;
    }
    return ostr.str();
}

void DumpKernelStack(std::ostream &os, pid_t tid, const char *tag, bool count)
{
    if (tid == static_cast<pid_t>(thread::GetCurrentThreadId())) {
        return;
    }
    std::string kernel_stack;
    std::ostringstream stack_file;
    stack_file << "/proc/self/task/" << tid << "/stack";
    if (!native_stack::ReadOsFile(stack_file.str(), &kernel_stack)) {
        os << tag << "(couldn't read " << stack_file.str() << ")\n";
        return;
    }

    std::regex split("\n");
    std::vector<std::string> kernel_stack_frames(
        std::sregex_token_iterator(kernel_stack.begin(), kernel_stack.end(), split, -1), std::sregex_token_iterator());

    if (!kernel_stack_frames.empty()) {
        kernel_stack_frames.pop_back();
    }

    for (size_t i = 0; i < kernel_stack_frames.size(); ++i) {
        const char *kernel_stack_build = kernel_stack_frames[i].c_str();
        // change the stack string, case:
        // kernel stack in linux file is : "[<0>] do_syscall_64+0x73/0x130"
        // kernel stack in ANR file is : "do_syscall_64+0x73/0x130"
        const char *remove_bracket = strchr(kernel_stack_build, ']');
        if (remove_bracket != nullptr) {
            kernel_stack_build = remove_bracket + MOVE_2;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // if need count, case: "do_syscall_64+0x73/0x130" --> "#00 do_syscall_64+0x73/0x130"
        os << tag;
        if (count) {
            os << BuildNumber(i);
        }
        os << kernel_stack_build << std::endl;
    }
}

void DumpUnattachedThread::AddTid(pid_t tid_thread)
{
    thread_manager_tids_.insert(tid_thread);
}

bool DumpUnattachedThread::InitKernelTidLists()
{
    kernel_tid_.clear();
    DIR *task = opendir("/proc/self/task");
    if (task == nullptr) {
        return false;
    }

    dirent *dir = nullptr;
    while ((dir = readdir(task)) != nullptr) {
        char *dir_end = nullptr;
        pid_t tid = strtol(dir->d_name, &dir_end, FIND_TID);
        if (*dir_end == 0) {
            kernel_tid_.insert(tid);
        }
    }
    closedir(task);
    return true;
}

void DumpUnattachedThread::Dump(std::ostream &os, bool dump_native_crash, FUNC_UNWINDSTACK call_unwindstack)
{
    std::set<pid_t> dump_tid;
    set_difference(kernel_tid_.begin(), kernel_tid_.end(), thread_manager_tids_.begin(), thread_manager_tids_.end(),
                   inserter(dump_tid, dump_tid.begin()));
    std::set<int>::iterator tid;
    for (tid = dump_tid.begin(); tid != dump_tid.end(); ++tid) {
        // thread_manager tid may be invalid. Check again
        if (kernel_tid_.count(*tid) == 0) {
            continue;
        }
        int priority = thread::GetPriority(*tid);
        os << '"' << GetNativeThreadNameForFile(*tid) << '"' << " prio=" << priority << " (not attached)\n";
        os << "  | sysTid=" << *tid << " nice=" << priority << "\n";
        DumpKernelStack(os, *tid, "  kernel: ", false);

        if (dump_native_crash && (call_unwindstack != nullptr)) {
            call_unwindstack(*tid, os, STACK_TYPE);
        }
        os << "\n";
    }
}

bool ReadOsFile(const std::string &file_name, std::string *result)
{
    panda::os::unix::file::File cmdfile = panda::os::file::Open(file_name, panda::os::file::Mode::READONLY);
    panda::os::file::FileHolder fholder(cmdfile);
    constexpr size_t BUFF_SIZE = 8_KB;
    std::vector<char> buffer(BUFF_SIZE);
    auto res = cmdfile.Read(&buffer[0], buffer.size());
    if (res) {
        result->append(&buffer[0], res.Value());
        return true;
    }
    return false;
}

bool WriterOsFile(const void *buffer, size_t count, int fd)
{
    panda::os::unix::file::File myfile(fd);
    panda::os::file::FileHolder fholder(myfile);
    return myfile.WriteAll(buffer, count);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
std::string ChangeJaveStackFormat(const char *descriptor)
{
    if (descriptor == nullptr || strlen(descriptor) < 1) {
        LOG(ERROR, RUNTIME) << "Invalid descriptor";
        return "";
    }

    if (descriptor[0] == 'L') {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::string str(descriptor);
        size_t end = str.find_last_of(';');
        if (end == std::string::npos) {
            LOG(ERROR, RUNTIME) << "Invalid descriptor: no scln at end";
            return "";
        }
        std::string java_name = str.substr(1, end - 1);  // Remove 'L' and ';'
        std::replace(java_name.begin(), java_name.end(), '/', '.');
        return java_name;
    }

    if (descriptor[0] == '[') {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::string java_name(descriptor);
        std::replace(java_name.begin(), java_name.end(), '/', '.');
        return java_name;
    }

    const char *primitive_name = "";
    switch (descriptor[0]) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        case 'Z':
            primitive_name = "boolean";
            break;
        case 'B':
            primitive_name = "byte";
            break;
        case 'C':
            primitive_name = "char";
            break;
        case 'S':
            primitive_name = "short";
            break;
        case 'I':
            primitive_name = "int";
            break;
        case 'J':
            primitive_name = "long";
            break;
        case 'F':
            primitive_name = "float";
            break;
        case 'D':
            primitive_name = "double";
            break;
        case 'V':
            primitive_name = "void";
            break;
        default:
            break;
    }

    return primitive_name;
}

}  // namespace panda::os::unix::native_stack
