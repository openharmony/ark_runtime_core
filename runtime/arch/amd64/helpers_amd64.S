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

.macro PUSH_GENERAL_REGS
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
.endm

.macro POP_GENERAL_REGS
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
.endm

.macro PUSH_FP_REGS
    subq $64, %rsp
    movsd %xmm0, (%rsp)
    movsd %xmm1, 8(%rsp)
    movsd %xmm2, 16(%rsp)
    movsd %xmm3, 24(%rsp)
    movsd %xmm4, 32(%rsp)
    movsd %xmm5, 40(%rsp)
    movsd %xmm6, 48(%rsp)
    movsd %xmm7, 56(%rsp)
.endm

.macro POP_FP_REGS
    movsd (%rsp), %xmm0
    movsd 8(%rsp), %xmm1
    movsd 16(%rsp), %xmm2
    movsd 24(%rsp), %xmm3
    movsd 32(%rsp), %xmm4
    movsd 40(%rsp), %xmm5
    movsd 48(%rsp), %xmm6
    movsd 56(%rsp), %xmm7
    addq $64, %rsp
.endm
