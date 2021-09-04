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

#include "arch/asm_support.h"
#include "arch/x86/shorty.S"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

// Frame* CreateFrameForMethod(Method* method, Frame* prev);
.extern CreateFrameForMethod
// void InterpreterEntryPoint(Method *method, Frame* frame);
.extern InterpreterEntryPoint

.extern IncrementHotnessCounter

.global CompiledCodeToInterpreterBridge
.type CompiledCodeToInterpreterBridge, %function
CompiledCodeToInterpreterBridge:
    // %esp % 16 == 12 here (-4 == 12 (mod 16))
    movl %esp, %eax

    // construct the frame
    pushl %ebp
    pushl $COMPILED_CODE_TO_INTERPRETER_BRIDGE

    movl %eax, %ebp // set frame pointer

    pushl 4(%ebp)
    calll IncrementHotnessCounter
    addl $4, %esp

    cmpb $0, %al
    je .Lnot_compiled

    addl $4, %esp
    popl %ebp

    movl 4(%esp), %eax
    movl METHOD_COMPILED_ENTRY_POINT_OFFSET(%eax), %eax
    jmp *%eax

.Lnot_compiled:

    // save all the callee saved registers to the stack
    // stack walker will read them during stack unwinding
    pushl %ebx
    pushl %esi
    pushl %edi
    // %esp % 16 == 8 here

    movl 4(%ebp), %ebx // method*
    leal -8(%ebp), %ecx // prev*

    pushl %ecx
    pushl %ebx
    // %esp should be 16-byte aligned here
    calll CreateFrameForMethod
    addl $8, %esp // cleanup

    pushl %eax // iframe*

    // setup regs as follow
    // %eax - SHORTY_PTR_REG, %edx - SHORTY_REG, %ecx - shorty value, %edi - iframe.vregs_ + num_vregs_,
    // %esi - args, (%esp) - iframe*
    addl $FRAME_VREGS_OFFSET, %eax
    movl FRAME_NUM_VREGS_OFFSET(%eax), %edi
    subl METHOD_NUM_ARGS_OFFSET(%ebx), %edi
    shll $4, %edi
    addl %eax, %edi // iframe.vregs_ + num_vregs_

    leal 8(%ebp), %esi // args

    movl METHOD_SHORTY_OFFSET(%ebx), %SHORTY_PTR_REG
    INIT_SHORTY_REG


    // fill in the iframe

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type

    // check whether the method is an instance
    movl METHOD_ACCESS_FLAGS_OFFSET(%ebx), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 1f

    // it is an instance method
    // replace the return type by REF
    andl $0xFFFFFFF0, %SHORTY_REG // clear the the least significant 4  bits
    orl $SHORTY_REFERENCE, %SHORTY_REG
    jmp .Lloop_copy_args

1:
    SKIP_SHORTY

.Lloop_copy_args:
    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je .Lloopend_copy_args

    subl $SHORTY_FIRST_64, %ecx
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %ecx
    jbe 1f

    // it is a 32bit value or reference
    // store the tag
    cmpl $(SHORTY_REFERENCE - SHORTY_FIRST_64), %ecx
    movl $0, %ecx
    sete %cl
    movl %ecx, FRAME_VREGISTER_TAG_OFFSET(%edi)

    movl (%esi), %ecx
    movl %ecx, (%edi)

    addl $4, %esi
    jmp 2f
1:
    // it is a 64bit value
    // store the tag
    movl $FRAME_VREGISTER_PRIMITIVE_TAG, FRAME_VREGISTER_TAG_OFFSET(%edi)

    movl (%esi), %ecx
    movl %ecx, (%edi)
    movl 4(%esi), %ecx
    movl %ecx, 4(%edi)

    addl $8, %esi
2:
    addl $FRAME_VREGISTER_SIZE, %edi
    jmp .Lloop_copy_args
.Lloopend_copy_args:

    movl (%esp), %esi // iframe*

    // call InterpreterEntryPoint
    // iframe*
    pushl %ebx // method*
    // %esp should be 16-byte aligned here
    calll InterpreterEntryPoint
    addl $8, %esp // cleanup

    // handle the result
    // setup regs as follow
    // %esi - iframe, %ebx - *method.shorty, %eax - &iframe.acc_
    leal FRAME_ACC_OFFSET(%esi), %eax
    movl METHOD_SHORTY_OFFSET(%ebx), %ebx
    movzbl (%ebx), %ebx
    andl $0xF, %ebx

    cmpl $SHORTY_VOID, %ebx
    jne 1f

    subl $4, %esp // alignment
    pushl %esi // iframe*
    calll FreeFrame
    addl $8, %esp // cleanup

    jmp 4f
1:
    movl %ebx, %ecx
    subl $SHORTY_FIRST_FLOAT, %ecx
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %ecx
    jbe 3f

    subl $SHORTY_FIRST_64, %ebx
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %ebx
    jbe 2f

    // result is 32 bit integer or reference
    movl (%eax), %ebx

    subl $4, %esp // alignment
    pushl %esi // iframe*
    calll FreeFrame
    addl $8, %esp // cleanup

    movl %ebx, %eax
    jmp 4f
2:
    // result is 64bit integer
    movl (%eax), %ebx
    movl 4(%eax), %edi

    subl $4, %esp // alignment
    pushl %esi // iframe*
    calll FreeFrame
    addl $8, %esp // cleanup

    movl %ebx, %eax
    movl %edi, %edx
    jmp 4f
3:
    // result is float
    movl (%eax), %ebx
    movl 4(%eax), %edi

    subl $4, %esp // alignment
    pushl %esi // iframe*
    calll FreeFrame
    addl $8, %esp // cleanup

    subl $8, %esp
    movl %ebx, (%esp)
    movl %edi, 4(%esp)
    fldl (%esp)
    addl $8, %esp
4:
    leal -20(%ebp), %esp

    popl %edi
    popl %esi
    popl %ebx
    addl $4, %esp
    popl %ebp

    retl
