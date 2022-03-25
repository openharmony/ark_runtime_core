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

#ifndef PANDA_RUNTIME_INCLUDE_ITABLE_BUILDER_H_
#define PANDA_RUNTIME_INCLUDE_ITABLE_BUILDER_H_

#include "libpandabase/macros.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/mem/panda_smart_pointers.h"

namespace panda {

class ClassLinker;

class ITableBuilder {
public:
    ITableBuilder() = default;

    virtual void Build(ClassLinker *class_linker, Class *base, Span<Class *> class_interfaces, bool is_interface) = 0;

    virtual void Resolve(Class *klass) = 0;

    virtual void UpdateClass(Class *klass) = 0;

    virtual void DumpITable(Class *klass) = 0;

    virtual ITable GetITable() const = 0;

    virtual ~ITableBuilder() = default;

    NO_COPY_SEMANTIC(ITableBuilder);
    NO_MOVE_SEMANTIC(ITableBuilder);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_ITABLE_BUILDER_H_
