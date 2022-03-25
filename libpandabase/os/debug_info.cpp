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

#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libdwarf/dwarf.h>
#include "debug_info.h"
#include "utils/logger.h"

namespace panda {

class DwarfGuard {
public:
    DwarfGuard(Dwarf_Debug dbg, void *mem, Dwarf_Unsigned tag) : dbg_(dbg), mem_(mem), tag_(tag) {}

    void Release()
    {
        mem_ = nullptr;
    }

    void Reset(void *new_mem)
    {
        if (mem_ != new_mem && mem_ != nullptr) {
            dwarf_dealloc(dbg_, mem_, tag_);
            mem_ = new_mem;
        }
    }

    ~DwarfGuard()
    {
        Reset(nullptr);
    }

    NO_MOVE_SEMANTIC(DwarfGuard);
    NO_COPY_SEMANTIC(DwarfGuard);

private:
    Dwarf_Debug dbg_;
    void *mem_;
    Dwarf_Unsigned tag_;
};

template <class F>
class AtReturn {
public:
    explicit AtReturn(F func) : func_(func) {}

    ~AtReturn()
    {
        func_();
    }

    NO_MOVE_SEMANTIC(AtReturn);
    NO_COPY_SEMANTIC(AtReturn);

private:
    F func_;
};

static void FreeAranges(Dwarf_Debug dbg, Dwarf_Arange *aranges, Dwarf_Signed count)
{
    for (Dwarf_Signed i = 0; i < count; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dwarf_dealloc(dbg, aranges[i], DW_DLA_ARANGE);
    }
    dwarf_dealloc(dbg, aranges, DW_DLA_LIST);
}

static void SkipCuHeaders(Dwarf_Debug dbg)
{
    Dwarf_Unsigned cu_header_idx;
    Dwarf_Half cu_type;
    while (dwarf_next_cu_header_d(dbg, static_cast<Dwarf_Bool>(true), nullptr, nullptr, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, &cu_header_idx, &cu_type, nullptr) != DW_DLV_NO_ENTRY) {
    }
}

static void DwarfErrorHandler(Dwarf_Error err, [[maybe_unused]] Dwarf_Ptr errarg)
{
    LOG(ERROR, RUNTIME) << "libdwarf error: " << dwarf_errmsg(err);
}

static bool GetDieRange(Dwarf_Die die, Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc)
{
    Dwarf_Addr low_pc = DW_DLV_BADADDR;
    Dwarf_Addr high_pc = 0;
    Dwarf_Half form = 0;
    Dwarf_Form_Class formclass;

    if (dwarf_lowpc(die, &low_pc, nullptr) != DW_DLV_OK ||
        dwarf_highpc_b(die, &high_pc, &form, &formclass, nullptr) != DW_DLV_OK) {
        return false;
    }
    if (formclass == DW_FORM_CLASS_CONSTANT) {
        high_pc += low_pc;
    }
    *out_low_pc = low_pc;
    *out_high_pc = high_pc;
    return true;
}

template <class F>
bool IterateDieRanges(Dwarf_Debug dbg, Dwarf_Die die, F func)
{
    Dwarf_Addr low_pc = DW_DLV_BADADDR;
    Dwarf_Addr high_pc = DW_DLV_BADADDR;

    if (GetDieRange(die, &low_pc, &high_pc)) {
        return func(low_pc, high_pc);
    }

    Dwarf_Attribute attr;
    if (dwarf_attr(die, DW_AT_ranges, &attr, nullptr) != DW_DLV_OK) {
        return false;
    }
    DwarfGuard g(dbg, attr, DW_DLA_ATTR);
    Dwarf_Unsigned offset = 0;
    Dwarf_Addr base_addr = 0;
    if (low_pc != DW_DLV_BADADDR) {
        base_addr = low_pc;
    }
    Dwarf_Signed count = 0;
    Dwarf_Ranges *buf = nullptr;
    if (dwarf_global_formref(attr, &offset, nullptr) == DW_DLV_OK &&
        dwarf_get_ranges_a(dbg, offset, die, &buf, &count, nullptr, nullptr) == DW_DLV_OK) {
        AtReturn r([dbg, buf, count]() { dwarf_ranges_dealloc(dbg, buf, count); });
        Span<Dwarf_Ranges> ranges(buf, count);
        for (const Dwarf_Ranges &range : ranges) {
            if (range.dwr_type == DW_RANGES_ENTRY) {
                Dwarf_Addr rng_low_pc = base_addr + range.dwr_addr1;
                Dwarf_Addr rng_high_pc = base_addr + range.dwr_addr2;
                if (func(rng_low_pc, rng_high_pc)) {
                    return true;
                }
            } else if (range.dwr_type == DW_RANGES_ADDRESS_SELECTION) {
                base_addr = range.dwr_addr2;
            } else {
                break;
            }
        }
    }
    return false;
}

DebugInfo::CompUnit::~CompUnit()
{
    if (line_ctx_ != nullptr) {
        dwarf_srclines_dealloc_b(line_ctx_);
    }
    if (cu_die_ != nullptr) {
        dwarf_dealloc(dbg_, cu_die_, DW_DLA_DIE);
    }
}

Dwarf_Line_Context DebugInfo::CompUnit::GetLineContext()
{
    if (line_ctx_ != nullptr) {
        return line_ctx_;
    }
    // Decode line number information for the whole compilation unit
    Dwarf_Unsigned version = 0;
    Dwarf_Small table_count = 0;
    if (dwarf_srclines_b(cu_die_, &version, &table_count, &line_ctx_, nullptr) != DW_DLV_OK) {
        line_ctx_ = nullptr;
    }
    return line_ctx_;
}

void DebugInfo::Destroy()
{
    if (dbg_ == nullptr) {
        return;
    }
    if (aranges_ != nullptr) {
        FreeAranges(dbg_, aranges_, arange_count_);
    }
    aranges_ = nullptr;
    arange_count_ = 0;
    cu_list_.clear();
    ranges_.clear();
    dwarf_finish(dbg_, nullptr);
    close(fd_);
    fd_ = INVALID_FD;
    dbg_ = nullptr;
}

DebugInfo::ErrorCode DebugInfo::ReadFromFile(const char *filename)
{
    Dwarf_Error err = nullptr;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-signed-bitwise)
    fd_ = open(filename, O_RDONLY | O_CLOEXEC);
    int res = dwarf_init(fd_, DW_DLC_READ, DwarfErrorHandler, nullptr, &dbg_, &err);
    if (res != DW_DLV_OK) {
        // In case dwarf_init fails it allocates memory for the error and returns it in 'err' variable.
        // But since dbg is NULL, dwarf_dealloc just returns in case of dbg == nullptr and doesn't free this memory.
        // A possible solution is to use 20201201 version and call dwarf_dealloc.
        free(err);  // NOLINT(cppcoreguidelines-no-malloc)
        close(fd_);
        fd_ = INVALID_FD;
        dbg_ = nullptr;
    }
    if (res == DW_DLV_ERROR) {
        return ERROR;
    }
    if (res == DW_DLV_NO_ENTRY) {
        return NO_DEBUG_INFO;
    }
    // Aranges (address ranges, something like index) is an entity which helps us to find the compilation unit quickly.
    if (dwarf_get_aranges(dbg_, &aranges_, &arange_count_, nullptr) != DW_DLV_OK) {
        aranges_ = nullptr;
        arange_count_ = 0;
    }
    return SUCCESS;
}

