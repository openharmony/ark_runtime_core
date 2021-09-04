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

#include "libpandabase/macros.h"
#include "runtime/include/imtable_builder.h"
#include "runtime/include/class_linker.h"

namespace panda {
void IMTableBuilder::Build(const panda_file::ClassDataAccessor *cda, ITable itable)
{
    if (cda->IsInterface() || itable.Size() == 0U) {
        return;
    }

    size_t ifm_num = 0;
    for (size_t i = 0; i < itable.Size(); i++) {
        auto &entry = itable[i];
        auto methods = entry.GetMethods();
        ifm_num += methods.Size();
    }

    // set imtable size rules
    // (1) as interface methods number when it's smaller than fixed IMTABLE_SIZE
    // (2) as IMTABLE_SIZE when it's in [IMTABLE_SIZE, IMTABLE_SIZE * OVERSIZE_MULTIPLE], eg. [32,64]
    // (3) as 0 when it's much more bigger than fixed IMTABLE_SIZE, since conflict probability is high and IMTable will
    // be almost empty
    SetIMTSize(ifm_num <= Class::IMTABLE_SIZE
                   ? ifm_num
                   : (ifm_num <= Class::IMTABLE_SIZE * OVERSIZE_MULTIPLE ? Class::IMTABLE_SIZE : 0));
}

void IMTableBuilder::Build(ITable itable, bool is_interface)
{
    if (is_interface || itable.Size() == 0U) {
        return;
    }

    size_t ifm_num = 0;
    for (size_t i = 0; i < itable.Size(); i++) {
        auto &entry = itable[i];
        auto methods = entry.GetMethods();
        ifm_num += methods.Size();
    }

    // set imtable size rules: the same as function above
    SetIMTSize(ifm_num <= Class::IMTABLE_SIZE
                   ? ifm_num
                   : (ifm_num <= Class::IMTABLE_SIZE * OVERSIZE_MULTIPLE ? Class::IMTABLE_SIZE : 0));
}

void IMTableBuilder::UpdateClass(Class *klass)
{
    if (klass->IsInterface() || klass->IsAbstract()) {
        return;
    }

    auto imtable_size = klass->GetIMTSize();
    if (imtable_size == 0U) {
        return;
    }

    std::array<bool, Class::IMTABLE_SIZE> is_method_conflict = {false};

    auto itable = klass->GetITable();
    auto imtable = klass->GetIMT();

    for (size_t i = 0; i < itable.Size(); i++) {
        auto &entry = itable[i];
        auto itf_methods = entry.GetInterface()->GetVirtualMethods();
        auto imp_methods = entry.GetMethods();

        for (size_t j = 0; j < itf_methods.Size(); j++) {
            auto imp_method = imp_methods[j];
            auto itf_method_id = klass->GetIMTableIndex(itf_methods[j].GetFileId().GetOffset());
            if (!is_method_conflict.at(itf_method_id)) {
                auto ret = AddMethod(imtable, imtable_size, itf_method_id, imp_method);
                is_method_conflict[itf_method_id] = !ret;
            }
        }
    }

#ifndef NDEBUG
    DumpIMTable(klass);
#endif  // NDEBUG
}

bool IMTableBuilder::AddMethod(panda::Span<panda::Method *> imtable, [[maybe_unused]] uint32_t imtable_size,
                               uint32_t id, Method *method)
{
    bool result = false;
    ASSERT(id < imtable_size);
    if (imtable[id] == nullptr) {
        imtable[id] = method;
        result = true;
    } else {
        imtable[id] = nullptr;
    }
    return result;
}

void IMTableBuilder::DumpIMTable(Class *klass)
{
    LOG(DEBUG, CLASS_LINKER) << "imtable of class " << klass->GetName() << ":";
    auto imtable = klass->GetIMT();
    auto imtable_size = klass->GetIMTSize();
    for (size_t i = 0; i < imtable_size; i++) {
        auto method = imtable[i];
        if (method != nullptr) {
            LOG(DEBUG, CLASS_LINKER) << "[ " << i << " ] " << method->GetFullName();
        } else {
            LOG(DEBUG, CLASS_LINKER) << "[ " << i << " ] "
                                     << "FREE SLOT";
        }
    }
}

}  // namespace panda
