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

#ifndef PANDA_RUNTIME_INCLUDE_IMTABLE_BUILDER_H_
#define PANDA_RUNTIME_INCLUDE_IMTABLE_BUILDER_H_

#include "libpandabase/macros.h"
#include "libpandafile/class_data_accessor.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/mem/panda_smart_pointers.h"

namespace panda {

class ClassLinker;

class IMTableBuilder {
public:
    static constexpr uint32_t OVERSIZE_MULTIPLE = 2;

    void Build(const panda_file::ClassDataAccessor *cda, ITable itable);

    void Build(ITable itable, bool is_interface);

    void UpdateClass(Class *klass);

    bool AddMethod(panda::Span<panda::Method *> imtable, uint32_t imtable_size, uint32_t id, Method *method);

    void DumpIMTable(Class *klass);

    size_t GetIMTSize() const
    {
        return imt_size;
    }

    void SetIMTSize(size_t size)
    {
        imt_size = size;
    }

    IMTableBuilder() = default;
    ~IMTableBuilder() = default;

    NO_COPY_SEMANTIC(IMTableBuilder);
    NO_MOVE_SEMANTIC(IMTableBuilder);

private:
    size_t imt_size = 0;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_IMTABLE_BUILDER_H_
