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

// call implemented interpreter execution directly
// doesn't support DWARF in x86 platform
.global ExecuteImplStub
.type ExecuteImplStub, %function
ExecuteImplStub:
    // arg0: ESP+4:  thread
    // arg1: ESP+8:  PC
    // arg2: ESP+12: I-frame
    // arg3: ESP+16: impl
    xor %eax, %eax
    mov 20(%esp), %eax
    call *%eax
    ret

