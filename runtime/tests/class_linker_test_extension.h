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

#ifndef PANDA_RUNTIME_TESTS_CLASS_LINKER_TEST_EXTENSION_H_
#define PANDA_RUNTIME_TESTS_CLASS_LINKER_TEST_EXTENSION_H_

#include "runtime/include/class_linker.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/include/panda_vm.h"

namespace panda::test {

class ClassLinkerTestExtension : public ClassLinkerExtension {
public:
    ClassLinkerTestExtension(ManagedThread *thread, panda_file::SourceLang lang)
        : ClassLinkerExtension(lang), thread_(thread)
    {
    }

    ~ClassLinkerTestExtension()
    {
        FreeLoadedClasses();
    }

    void InitializeArrayClass(Class *array_class, Class *component_class) override
    {
        auto *object_class = GetClassRoot(ClassRoot::OBJECT);
        array_class->SetBase(object_class);
        array_class->SetComponentType(component_class);
    }

    void InitializePrimitiveClass([[maybe_unused]] Class *primitive_class) override {}

    size_t GetClassVTableSize([[maybe_unused]] ClassRoot root) override
    {
        return 0;
    }

    size_t GetClassIMTSize([[maybe_unused]] ClassRoot root) override
    {
        return 0;
    }

    size_t GetClassSize(ClassRoot root) override
    {
        return ClassHelper::ComputeClassSize(GetClassVTableSize(root), GetClassIMTSize(root), 0, 0, 0, 0, 0, 0);
    }

    size_t GetArrayClassVTableSize() override
    {
        return GetClassVTableSize(ClassRoot::OBJECT);
    }

    size_t GetArrayClassIMTSize() override
    {
        return GetClassIMTSize(ClassRoot::OBJECT);
    }

    size_t GetArrayClassSize() override
    {
        return GetClassSize(ClassRoot::OBJECT);
    }

    Class *CreateClass(const uint8_t *descriptor, size_t vtable_size, size_t imt_size, size_t size) override
    {
        auto vm = thread_->GetVM();
        auto allocator = vm->GetHeapManager()->GetObjectAllocator();
        void *ptr = allocator->AllocateNonMovable(coretypes::Class::GetSize(size), DEFAULT_ALIGNMENT, nullptr);
        auto *res = reinterpret_cast<coretypes::Class *>(ptr);
        res->InitClass(descriptor, vtable_size, imt_size, size);
        vm->GetGC()->InitGCBits(res);
        res->SetClass(GetClassRoot(ClassRoot::CLASS));
        auto *klass = res->GetRuntimeClass();
        klass->SetManagedObject(res);
        klass->SetSourceLang(GetLanguage());
        AddCreatedClass(klass);
        return klass;
    }

    void FreeClass(Class *klass) override
    {
        RemoveCreatedClass(klass);
    }

    void InitializeClass(Class *) override {}

    const void *GetNativeEntryPointFor(panda::Method *) const override
    {
        return nullptr;
    }

    bool CanThrowException([[maybe_unused]] const Method *method) const override
    {
        return true;
    }

    ClassLinkerErrorHandler *GetErrorHandler() override
    {
        return nullptr;
    }

private:
    bool InitializeImpl(bool compressed_string_enabled) override;

    ManagedThread *thread_;
};

}  // namespace panda::test

#endif  // PANDA_RUNTIME_TESTS_CLASS_LINKER_TEST_EXTENSION_H_