bool DebugInfo::GetSrcLocation(uintptr_t pc, std::string *function, std::string *src_file, uint32_t *line)
{
    if (dbg_ == nullptr) {
        return false;
    }

    // Debug information has hierarchical structure.
    // Each node is represented by DIE (debug information entity).
    // .debug_info has a list of DIEs which correspond to compilation units (object files).
    // Mapping pc to function is to find the compilation unit DIE and then find the subprogram DIE.
    // From the subprogram DIE we get the function name.
    // Line information is available for compilation unit DIEs. So we decode lines for the whole
    // compilation unit and find the corresponding line and file which matche the pc.
    //
    // You could use objdump --dwarf=info <object file> to view available debug information.

    Range range(pc, pc);
    auto it = ranges_.upper_bound(range);
    if (it == ranges_.end() || !it->Contain(pc)) {
        Dwarf_Die cu_die = nullptr;
        if (!FindCompUnitByPc(pc, &cu_die)) {
            return false;
        }
        cu_list_.emplace_back(CompUnit(cu_die, dbg_));
        auto ranges = &ranges_;
        auto cu = &cu_list_.back();
        IterateDieRanges(dbg_, cu_die, [ranges, cu](Dwarf_Addr low_pc, Dwarf_Addr high_pc) {
            ranges->insert(Range(low_pc, high_pc, cu));
            return false;
        });
        TraverseChildren(cu, cu_die);
    }
    it = ranges_.upper_bound(range);
    if (it == ranges_.end() || !it->Contain(pc)) {
        return false;
    }

    ASSERT(it->GetCu() != nullptr);
    *function = it->GetFunction();
    // Find the corresponding line number and source file.
    GetSrcFileAndLine(pc, it->GetCu()->GetLineContext(), src_file, line);
    return true;
}

