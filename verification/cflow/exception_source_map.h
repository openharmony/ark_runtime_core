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

#ifndef PANDA_VERIFICATION_CFLOW_EXCEPTION_SOURCE_MAP_H_
#define PANDA_VERIFICATION_CFLOW_EXCEPTION_SOURCE_MAP_H_

#include "util/addr_map.h"

#include <cstdint>

namespace panda::verifier {
class ExceptionSourceMap {
public:
    bool PutExceptionSource(const void *pc)
    {
        return Map_.Mark(pc, pc);
    }
    bool PutExceptionSourceRange(const void *pc_start, const void *pc_end)
    {
        return Map_.Mark(pc_start, pc_end);
    }
    bool PutExceptionSourceRange(const void *pc_start, size_t sz)
    {
        return Map_.Mark(pc_start, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_start) + sz - 1));
    }
    bool ClearExceptionSource(const void *pc)
    {
        return Map_.Clear(pc, pc);
    }
    bool ClearExceptionSourceRange(const void *pc_start, const void *pc_end)
    {
        return Map_.Clear(pc_start, pc_end);
    }
    bool ClearExceptionSourceRange(const void *pc_start, size_t sz)
    {
        return Map_.Clear(pc_start, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(pc_start) + sz - 1));
    }
    ExceptionSourceMap(const void *ptr_start, const void *ptr_end) : Map_ {ptr_start, ptr_end} {}
    ExceptionSourceMap(const void *ptr_start, size_t size)
        : ExceptionSourceMap(ptr_start,
                             reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(ptr_start) + size - 1))
    {
    }
    ExceptionSourceMap() = delete;
    ~ExceptionSourceMap() = default;

    bool IsExceptionSource(const void *pc) const
    {
        return Map_.HasMark(pc);
    }

    template <typename Handler>
    void ForSourcesInRange(const void *from, const void *to, Handler &&handler) const
    {
        Map_.EnumerateMarksInScope<const uint8_t *>(from, to, std::move(handler));
    }

private:
    AddrMap Map_;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_CFLOW_EXCEPTION_SOURCE_MAP_H_
