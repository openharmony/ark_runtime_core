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

#include "runtime/tests/interpreter/test_runtime_interface.h"

namespace panda::interpreter::test {

RuntimeInterface::NullPointerExceptionData RuntimeInterface::npe_data;

RuntimeInterface::ArrayIndexOutOfBoundsExceptionData RuntimeInterface::array_oob_exception_data;

RuntimeInterface::NegativeArraySizeExceptionData RuntimeInterface::array_neg_size_exception_data;

RuntimeInterface::ArithmeticException RuntimeInterface::arithmetic_exception_data;

RuntimeInterface::ClassCastExceptionData RuntimeInterface::class_cast_exception_data;

RuntimeInterface::AbstractMethodError RuntimeInterface::abstract_method_error_data;

RuntimeInterface::ArrayStoreExceptionData RuntimeInterface::array_store_exception_data;

coretypes::Array *RuntimeInterface::array_object;

Class *RuntimeInterface::array_class;

uint32_t RuntimeInterface::array_length;

Class *RuntimeInterface::resolved_class;

ObjectHeader *RuntimeInterface::object;

Class *RuntimeInterface::object_class;

uint32_t RuntimeInterface::catch_block_pc_offset;

RuntimeInterface::InvokeMethodHandler RuntimeInterface::invoke_handler;

DummyGC::DummyGC(panda::mem::ObjectAllocatorBase *object_allocator, const panda::mem::GCSettings &settings)
    : GC(object_allocator, settings)
{
}

DummyGC RuntimeInterface::dummy_gc(nullptr, panda::mem::GCSettings());

Method *RuntimeInterface::resolved_method;

Field *RuntimeInterface::resolved_field;

uint32_t RuntimeInterface::jit_threshold;

}  // namespace panda::interpreter::test