bool DebugInfo::FindCompUnitByPc(uintptr_t pc, Dwarf_Die *cu_die)
{
    if (aranges_ != nullptr) {
        Dwarf_Arange arange = nullptr;
        Dwarf_Off offset = 0;
        if (dwarf_get_arange(aranges_, arange_count_, pc, &arange, nullptr) == DW_DLV_OK &&
            dwarf_get_cu_die_offset(arange, &offset, nullptr) == DW_DLV_OK &&
            dwarf_offdie(dbg_, offset, cu_die, nullptr) == DW_DLV_OK) {
            return true;
        }
    }

    // No aranges are available or we can't find the corresponding arange. Iterate over all compilation units.
    // Its slow but works.
    Dwarf_Unsigned cu_header_idx;
    Dwarf_Half cu_type;
    int res = dwarf_next_cu_header_d(dbg_, static_cast<Dwarf_Bool>(true), nullptr, nullptr, nullptr, nullptr, nullptr,
                                     nullptr, nullptr, nullptr, &cu_header_idx, &cu_type, nullptr);
    while (res == DW_DLV_OK) {
        Dwarf_Die die = nullptr;
        if (dwarf_siblingof_b(dbg_, nullptr, static_cast<Dwarf_Bool>(true), &die, nullptr) == DW_DLV_OK) {
            if (PcMatches(pc, die)) {
                *cu_die = die;
                // Skip the rest cu headers because next time we need to stat search from the beginning.
                SkipCuHeaders(dbg_);
                return true;
            }
            dwarf_dealloc(dbg_, die, DW_DLA_DIE);
        }
        res = dwarf_next_cu_header_d(dbg_, static_cast<Dwarf_Bool>(true), nullptr, nullptr, nullptr, nullptr, nullptr,
                                     nullptr, nullptr, nullptr, &cu_header_idx, &cu_type, nullptr);
    }
    return false;
}

void DebugInfo::TraverseChildren(CompUnit *cu, Dwarf_Die die)
{
    Dwarf_Die child_die = nullptr;
    if (dwarf_child(die, &child_die, nullptr) != DW_DLV_OK) {
        return;
    }
    TraverseSiblings(cu, child_die);
}

void DebugInfo::TraverseSiblings(CompUnit *cu, Dwarf_Die die)
{
    DwarfGuard g(dbg_, die, DW_DLA_DIE);
    Dwarf_Half tag = 0;
    int res;
    do {
        if (dwarf_tag(die, &tag, nullptr) != DW_DLV_OK) {
            return;
        }
        if ((tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)) {
            Dwarf_Addr low_pc = DW_DLV_BADADDR;
            Dwarf_Addr high_pc = 0;

            if (GetDieRange(die, &low_pc, &high_pc)) {
                std::string fname;
                GetFunctionName(die, &fname);
                AddFunction(cu, low_pc, high_pc, fname);
            }
        }
        TraverseChildren(cu, die);
        Dwarf_Die sibling = nullptr;
        res = dwarf_siblingof_b(dbg_, die, static_cast<Dwarf_Bool>(true), &sibling, nullptr);
        if (res == DW_DLV_OK) {
            g.Reset(sibling);
            die = sibling;
        }
    } while (res == DW_DLV_OK);
}

void DebugInfo::AddFunction(CompUnit *cu, Dwarf_Addr low_pc, Dwarf_Addr high_pc, const std::string &function)
{
    auto it = ranges_.upper_bound(Range(low_pc, low_pc));
    ASSERT(it != ranges_.end());
    Range range(low_pc, high_pc, cu, function);
    if (it->Contain(range)) {
        Range enclosing = *it;
        ranges_.erase(it);
        if (enclosing.GetLowPc() < low_pc) {
            ranges_.insert(Range(enclosing.GetLowPc(), low_pc, enclosing.GetCu(), enclosing.GetFunction()));
        }
        ranges_.insert(range);
        if (high_pc < enclosing.GetHighPc()) {
            ranges_.insert(Range(high_pc, enclosing.GetHighPc(), enclosing.GetCu(), enclosing.GetFunction()));
        }
    } else if (range.Contain(*it)) {
        ranges_.insert(Range(range.GetLowPc(), it->GetLowPc(), cu, function));
        ranges_.insert(Range(it->GetHighPc(), range.GetHighPc(), cu, function));
    } else if (high_pc <= it->GetLowPc()) {
        ranges_.insert(range);
    }
}

