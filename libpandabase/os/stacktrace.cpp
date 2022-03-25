/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <unwind.h>

#include "stacktrace.h"
#include "os/mutex.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include "debug_info.h"

namespace panda {

struct VmaEntry {
    enum DebugInfoStatus { NOT_READ, VALID, BAD };

    // NOLINTNEXTLINE(modernize-pass-by-value)
    VmaEntry(uintptr_t param_start_addr, uintptr_t param_end_addr, uintptr_t param_offset, const std::string &fname)
        : start_addr(param_start_addr), end_addr(param_end_addr), offset(param_offset), filename(fname)
    {
    }

    ~VmaEntry() = default;

    uintptr_t start_addr;               // NOLINT(misc-non-private-member-variables-in-classes)
    uintptr_t end_addr;                 // NOLINT(misc-non-private-member-variables-in-classes)
    uintptr_t offset;                   // NOLINT(misc-non-private-member-variables-in-classes)
    std::string filename;               // NOLINT(misc-non-private-member-variables-in-classes)
    DebugInfoStatus status {NOT_READ};  // NOLINT(misc-non-private-member-variables-in-classes)
    DebugInfo debug_info;               // NOLINT(misc-non-private-member-variables-in-classes)

    DEFAULT_MOVE_SEMANTIC(VmaEntry);
    NO_COPY_SEMANTIC(VmaEntry);
};

class Tokenizer {
public:
    // NOLINTNEXTLINE(modernize-pass-by-value)
    explicit Tokenizer(const std::string &str) : str_(str), pos_(0) {}

    std::string Next(char delim = ' ')
    {
        while (pos_ < str_.length() && str_[pos_] == ' ') {
            ++pos_;
        }
        size_t pos = str_.find(delim, pos_);
        std::string token;
        if (pos == std::string::npos) {
            token = str_.substr(pos_);
            pos_ = str_.length();
        } else {
            token = str_.substr(pos_, pos - pos_);
            pos_ = pos + 1;  // skip delimiter
        }
        return token;
    }

private:
    std::string str_;
    size_t pos_;
};

class StackPrinter {
public:
    static StackPrinter &GetInstance()
    {
        static StackPrinter printer;
        return printer;
    }

    std::ostream &Print(const std::vector<uintptr_t> &stacktrace, std::ostream &out)
    {
        os::memory::LockHolder lock(mutex_);
        ScanVma();
        for (size_t frame_num = 0; frame_num < stacktrace.size(); ++frame_num) {
            PrintFrame(frame_num, stacktrace[frame_num], out);
        }
        return out;
    }

    NO_MOVE_SEMANTIC(StackPrinter);
    NO_COPY_SEMANTIC(StackPrinter);

private:
    explicit StackPrinter() = default;
    ~StackPrinter() = default;

    void PrintFrame(size_t frame_num, uintptr_t pc, std::ostream &out)
    {
        std::ios_base::fmtflags f = out.flags();
        auto w = out.width();
        out << "#" << std::setw(2U) << std::left << frame_num << ": 0x" << std::hex << pc << " ";
        out.flags(f);
        out.width(w);

        VmaEntry *vma = FindVma(pc);
        if (vma == nullptr) {
            vmas_.clear();
            ScanVma();
            vma = FindVma(pc);
        }
        if (vma != nullptr) {
            uintptr_t pc_offset = pc - vma->start_addr + vma->offset;
            // pc points to the instruction after the call
            // Decrement pc to get source line number pointing to the function call
            --pc_offset;
            std::string function;
            std::string src_file;
            unsigned int line = 0;
            if (ReadDebugInfo(vma) && vma->debug_info.GetSrcLocation(pc_offset, &function, &src_file, &line)) {
                PrintFrame(function, src_file, line, out);
                return;
            }
            uintptr_t offset = 0;
            if (ReadSymbol(pc, &function, &offset)) {
                PrintFrame(function, offset, out);
                return;
            }
        }
        out << "??:??\n";
    }

    void PrintFrame(const std::string &function, const std::string &src_file, unsigned int line,
                    std::ostream &out) const
    {
        if (function.empty()) {
            out << "??";
        } else {
            Demangle(function, out);
        }
        out << "\n     at ";
        if (src_file.empty()) {
            out << "??";
        } else {
            out << src_file;
        }
        out << ":";
        if (line == 0) {
            out << "??";
        } else {
            out << line;
        }

        out << "\n";
    }

    void PrintFrame(const std::string &function, uintptr_t offset, std::ostream &out) const
    {
        std::ios_base::fmtflags f = out.flags();
        Demangle(function, out);
        out << std::hex << "+0x" << offset << "\n";
        out.flags(f);
    }

