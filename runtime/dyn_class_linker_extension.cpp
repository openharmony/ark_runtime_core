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

#include "runtime/dyn_class_linker_extension.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/coretypes/native_pointer.h"
#include "runtime/include/panda_vm.h"

namespace panda {
using Array = coretypes::Array;
using NativePointer = coretypes::NativePointer;

using Type = panda_file::Type;
using SourceLang = panda_file::SourceLang;

DynamicClassLinkerExtension::~DynamicClassLinkerExtension()
{
    if (!IsInitialized()) {
        return;
    }

    FreeLoadedClasses();
}

bool DynamicClassLinkerExtension::InitializeImpl([[maybe_unused]] bool cmpStrEnabled)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(GetLanguage());

    auto *classClass = CreateClass(ctx.GetClassClassDescriptor(), GetClassVTableSize(ClassRoot::CLASS),
                                   GetClassIMTSize(ClassRoot::CLASS), GetClassSize(ClassRoot::CLASS));
    coretypes::Class::FromRuntimeClass(classClass)->SetClass(classClass);
    classClass->SetSourceLang(SourceLang::ECMASCRIPT);
    classClass->SetState(Class::State::LOADED);
    classClass->SetLoadContext(GetBootContext());
    GetClassLinker()->AddClassRoot(ClassRoot::CLASS, classClass);

    auto *objClass = CreateClass(ctx.GetObjectClassDescriptor(), GetClassVTableSize(ClassRoot::OBJECT),
                                 GetClassIMTSize(ClassRoot::OBJECT), GetClassSize(ClassRoot::OBJECT));
    objClass->SetObjectSize(ObjectHeader::ObjectHeaderSize());
    objClass->SetSourceLang(SourceLang::ECMASCRIPT);
    classClass->SetBase(objClass);
    objClass->SetState(Class::State::LOADED);
    objClass->SetLoadContext(GetBootContext());
    GetClassLinker()->AddClassRoot(ClassRoot::OBJECT, objClass);
    return true;
}

void DynamicClassLinkerExtension::InitializeArrayClass(Class *arrayClass, Class *componentClass)
{
    ASSERT(IsInitialized());

    auto *objectClass = GetClassRoot(ClassRoot::OBJECT);
    arrayClass->SetBase(objectClass);
    arrayClass->SetComponentType(componentClass);
    uint32_t access_flags = componentClass->GetAccessFlags() & ACC_FILE_MASK;
    access_flags &= ~ACC_INTERFACE;
    access_flags |= ACC_FINAL | ACC_ABSTRACT;
    arrayClass->SetAccessFlags(access_flags);
    arrayClass->SetState(Class::State::INITIALIZED);
}

void DynamicClassLinkerExtension::InitializePrimitiveClass(Class *primitiveClass)
{
    ASSERT(IsInitialized());

    primitiveClass->SetAccessFlags(ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);
    primitiveClass->SetState(Class::State::INITIALIZED);
}

size_t DynamicClassLinkerExtension::GetClassVTableSize([[maybe_unused]] ClassRoot root)
{
    ASSERT(IsInitialized());
    return 0;
}

size_t DynamicClassLinkerExtension::GetClassIMTSize([[maybe_unused]] ClassRoot root)
{
    ASSERT(IsInitialized());
    return 0;
}

size_t DynamicClassLinkerExtension::GetClassSize(ClassRoot root)
{
    ASSERT(IsInitialized());

    switch (root) {
        case ClassRoot::U1:
        case ClassRoot::I8:
        case ClassRoot::U8:
        case ClassRoot::I16:
        case ClassRoot::U16:
        case ClassRoot::I32:
        case ClassRoot::U32:
        case ClassRoot::I64:
        case ClassRoot::U64:
        case ClassRoot::F32:
        case ClassRoot::F64:
        case ClassRoot::TAGGED:
            return ClassHelper::ComputeClassSize(GetClassVTableSize(root), GetClassIMTSize(root), 0, 0, 0, 0, 0, 0);
        case ClassRoot::ARRAY_U1:
        case ClassRoot::ARRAY_I8:
        case ClassRoot::ARRAY_U8:
        case ClassRoot::ARRAY_I16:
        case ClassRoot::ARRAY_U16:
        case ClassRoot::ARRAY_I32:
        case ClassRoot::ARRAY_U32:
        case ClassRoot::ARRAY_I64:
        case ClassRoot::ARRAY_U64:
        case ClassRoot::ARRAY_F32:
        case ClassRoot::ARRAY_F64:
        case ClassRoot::ARRAY_TAGGED:
        case ClassRoot::ARRAY_CLASS:
        case ClassRoot::ARRAY_STRING:
            return GetArrayClassSize();
        case ClassRoot::OBJECT:
        case ClassRoot::CLASS:
        case ClassRoot::STRING:
            return ClassHelper::ComputeClassSize(GetClassVTableSize(root), GetClassIMTSize(root), 0, 0, 0, 0, 0, 0);
        default: {
            UNREACHABLE();
            break;
        }
    }

    return 0;
}

size_t DynamicClassLinkerExtension::GetArrayClassVTableSize()
{
    ASSERT(IsInitialized());

    return GetClassVTableSize(ClassRoot::OBJECT);
}

size_t DynamicClassLinkerExtension::GetArrayClassSize()
{
    ASSERT(IsInitialized());

    return GetClassSize(ClassRoot::OBJECT);
}

Class *DynamicClassLinkerExtension::CreateClass(const uint8_t *descriptor, size_t vtableSize, size_t imt_size,
                                                size_t size)
{
    ASSERT(IsInitialized());

    auto vm = Thread::GetCurrent()->GetVM();
    auto *heap_manager = vm->GetHeapManager();
    auto object_header =
        heap_manager->AllocateNonMovableObject(GetClassRoot(ClassRoot::CLASS), coretypes::Class::GetSize(size));
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto *res = reinterpret_cast<coretypes::Class *>(object_header);
    res->InitClass(descriptor, vtableSize, imt_size, size);
    auto *klass = res->GetRuntimeClass();
    klass->SetManagedObject(res);
    klass->SetSourceLang(GetLanguage());
    return klass;
}

void DynamicClassLinkerExtension::FreeClass(Class *klass)
{
    ASSERT(IsInitialized());

    auto *cls = coretypes::Class::FromRuntimeClass(klass);
    auto allocator = GetClassLinker()->GetAllocator();
    allocator->Free(cls);
}

void DynamicClassLinkerExtension::ErrorHandler::OnError([[maybe_unused]] ClassLinker::Error error,
                                                        [[maybe_unused]] const PandaString &message)
{
}
}  // namespace panda
