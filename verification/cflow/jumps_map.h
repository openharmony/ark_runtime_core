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

#ifndef PANDA_VERIFICATION_CFLOW_JUMPS_MAP_H_
#define PANDA_VERIFICATION_CFLOW_JUMPS_MAP_H_

#include "instructions_map.h"

#include "util/index.h"
#include "util/addr_map.h"
#include "util/lazy.h"

#include "runtime/include/mem/panda_containers.h"

#include <cstdint>

namespace panda::verifier {
class JumpsMap {
public:
    struct FromTo {
        const void *From;
        const void *To;
    };

    bool PutJump(const void *pc_jump_ptr, const void *pc_target_ptr)
    {
        if (!AddrMap_.IsInAddressSpace(pc_jump_ptr) || !AddrMap_.IsInAddressSpace(pc_target_ptr)) {
            return false;
        }
        bool target_already_marked = AddrMap_.HasMark(pc_target_ptr);
        if (!AddrMap_.Mark(pc_target_ptr)) {
            return false;
        }
        FromTo_.push_back({pc_jump_ptr, pc_target_ptr});
        if (!target_already_marked) {
            Target_.push_back(pc_target_ptr);
        }
        return true;
    }

    JumpsMap(const void *pc_start_ptr, const void *pc_end_ptr) : AddrMap_(pc_start_ptr, pc_end_ptr) {}
    JumpsMap(const void *pc_start_ptr, size_t size)
        : JumpsMap(pc_start_ptr, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_start_ptr) + size - 1))
    {
    }
    JumpsMap() = delete;
    ~JumpsMap() = default;

    template <typename PtrType, typename Callback>
    void EnumerateAllTargets(Callback cb) const
    {
        for (const auto &tgt : Target_) {
            if (!cb(reinterpret_cast<PtrType>(tgt))) {
                return;
            }
        }
    }

    template <typename PtrType>
    auto AllTargetsLazy() const
    {
        return Transform(ConstLazyFetch(Target_),
                         [](const void *ptr) -> PtrType { return reinterpret_cast<PtrType>(const_cast<void *>(ptr)); });
    }

    template <typename PtrType, typename Callback>
    void EnumerateAllJumpsToTarget(PtrType pc_target_ptr, Callback cb) const
    {
        // it is slow, but this operation is assumed to be very rare, only on
        // cflow verification failures, so we here trade speed of this function
        // for much faster positive path checks
        for (const auto &from_to : FromTo_) {
            if (from_to.To == reinterpret_cast<const void *>(pc_target_ptr)) {
                if (!cb(reinterpret_cast<PtrType>(from_to.From))) {
                    return;
                }
            }
        }
    }
    bool IsConflictingWith(const InstructionsMap &inst_map) const
    {
        return AddrMap_.HasCommonMarks(inst_map.AddrMap_);
    }
    template <typename PtrType>
    bool GetFirstConflictingJump(const InstructionsMap &inst_map, PtrType *pc_jump_ptr, PtrType *pc_target_ptr) const
    {
        if (!AddrMap_.GetFirstCommonMark(inst_map.AddrMap_, pc_target_ptr)) {
            return false;
        }
        *pc_jump_ptr = nullptr;
        EnumerateAllJumpsToTarget<PtrType>(*pc_target_ptr, [&pc_jump_ptr](PtrType jmp_ptr) {
            *pc_jump_ptr = jmp_ptr;
            return false;
        });
        ASSERT(*pc_jump_ptr != nullptr);
        return true;
    }

private:
    AddrMap AddrMap_;
    PandaVector<const void *> Target_;
    PandaVector<FromTo> FromTo_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_CFLOW_JUMPS_MAP_H_
