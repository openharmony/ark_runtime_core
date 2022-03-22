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

#ifndef PANDA_LIBPANDABASE_OS_DEBUG_INFO_H_
#define PANDA_LIBPANDABASE_OS_DEBUG_INFO_H_

#include <set>
#include <list>
#include <string>
#include <libdwarf/libdwarf.h>
#include "macros.h"
#include "utils/span.h"

namespace panda {

class DebugInfo {
public:
    enum ErrorCode { SUCCESS, NO_DEBUG_INFO, ERROR };

    explicit DebugInfo() = default;

    ~DebugInfo()
    {
        Destroy();
    }

    ErrorCode ReadFromFile(const char *filename);

    /*
     * Find location (name, source file, line) of the specified pc in source code
     */
    bool GetSrcLocation(uintptr_t pc, std::string *function, std::string *src_file, uint32_t *line);

    void Destroy();

    DEFAULT_MOVE_SEMANTIC(DebugInfo);
    NO_COPY_SEMANTIC(DebugInfo);

private:
    /**
     * Cache entry for a compilation unit (object file).
     * It contains the pointer to the corresponding DIE (Debug Information Entity),
     * offset of the DIE in .debug_info, decoded line numbers for the compilation unit
     * and function cache.
     */
    class CompUnit {
    public:
        CompUnit(Dwarf_Die cu_die, Dwarf_Debug dbg) : dbg_(dbg), cu_die_(cu_die) {}

        CompUnit(CompUnit &&e) : dbg_(e.dbg_), cu_die_(e.cu_die_), line_ctx_(e.line_ctx_)
        {
            e.cu_die_ = nullptr;
            e.line_ctx_ = nullptr;
        }

        ~CompUnit();

        CompUnit &operator=(CompUnit &&e)
        {
            dbg_ = e.dbg_;
            cu_die_ = e.cu_die_;
            e.cu_die_ = nullptr;
            line_ctx_ = e.line_ctx_;
            e.line_ctx_ = nullptr;
            return *this;
        }

        Dwarf_Die GetDie() const
        {
            return cu_die_;
        }

        Dwarf_Line_Context GetLineContext();

        NO_COPY_SEMANTIC(CompUnit);

    private:
        Dwarf_Debug dbg_;
        Dwarf_Die cu_die_;
        Dwarf_Line_Context line_ctx_ {nullptr};
    };

    class Range {
    public:
        Range(Dwarf_Addr low_pc, Dwarf_Addr high_pc, CompUnit *cu = nullptr,
              const std::string &function = std::string())  // NOLINT(modernize-pass-by-value)
            : low_pc_(low_pc), high_pc_(high_pc), cu_(cu), function_(function)
        {
        }
        ~Range() = default;
        DEFAULT_COPY_SEMANTIC(Range);
        DEFAULT_MOVE_SEMANTIC(Range);

        Dwarf_Addr GetLowPc() const
        {
            return low_pc_;
        }

        Dwarf_Addr GetHighPc() const
        {
            return high_pc_;
        }

        bool Contain(Dwarf_Addr addr) const
        {
            return low_pc_ <= addr && addr < high_pc_;
        }

        bool Contain(const Range &r) const
        {
            return low_pc_ <= r.low_pc_ && r.high_pc_ <= high_pc_;
        }

        CompUnit *GetCu() const
        {
            return cu_;
        }

        std::string GetFunction() const
        {
            return function_;
        }

        void SetFunction(const std::string &function)
        {
            this->function_ = function;
        }

        bool operator<(const Range &r) const
        {
            return high_pc_ < r.high_pc_;
        }

        bool operator==(const Range &r) const
        {
            return low_pc_ == r.low_pc_ && high_pc_ == r.high_pc_;
        }

    private:
        Dwarf_Addr low_pc_;
        Dwarf_Addr high_pc_;
        CompUnit *cu_ = nullptr;
        std::string function_;
    };

private:
    bool FindCompUnitByPc(uintptr_t pc, Dwarf_Die *cu_die);
    void TraverseChildren(CompUnit *cu, Dwarf_Die die);
    void TraverseSiblings(CompUnit *cu, Dwarf_Die die);
    void GetFunctionName(Dwarf_Die die, std::string *function) const;
    void AddFunction(CompUnit *cu, Dwarf_Addr low_pc, Dwarf_Addr high_pc, const std::string &function);
    bool GetSrcFileAndLine(uintptr_t pc, Dwarf_Line_Context line_ctx, std::string *src_file, uint32_t *line) const;
    Dwarf_Line GetLastLineWithPc(Dwarf_Addr pc, Span<Dwarf_Line>::ConstIterator it,
                                 Span<Dwarf_Line>::ConstIterator end) const;
    void GetSrcFileAndLine(Dwarf_Line line, std::string *out_src_file, uint32_t *out_line) const;
    bool PcMatches(uintptr_t pc, Dwarf_Die die) const;
    bool GetDieRange(Dwarf_Die die, Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc) const;
    bool GetDieRangeForPc(uintptr_t pc, Dwarf_Die die, Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc) const;
    bool FindRangeForPc(uintptr_t pc, const Span<Dwarf_Ranges> &ranges, Dwarf_Addr base_addr, Dwarf_Addr *out_low_pc,
                        Dwarf_Addr *out_high_pc) const;

private:
    static constexpr int INVALID_FD = -1;

    int fd_ {INVALID_FD};
    Dwarf_Debug dbg_ {nullptr};
    Dwarf_Arange *aranges_ {nullptr};
    Dwarf_Signed arange_count_ {0};
    std::list<CompUnit> cu_list_;
    std::set<Range> ranges_;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_OS_DEBUG_INFO_H_