    bool ReadSymbol(uintptr_t pc, std::string *function, uintptr_t *offset) const
    {
        Dl_info info {};
        if (dladdr(reinterpret_cast<void *>(pc), &info) != 0 && info.dli_sname != nullptr) {
            *function = info.dli_sname;
            *offset = pc - reinterpret_cast<uintptr_t>(info.dli_saddr);
            return true;
        }
        return false;
    }

    void Demangle(const std::string &function, std::ostream &out) const
    {
        size_t length = 0;
        int status = 0;
        char *demangled_function = abi::__cxa_demangle(function.c_str(), nullptr, &length, &status);
        if (status == 0) {
            out << demangled_function;
            free(demangled_function);  // NOLINT(cppcoreguidelines-no-malloc)
        } else {
            out << function;
        }
    }

    VmaEntry *FindVma(uintptr_t pc)
    {
        VmaEntry el(pc, pc, 0, "");
        auto it = std::upper_bound(vmas_.begin(), vmas_.end(), el,
                                   [](const VmaEntry &e1, const VmaEntry &e2) { return e1.end_addr < e2.end_addr; });
        if (it != vmas_.end() && (it->start_addr <= pc && pc < it->end_addr)) {
            return &(*it);
        }
        return nullptr;
    }

    bool ReadDebugInfo(VmaEntry *vma) const
    {
        if (vma->status == VmaEntry::VALID) {
            return true;
        }
        if (vma->status == VmaEntry::BAD) {
            return false;
        }
        if (!vma->filename.empty() && vma->debug_info.ReadFromFile(vma->filename.c_str()) == DebugInfo::SUCCESS) {
            vma->status = VmaEntry::VALID;
            return true;
        }
        vma->status = VmaEntry::BAD;
        return false;
    }

    void ScanVma()
    {
        static const int HEX_RADIX = 16;
        static const size_t MODE_FIELD_LEN = 4;
        static const size_t XMODE_POS = 2;

        if (!vmas_.empty()) {
            return;
        }

        std::stringstream fname;
        fname << "/proc/self/maps";
        std::string filename = fname.str();
        std::ifstream maps(filename.c_str());

        while (maps) {
            std::string line;
            std::getline(maps, line);
            Tokenizer tokenizer(line);
            std::string start_addr = tokenizer.Next('-');
            std::string end_addr = tokenizer.Next();
            std::string rights = tokenizer.Next();
            if (rights.length() == MODE_FIELD_LEN && rights[XMODE_POS] == 'x') {
                std::string offset = tokenizer.Next();
                tokenizer.Next();
                tokenizer.Next();
                std::string obj_filename = tokenizer.Next();
                vmas_.emplace_back(stoul(start_addr, nullptr, HEX_RADIX), stoul(end_addr, nullptr, HEX_RADIX),
                                   stoul(offset, nullptr, HEX_RADIX), obj_filename);
            }
        }
    }

private:
    std::vector<VmaEntry> vmas_;
    os::memory::Mutex mutex_;
};

class Buf {
public:
    Buf(uintptr_t *buf, size_t skip, size_t capacity) : buf_(buf), skip_(skip), size_(0), capacity_(capacity) {}
    ~Buf() = default;
    DEFAULT_MOVE_SEMANTIC(Buf);
    DEFAULT_COPY_SEMANTIC(Buf);
    void Append(uintptr_t pc)
    {
        if (skip_ > 0) {
            // Skip the element
            --skip_;
            return;
        }
        if (size_ >= capacity_) {
            return;
        }
        buf_[size_++] = pc;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    int Size() const
    {
        return size_;
    }

private:
    uintptr_t *buf_;
    size_t skip_;
    size_t size_;
    size_t capacity_;
};

static _Unwind_Reason_Code FrameHandler(struct _Unwind_Context *ctx, [[maybe_unused]] void *arg)
{
    Buf *buf = reinterpret_cast<Buf *>(arg);
    uintptr_t pc = _Unwind_GetIP(ctx);
    // _Unwind_GetIP returns 0 pc at the end of the stack. Ignore it
    if (pc != 0) {
        buf->Append(pc);
    }
    return _URC_NO_REASON;
}

std::vector<uintptr_t> GetStacktrace()
{
    static constexpr size_t BUF_SIZE = 100;
    static constexpr int SKIP_FRAMES = 2;  // backtrace
    std::vector<uintptr_t> buf;
    buf.resize(BUF_SIZE);
    Buf buf_wrapper(buf.data(), SKIP_FRAMES, buf.size());
    _Unwind_Reason_Code res = _Unwind_Backtrace(FrameHandler, &buf_wrapper);
    if (res != _URC_END_OF_STACK || buf_wrapper.Size() < 0) {
        return std::vector<uintptr_t>();
    }

    buf.resize(buf_wrapper.Size());
    return buf;
}

std::ostream &PrintStack(const std::vector<uintptr_t> &stacktrace, std::ostream &out)
{
    return StackPrinter::GetInstance().Print(stacktrace, out);
}

}  // namespace panda