void DebugInfo::GetFunctionName(Dwarf_Die die, std::string *function) const
{
    char *name = nullptr;

    // Prefer linkage name instead of name
    // Linkage name is a mangled name which contains information about enclosing class,
    // return type, parameters and so on.
    // The name which is stored in DW_AT_name attribute is only a function name.
    if (dwarf_die_text(die, DW_AT_linkage_name, &name, nullptr) == DW_DLV_OK ||
        dwarf_diename(die, &name, nullptr) == DW_DLV_OK) {
        DwarfGuard g(dbg_, name, DW_DLA_STRING);
        *function = name;
        return;
    }

    Dwarf_Off off = 0;
    Dwarf_Attribute attr = nullptr;
    Dwarf_Die abs_orig_die = nullptr;
    // If there is no name | linkage_name the function may be inlined.
    // Try to get it from the abstract origin
    if (dwarf_attr(die, DW_AT_abstract_origin, &attr, nullptr) == DW_DLV_OK) {
        DwarfGuard ag(dbg_, attr, DW_DLA_ATTR);
        if (dwarf_global_formref(attr, &off, nullptr) == DW_DLV_OK &&
            dwarf_offdie(dbg_, off, &abs_orig_die, nullptr) == DW_DLV_OK) {
            DwarfGuard dg(dbg_, abs_orig_die, DW_DLA_DIE);
            GetFunctionName(abs_orig_die, function);
            return;
        }
    }

    // If there is no name | linkage_name try to get it from the specification.
    Dwarf_Die spec_die = nullptr;
    if (dwarf_attr(die, DW_AT_specification, &attr, nullptr) == DW_DLV_OK) {
        DwarfGuard ag(dbg_, attr, DW_DLA_ATTR);
        if (dwarf_global_formref(attr, &off, nullptr) == DW_DLV_OK &&
            dwarf_offdie(dbg_, off, &spec_die, nullptr) == DW_DLV_OK) {
            DwarfGuard dg(dbg_, spec_die, DW_DLA_DIE);
            GetFunctionName(spec_die, function);
        }
    }
}

bool DebugInfo::GetSrcFileAndLine(uintptr_t pc, Dwarf_Line_Context line_ctx, std::string *out_src_file,
                                  uint32_t *out_line) const
{
    if (line_ctx == nullptr) {
        return false;
    }
    Dwarf_Line *line_buf = nullptr;
    Dwarf_Signed line_buf_size = 0;
    if (dwarf_srclines_from_linecontext(line_ctx, &line_buf, &line_buf_size, nullptr) != DW_DLV_OK) {
        return false;
    }
    Span<Dwarf_Line> lines(line_buf, line_buf_size);
    Dwarf_Addr prev_line_pc = 0;
    Dwarf_Line prev_line = nullptr;
    bool found = false;
    for (auto it = lines.begin(); it != lines.end() && !found; ++it) {
        Dwarf_Line line = *it;
        Dwarf_Addr line_pc = 0;
        dwarf_lineaddr(line, &line_pc, nullptr);
        if (pc == line_pc) {
            GetSrcFileAndLine(GetLastLineWithPc(pc, it, lines.end()), out_src_file, out_line);
            found = true;
        } else if (prev_line != nullptr && prev_line_pc < pc && pc < line_pc) {
            GetSrcFileAndLine(prev_line, out_src_file, out_line);
            found = true;
        } else {
            Dwarf_Bool is_line_end;
            dwarf_lineendsequence(line, &is_line_end, nullptr);
            if (is_line_end != 0) {
                prev_line = nullptr;
            } else {
                prev_line_pc = line_pc;
                prev_line = line;
            }
        }
    }
    return found;
}

Dwarf_Line DebugInfo::GetLastLineWithPc(Dwarf_Addr pc, Span<Dwarf_Line>::ConstIterator it,
                                        Span<Dwarf_Line>::ConstIterator end) const
{
    Dwarf_Addr line_pc = 0;
    auto next = std::next(it);
    while (next != end) {
        dwarf_lineaddr(*next, &line_pc, nullptr);
        if (pc != line_pc) {
            return *it;
        }
        it = next;
        ++next;
    }
    return *it;
}

