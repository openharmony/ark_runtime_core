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

#ifndef PANDA_RUNTIME_CLASS_INITIALIZER_H_
#define PANDA_RUNTIME_CLASS_INITIALIZER_H_

#include "runtime/include/class-inl.h"

namespace panda {

class ClassLinker;

class ClassInitializer {
public:
    static bool Initialize(ClassLinker *class_linker, ManagedThread *thread, Class *klass);

    static bool InitializeFields(Class *klass);

private:
    static bool InitializeInterface(ClassLinker *class_linker, ManagedThread *thread, Class *iface);

    static bool VerifyClass(Class *klass);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_CLASS_INITIALIZER_H_
