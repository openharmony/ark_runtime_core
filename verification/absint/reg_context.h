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

#ifndef PANDA_VERIFICATION_ABSINT_REG_CONTEXT_H_
#define PANDA_VERIFICATION_ABSINT_REG_CONTEXT_H_

#include "value/abstract_typed_value.h"

#include "util/index.h"
#include "util/lazy.h"
#include "util/str.h"
#include "util/shifted_vector.h"

#include "macros.h"

#include <algorithm>

namespace panda::verifier {
/*
Design decisions:
1. regs - unordered map, for speed (compared to map) and space efficiency (compared to vector)
   after implementing sparse vectors - rebase on them (taking into consideration immutability, see immer)
*/

class RegContext {
public:
    RegContext() = default;
    explicit RegContext(size_t size) : Regs_(size) {}
    ~RegContext() = default;
    DEFAULT_COPY_SEMANTIC(RegContext);
    DEFAULT_MOVE_SEMANTIC(RegContext);

    RegContext operator&(const RegContext &rhs) const
    {
        RegContext result(std::max(Regs_.size(), rhs.Regs_.size()));
        auto result_it = result.Regs_.begin();
        auto lhs_it = Regs_.begin();
        auto rhs_it = rhs.Regs_.begin();
        while (lhs_it != Regs_.end() && rhs_it != rhs.Regs_.end()) {
            if (!(*lhs_it).IsNone() && !(*rhs_it).IsNone()) {
                *result_it = *lhs_it & *rhs_it;
            }
            ++lhs_it;
            ++rhs_it;
            ++result_it;
        }
        return result;
    }
    RegContext &operator&=(const RegContext &rhs)
    {
        auto lhs_it = Regs_.begin();
        auto rhs_it = rhs.Regs_.begin();
        while (lhs_it != Regs_.end() && rhs_it != rhs.Regs_.end()) {
            if (!(*lhs_it).IsNone() && !(*rhs_it).IsNone()) {
                *lhs_it = *lhs_it & *rhs_it;
            } else {
                *lhs_it = AbstractTypedValue {};
            }
            ++lhs_it;
            ++rhs_it;
        }
        while (lhs_it != Regs_.end()) {
            *lhs_it = AbstractTypedValue {};
            ++lhs_it;
        }
        return *this;
    }
    void ChangeValuesOfSameOrigin(int idx, const AbstractTypedValue &atv)
    {
        if (!Regs_.InValidRange(idx)) {
            Regs_[idx] = atv;
            return;
        }
        auto old_atv = Regs_[idx];
        if (old_atv.IsNone()) {
            Regs_[idx] = atv;
            return;
        }
        const auto &old_origin = old_atv.GetOrigin();
        if (!old_origin.IsValid()) {
            Regs_[idx] = atv;
            return;
        }
        auto it = Regs_.begin();
        while (it != Regs_.end()) {
            if (!(*it).IsNone()) {
                const auto &origin = (*it).GetOrigin();
                if (origin.IsValid() && origin == old_origin) {
                    *it = atv;
                }
            }
            ++it;
        }
    }
    AbstractTypedValue &operator[](int idx)
    {
        if (!Regs_.InValidRange(idx)) {
            Regs_.ExtendToInclude(idx);
        }
        return Regs_[idx];
    }
    AbstractTypedValue operator[](int idx) const
    {
        ASSERT(IsRegDefined(idx) && Regs_.InValidRange(idx));
        return Regs_[idx];
    }
    size_t Size() const
    {
        size_t size = 0;
        EnumerateAllRegs([&size](auto, auto) {
            ++size;
            return true;
        });
        return size;
    }
    template <typename Callback>
    void EnumerateAllRegs(Callback cb) const
    {
        for (int idx = Regs_.begin_index(); idx < Regs_.end_index(); ++idx) {
            if (!Regs_[idx].IsNone()) {
                const auto &atv = Regs_[idx];
                if (!cb(idx, atv)) {
                    return;
                }
            }
        }
    }
    template <typename Callback>
    void EnumerateAllRegs(Callback cb)
    {
        for (int idx = Regs_.begin_index(); idx < Regs_.end_index(); ++idx) {
            if (!Regs_[idx].IsNone()) {
                auto &atv = Regs_[idx];
                if (!cb(idx, atv)) {
                    return;
                }
            }
        }
    }
    bool HasInconsistentRegs() const
    {
        bool result = false;
        EnumerateAllRegs([&result](int, const auto &av) {
            if (!av.IsConsistent()) {
                result = true;
                return false;
            }
            return true;
        });
        return result;
    }
    auto InconsistentRegsNums() const
    {
        PandaVector<int> result;
        EnumerateAllRegs([&result](int num, const auto &av) {
            if (!av.IsConsistent()) {
                result.push_back(num);
            }
            return true;
        });
        return result;
    }
    bool IsRegDefined(int num) const
    {
        return Regs_.InValidRange(num) && !Regs_[num].IsNone();
    }
    bool WasConflictOnReg(int num) const
    {
        return ConflictingRegs_.count(num) > 0;
    }
    void Clear()
    {
        Regs_.clear();
        ConflictingRegs_.clear();
    }
    void RemoveInconsistentRegs()
    {
        EnumerateAllRegs([this](int num, auto &atv) {
            if (!atv.IsConsistent()) {
                ConflictingRegs_.insert(num);
                atv = AbstractTypedValue {};
            } else {
                ConflictingRegs_.erase(num);
            }
            return true;
        });
    }
    template <typename ImageFunc>
    PandaString DumpRegs(ImageFunc img) const
    {
        PandaString log_string {""};
        bool comma = false;
        EnumerateAllRegs([&comma, &log_string, &img](int num, const auto &abs_type_val) {
            PandaString result {num == -1 ? "acc" : "v" + NumToStr<PandaString>(num)};
            result += " : ";
            result += abs_type_val.template Image<PandaString>(img);
            if (comma) {
                log_string += ", ";
            }
            log_string += result;
            comma = true;
            return true;
        });
        return log_string;
    }

private:
    ShiftedVector<1, AbstractTypedValue> Regs_;

    PandaUnorderedSet<int> ConflictingRegs_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_ABSINT_REG_CONTEXT_H_