void DebugInfo::GetSrcFileAndLine(Dwarf_Line line, std::string *out_src_file, uint32_t *out_line) const
{
    Dwarf_Unsigned ln;
    dwarf_lineno(line, &ln, nullptr);
    *out_line = ln;
    char *src_file = nullptr;
    if (dwarf_linesrc(line, &src_file, nullptr) == DW_DLV_OK) {
        *out_src_file = src_file;
        DwarfGuard g(dbg_, src_file, DW_DLA_STRING);
    } else {
        dwarf_linesrc(line, &src_file, nullptr);
        *out_src_file = src_file;
        DwarfGuard g(dbg_, src_file, DW_DLA_STRING);
    }
}

bool DebugInfo::PcMatches(uintptr_t pc, Dwarf_Die die) const
{
    Dwarf_Addr low_pc = DW_DLV_BADADDR;
    Dwarf_Addr high_pc = 0;
    return GetDieRangeForPc(pc, die, &low_pc, &high_pc);
}

bool DebugInfo::GetDieRange(Dwarf_Die die, Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc) const
{
    Dwarf_Addr low_pc = DW_DLV_BADADDR;
    Dwarf_Addr high_pc = 0;
    Dwarf_Half form = 0;
    Dwarf_Form_Class formclass;

    if (dwarf_lowpc(die, &low_pc, nullptr) != DW_DLV_OK ||
        dwarf_highpc_b(die, &high_pc, &form, &formclass, nullptr) != DW_DLV_OK) {
        return false;
    }
    if (formclass == DW_FORM_CLASS_CONSTANT) {
        high_pc += low_pc;
    }
    *out_low_pc = low_pc;
    *out_high_pc = high_pc;
    return true;
}

bool DebugInfo::GetDieRangeForPc(uintptr_t pc, Dwarf_Die die, Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc) const
{
    Dwarf_Addr low_pc = DW_DLV_BADADDR;
    Dwarf_Addr high_pc = 0;

    if (GetDieRange(die, &low_pc, &high_pc) && (*out_low_pc <= pc && pc < *out_high_pc)) {
        *out_low_pc = low_pc;
        *out_high_pc = high_pc;
        return true;
    }

    Dwarf_Attribute attr;
    if (dwarf_attr(die, DW_AT_ranges, &attr, nullptr) == DW_DLV_OK) {
        DwarfGuard g(dbg_, attr, DW_DLA_ATTR);
        Dwarf_Unsigned offset;
        Dwarf_Addr base_addr = 0;
        if (low_pc != DW_DLV_BADADDR) {
            base_addr = low_pc;
        }
        Dwarf_Signed count = 0;
        Dwarf_Ranges *ranges = nullptr;
        if (dwarf_global_formref(attr, &offset, nullptr) == DW_DLV_OK &&
            dwarf_get_ranges_a(dbg_, offset, die, &ranges, &count, nullptr, nullptr) == DW_DLV_OK) {
            Dwarf_Debug dbg = dbg_;
            AtReturn r([dbg, ranges, count]() { dwarf_ranges_dealloc(dbg, ranges, count); });
            return FindRangeForPc(pc, Span<Dwarf_Ranges>(ranges, count), base_addr, out_low_pc, out_high_pc);
        }
    }
    return false;
}

bool DebugInfo::FindRangeForPc(uintptr_t pc, const Span<Dwarf_Ranges> &ranges, Dwarf_Addr base_addr,
                               Dwarf_Addr *out_low_pc, Dwarf_Addr *out_high_pc) const
{
    for (const Dwarf_Ranges &range : ranges) {
        if (range.dwr_type == DW_RANGES_ENTRY) {
            Dwarf_Addr rng_low_pc = base_addr + range.dwr_addr1;
            Dwarf_Addr rng_high_pc = base_addr + range.dwr_addr2;
            if (rng_low_pc <= pc && pc < rng_high_pc) {
                *out_low_pc = rng_low_pc;
                *out_high_pc = rng_high_pc;
                return true;
            }
        } else if (range.dwr_type == DW_RANGES_ADDRESS_SELECTION) {
            base_addr = range.dwr_addr2;
        } else {
            break;
        }
    }
    return false;
}

}  // namespace panda
