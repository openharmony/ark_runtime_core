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

#ifndef PANDA_VERIFICATION_CFLOW_INSTRUCTIONS_MAP_H_
#define PANDA_VERIFICATION_CFLOW_INSTRUCTIONS_MAP_H_

#include "util/addr_map.h"

#include <cstdint>

namespace panda::verifier {
class InstructionsMap {
public:
    bool PutInstruction(const void *pc_curr, const void *pc_next)
    {
        if (!AddrMap_.IsInAddressSpace(pc_curr)) {
            return false;
        }
        pc_curr = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_curr) + 1);
        if (pc_curr == pc_next) {
            return true;
        }
        pc_next = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_next) - 1);
        return AddrMap_.Mark(pc_curr, pc_next);
    }
    bool PutInstruction(const void *pc_ptr, size_t sz)
    {
        return PutInstruction(pc_ptr, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_ptr) + sz));
    }
    bool MarkCodeBlock(const void *pc_start, const void *pc_end)
    {
        return AddrMap_.Mark(pc_start, pc_end);
    }
    bool MarkCodeBlock(const void *pc_start, size_t sz)
    {
        return AddrMap_.Mark(pc_start, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_start) + sz - 1));
    }
    bool ClearCodeBlock(const void *pc_start, const void *pc_end)
    {
        return AddrMap_.Clear(pc_start, pc_end);
    }
    bool ClearCodeBlock(const void *pc_start, size_t sz)
    {
        return AddrMap_.Clear(pc_start, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_start) + sz - 1));
    }

    InstructionsMap(const void *ptr_start, const void *ptr_end) : AddrMap_ {ptr_start, ptr_end} {}

    InstructionsMap(const void *ptr_start, size_t size)
        : InstructionsMap(ptr_start, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(ptr_start) + size - 1))
    {
    }
    InstructionsMap() = delete;
    ~InstructionsMap() = default;

    bool CanJumpTo(const void *pc_target_ptr) const
    {
        return !AddrMap_.HasMark(pc_target_ptr);
    }

    template <typename PtrType>
    PtrType AddrStart() const
    {
        return AddrMap_.AddrStart<PtrType>();
    }

    template <typename PtrType>
    PtrType AddrEnd() const
    {
        return AddrMap_.AddrEnd<PtrType>();
    }

private:
    AddrMap AddrMap_;
    friend class JumpsMap;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_CFLOW_INSTRUCTIONS_MAP_H_
