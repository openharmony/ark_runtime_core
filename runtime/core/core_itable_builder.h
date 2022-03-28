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

#ifndef PANDA_RUNTIME_CORE_CORE_ITABLE_BUILDER_H_
#define PANDA_RUNTIME_CORE_CORE_ITABLE_BUILDER_H_

#include "runtime/include/itable_builder.h"

namespace panda {

class CoreITableBuilder : public ITableBuilder {
public:
    void Build([[maybe_unused]] ClassLinker *class_linker, [[maybe_unused]] Class *base,
               [[maybe_unused]] Span<Class *> class_interfaces, [[maybe_unused]] bool is_interface) override
    {
        ASSERT(base->IsObjectClass());
        ASSERT(base->GetITable().Size() == 0);
        ASSERT(class_interfaces.Empty());
        ASSERT(!is_interface);
    }

    void Resolve([[maybe_unused]] Class *klass) override {};

    void UpdateClass([[maybe_unused]] Class *klass) override {};

    void DumpITable([[maybe_unused]] Class *klass) override {};

    ITable GetITable() const override
    {
        return {};
    };
};

}  // namespace panda

#endif  // PANDA_RUNTIME_CORE_CORE_ITABLE_BUILDER_H_
