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

#include <algorithm>

#include "libpandabase/macros.h"
#include "libpandabase/utils/hash.h"
#include "libpandabase/utils/logger.h"
#include "libpandafile/class_data_accessor.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"

namespace panda {

std::ostream &operator<<(std::ostream &os, const Class::State &state)
{
    switch (state) {
        case Class::State::INITIAL: {
            os << "INITIAL";
            break;
        }
        case Class::State::LOADED: {
            os << "LOADED";
            break;
        }
        case Class::State::VERIFIED: {
            os << "VERIFIED";
            break;
        }
        case Class::State::INITIALIZING: {
            os << "INITIALIZING";
            break;
        }
        case Class::State::ERRONEOUS: {
            os << "ERRONEOUS";
            break;
        }
        case Class::State::INITIALIZED: {
            os << "INITIALIZED";
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
    }

    return os;
}

Class::UniqId Class::CalcUniqId(const panda_file::File *file, panda_file::File::EntityId file_id)
{
    constexpr uint64_t HALF = 32ULL;
    uint64_t uid = file->GetUniqId();
    uid <<= HALF;
    uid |= file_id.GetOffset();
    return uid;
}

Class::UniqId Class::CalcUniqId(const uint8_t *descriptor)
{
    uint64_t uid;
    uid = GetHash32String(descriptor);
    constexpr uint64_t HALF = 32ULL;
    constexpr uint64_t NO_FILE = 0xFFFFFFFFULL << HALF;
    uid = NO_FILE | uid;
    return uid;
}

Class::UniqId Class::CalcUniqId() const
{
    if (panda_file_ != nullptr) {
        return CalcUniqId(panda_file_, file_id_);
    }
    return CalcUniqId(descriptor_);
}

Class::Class(const uint8_t *descriptor, panda_file::SourceLang lang, uint32_t vtable_size, uint32_t imt_size,
             uint32_t size)
    : BaseClass(lang), descriptor_(descriptor), vtable_size_(vtable_size), imt_size_(imt_size), class_size_(size)
{
    // Initializa all static fields with 0 value.
    auto statics_offset = GetStaticFieldsOffset();
    auto sp = GetClassSpan();
    ASSERT(sp.size() >= statics_offset);
    auto size_to_set = sp.size() - statics_offset;
    if (size_to_set > 0) {
        (void)memset_s(&sp[statics_offset], size_to_set, 0, size_to_set);
    }
}
void Class::SetState(Class::State state)
{
    if (state_ == State::ERRONEOUS || state <= state_) {
        LOG(FATAL, RUNTIME) << "Invalid class state transition " << state_ << " -> " << state;
    }

    state_ = state;
}

std::string Class::GetName() const
{
    return ClassHelper::GetName(descriptor_);
}

void Class::DumpClass(std::ostream &os, size_t flags)
{
    if ((flags & DUMPCLASSFULLDETAILS) == 0) {
        os << GetDescriptor();
        if ((flags & DUMPCLASSCLASSLODER) != 0) {
            LOG(INFO, RUNTIME) << " Panda can't get classloader at now\n";
        }
        if ((flags & DUMPCLASSINITIALIZED) != 0) {
            LOG(INFO, RUNTIME) << " There is no status structure of class in Panda at now\n";
        }
        os << "\n";
        return;
    }
    os << "\n";
    os << "----- " << (IsInterface() ? "interface" : "class") << " "
       << "'" << GetDescriptor() << "' -----\n";
    os << "  objectSize=" << BaseClass::GetObjectSize() << " \n";
    os << "  accessFlags=" << GetAccessFlags() << " \n";
    if (IsArrayClass()) {
        os << "  componentType=" << GetComponentType() << "\n";
    }
    size_t num_direct_interfaces = this->num_ifaces_;
    if (num_direct_interfaces > 0) {
        os << "  interfaces (" << num_direct_interfaces << "):\n";
    }
    if (!IsLoaded()) {
        os << "  class not yet loaded";
    } else {
        os << "  vtable (" << this->GetVTable().size() << " entries)\n";
        if (this->num_sfields_ > 0) {
            os << "  static fields (" << this->num_sfields_ << " entries)\n";
        }
        if (this->num_fields_ - this->num_sfields_ > 0) {
            os << "  instance fields (" << this->num_fields_ - this->num_sfields_ << " entries)\n";
        }
    }
}

Class *Class::FromClassObject(const ObjectHeader *obj)
{
    return Runtime::GetCurrent()->GetClassLinker()->ObjectToClass(obj);
}

size_t Class::GetClassObjectSizeFromClass(Class *cls)
{
    return Runtime::GetCurrent()->GetClassLinker()->GetClassObjectSize(cls);
}

}  // namespace panda
