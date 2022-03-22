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

#include "arch/asm_support.h"
#include "arch/x86/shorty.S"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

.macro PUSH_ARG arg_ptr, stack_ptr, tmp1, tmp2
    cmpl $SHORTY_TAGGED, %ecx
    je 2f

    subl $SHORTY_FIRST_64, %ecx
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %ecx
    jbe 1f

    // it is a 32bit value
    movl (\arg_ptr), \tmp1
    movl \tmp1, (\stack_ptr)
    addl $4, \stack_ptr
    jmp 3f
1:
    // it is a 64bit value
    movl (\arg_ptr), \tmp1
    movl 4(\arg_ptr), \tmp2
    movl \tmp1, (\stack_ptr)
    movl \tmp2, 4(\stack_ptr)
    addl $8, \stack_ptr
    jne 3f

2:  // it is a value of type 'any'
    // copy the value
    movl (\arg_ptr), \tmp1
    movl 4(\arg_ptr), \tmp2
    movl \tmp1, (\stack_ptr)
    movl \tmp2, 4(\stack_ptr)
    // copy the tag
    movl 8(\arg_ptr), \tmp1
    movl 12(\arg_ptr), \tmp2
    movl \tmp1, 8(\stack_ptr)
    movl \tmp2, 12(\stack_ptr)
    addl $16, \stack_ptr

3:
.endm

// The macro reserves stack space for the arguments
// Input:
// %eax - SHORTY_PTR_REG
// %edx - SHORTY_REG
// %ecx - shorty value (no initialization needed)
// %ebx - method
// The macro musn't use other registers
.macro PREPARE_ARG_STACK
    // check the return type
    NEXT_SHORTY %ecx

    cmpl $SHORTY_TAGGED, %ecx
    jne 1f
    // reserve space for the pointer to which the result will be stored
    subl $4, %esp

1:  subl $4, %esp // space for Method

    // parameter 'this' of instance methods is not encoded in the shorty
    // check whether the method is an instance
    movl METHOD_ACCESS_FLAGS_OFFSET(%ebx), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 3f
    // it is an instance method
    subl $4, %esp // reserve space for this

3:
    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je 6f

    cmpl $SHORTY_TAGGED, %ecx
    je 5f

    subl $SHORTY_FIRST_64, %ecx
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %ecx
    jbe 4f

    // it is a 32bit value
    subl $4, %esp
    jmp 3b
4:
    // it is a 64bit value
    subl $8, %esp
    jne 3b

5:  // it is 'any'
    subl $16, %esp
    jne 3b

6:
    // align arg slot 16 byte
    andl $0xFFFFFFF0, %esp
.endm

// void InterpreterToCompiledCodeBridge(const BytecodeInstruction* insn, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InterpreterToCompiledCodeBridge
.type InterpreterToCompiledCodeBridge, %function
InterpreterToCompiledCodeBridge:
    // %esp % 16 == 12 here (-4 == 12 (mod 16))
    movl %esp, %eax

    pushl 8(%eax) // iframe*

    // According to the current frame kind set the bridge type
    movl 16(%eax), %ecx
    movb THREAD_FRAME_KIND(%ecx), %cl
    testb %cl, %cl
    jz 1f
    pushl $BYPASS_BRIDGE
    jmp 2f
1:
    pushl $INTERPRETER_TO_COMPILED_CODE_BRIDGE
2:

    pushl %ebp
    pushl %THREAD_REG:thread_pointer@NTPOFF

    pushl %ebx
    pushl %esi
    pushl %edi
    // %esp should be 16-byte aligned here

    movl %eax, %ebp // set frame pointer

    movl 16(%ebp), %eax // thread*
    movl %eax, %THREAD_REG:thread_pointer@NTPOFF

    movl 12(%ebp), %ebx // method*
    movl METHOD_SHORTY_OFFSET(%ebx), %SHORTY_PTR_REG // method->shorty*
    INIT_SHORTY_REG

    PREPARE_ARG_STACK

    // setup regs and memory as follow
    // %eax - SHORTY_PTR_REG, %edx - SHORTY_REG, %ecx - shorty value, (%esp) - insn_ptr,
    // 8(%ebp) - iframe, 4(%esp) - iframe->vregs[], %edi - pointer to stack
    movl METHOD_SHORTY_OFFSET(%ebx), %SHORTY_PTR_REG // method->shorty*
    INIT_SHORTY_REG

    movl %esp, %edi
    subl $8, %esp

    movl 8(%ebp), %ebx // iframe*
    addl $FRAME_VREGS_OFFSET, %ebx // iframe->vregs[]
    pushl %ebx

    // check the return type
    NEXT_SHORTY %ecx
    cmpl $SHORTY_TAGGED, %ecx
    jne 1f
    // the return type is 'any'
    movl 8(%ebp), %ebx
    addl $FRAME_ACC_OFFSET, %ebx // iframe->acc
    movl %ebx, (%edi)
    addl $4, %edi

1:  movl 12(%ebp), %ebx // method*
    movl %ebx, (%edi) // push method to the stack
    addl $4, %edi

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    // check whether the method is an instance
    movl METHOD_ACCESS_FLAGS_OFFSET(%ebx), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 1f
    // it is an instance method
    // replace the return type by REF
    shll $4, %SHORTY_REG // unshift SHORTY_REG back
    orl $SHORTY_REFERENCE, %SHORTY_REG

1:  movl 4(%ebp), %esi // insn*
    movzbl (%esi), %ebx // read opcode and advance insn_ptr
    addl $1, %esi
    pushl %esi

    // The file contains code which checks opcode and jumps
    // to the corresponding handler.
    // At the end each handler jumps to .Linvoke_from_bridge label.
    // The file is autogenerated from runtime/templates/bridge_dispatch.S.erb
    // Handlers are distinguished by format and located in the corresponding files with name:
    // handle_call_<format>.S
    // If you get a compilation error that there is no such file it seems
    // new call format was introduced and you have to implement the corresponding handler.
#include "bridge_dispatch_x86.S"

.Linvoke_from_bridge:
    addl $16, %esp
    // %esp should be 16-byte aligned here
    movl 12(%ebp), %ebx // method*
    movl METHOD_SHORTY_OFFSET(%ebx), %esi // method->shorty*
    movl METHOD_COMPILED_ENTRY_POINT_OFFSET(%ebx), %ebx // method->entry_point*

    calll *%ebx

    // handle the result
    // setup registers as follow
    // %eax, %edx - result, %esi - shorty[0] & 0xF, %edi - frame.acc, %ebx, %ecx - temp
    movzbl (%esi), %esi
    andl $0xF, %esi

    cmpl $SHORTY_VOID, %esi
    je .Lreturn
    cmpl $SHORTY_TAGGED, %esi
    je .Lreturn

    movl 8(%ebp), %edi // iframe*
    addl $FRAME_ACC_OFFSET, %edi

    xorl %ecx, %ecx
    cmpl $SHORTY_REFERENCE, %esi
    sete %cl
    movl %ecx, FRAME_VREGISTER_TAG_OFFSET(%edi)
    je .L32bit_arg

    movl %esi, %ebx
    movl %esi, %ecx

    subl $SHORTY_FIRST_FLOAT, %ebx
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %ebx
    jbe .Lfloat

    subl $SHORTY_FIRST_64, %esi
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %esi
    jbe .L64bit_arg

    cmpl $SHORTY_I32, %ecx
    jae .L32bit_arg

    // less than 32-bit
    cmpl $SHORTY_I16, %ecx
    je .LI16
    ja .LU16

    cmpl $SHORTY_I8, %ecx
    je .LI8
    jne .LU1_U8

.LU1_U8:
    movzbl %al, %eax
    jmp .L32bit_arg
.LI8:
    movsbl %al, %eax
    jmp .L32bit_arg
.LI16:
    movswl %ax, %eax
    jmp .L32bit_arg
.LU16:
    movzwl %ax, %eax
.L32bit_arg:
    movl %eax, (%edi)
    jmp .Lreturn
.Lfloat:
    fstpl (%edi)
    jmp .Lreturn
.L64bit_arg:
    movl %eax, (%edi)
    movl %edx, 4(%edi)

.Lreturn:
    leal -28(%ebp), %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %THREAD_REG:thread_pointer@NTPOFF
    popl %ebp

    addl $8, %esp
    retl


// InvocationResult InvokeCompiledCodeWithArgArray(const int64_t* args, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InvokeCompiledCodeWithArgArray
.type InvokeCompiledCodeWithArgArray, %function
InvokeCompiledCodeWithArgArray:
    // Since Invocation result is 128bit structure the first argument is a pointer to
    // memory where the result must be stored

    // %esp % 16 == 12 here (-4 == 12 (mod 16))
    movl %esp, %eax

    pushl 12(%eax) // iframe*

    // According to the current frame kind set the bridge type
    movl 20(%eax), %ecx
    movb THREAD_FRAME_KIND(%ecx), %cl
    testb %cl, %cl
    jz 1f
    pushl $BYPASS_BRIDGE
    jmp 2f
1:
    pushl $INTERPRETER_TO_COMPILED_CODE_BRIDGE
2:

    pushl %ebp
    pushl %THREAD_REG:thread_pointer@NTPOFF

    pushl %ebx
    pushl %esi
    pushl %edi
    // %esp should be 16-byte aligned here

    movl %eax, %ebp // set frame pointer

    movl 20(%ebp), %eax // thread*
    movl %eax, %THREAD_REG:thread_pointer@NTPOFF

    movl 16(%ebp), %ebx // method*
    movl METHOD_SHORTY_OFFSET(%ebx), %esi // method->shorty*

1:
    movl METHOD_SHORTY_OFFSET(%ebx), %SHORTY_PTR_REG // method->shorty*
    INIT_SHORTY_REG

    PREPARE_ARG_STACK

    // setup regs and memory as follow
    // %eax - SHORTY_PTR_REG, %edx - SHORTY_REG, %ecx - shorty value, %esi - args[],
    // %edi - pointer to stack
    movl METHOD_SHORTY_OFFSET(%ebx), %SHORTY_PTR_REG // method->shorty*
    INIT_SHORTY_REG

    movl 8(%ebp), %esi // args[]
    movl %esp, %edi
    subl $8, %esp

    // check the return type
    NEXT_SHORTY %ecx
    cmpl $SHORTY_TAGGED, %ecx
    jne 1f
    // the return type is 'any'
    // push the pointer to memory where the result must be stored
    movl 4(%ebp), %ecx
    movl %ecx, (%edi)
    addl $4, %edi

1:  movl %ebx, (%edi) // push method to the stack
    addl $4, %edi

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in another case just skip the return type
    // check whether the method is an instance
    movl METHOD_ACCESS_FLAGS_OFFSET(%ebx), %ecx
    testl $ACCESS_STATIC, %ecx
    jne .Lloop_args_push
    // it is an instance method
    // replace the return type by REF
    shll $4, %SHORTY_REG // unshift SHORTY_REG back
    orl $SHORTY_REFERENCE, %SHORTY_REG

.Lloop_args_push:
    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je .Lloopend_args_push

    movl %eax, (%esp)
    movl %edx, 4(%esp)
    // handle the first arg index
    PUSH_ARG %esi, %edi, %eax, %edx
    movl (%esp), %eax
    movl 4(%esp), %edx

    addl $8, %esi
    jmp .Lloop_args_push
.Lloopend_args_push:
    movl METHOD_SHORTY_OFFSET(%ebx), %esi // method->shorty*
    addl $8, %esp

.Linvoke_with_args:
    movl METHOD_COMPILED_ENTRY_POINT_OFFSET(%ebx), %ebx // method->entry_point*
    calll *%ebx

    // handle the result
    // setup registers as follow
    // %eax, %edx - result, %esi - shorty[0] & 0xF, %edi - tag, %ebx - temp
    movzbl (%esi), %esi
    andl $0xF, %esi

    cmpl $SHORTY_VOID, %esi
    je .Lreturn_
    cmpl $SHORTY_TAGGED, %esi
    je .Lreturn_

    // get tag in %edi
    xor %ebx, %ebx
    cmpl $SHORTY_REFERENCE, %esi
    sete %bl
    movl %ebx, %edi

    movl %esi, %ebx
    movl %esi, %ecx

    subl $SHORTY_FIRST_FLOAT, %ebx
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %ebx
    jbe .Lfloat_

    subl $SHORTY_FIRST_64, %esi
    cmpl $(SHORTY_NUM_64BIT_TYPES - 1), %esi
    jbe .Lstore_result_

    // set high 32 bits of 64bit value to 0
    xorl %edx, %edx

    cmpl $SHORTY_I32, %ecx
    jae .Lstore_result_ // i32, u32 or ref

    // less than 32-bit
    cmpl $SHORTY_I16, %ecx
    je .LI16_
    ja .LU16_

    cmpl $SHORTY_I8, %ecx
    je .LI8_
    jne .LU1_U8_

.LU1_U8_:
    movzbl %al, %eax
    jmp .Lstore_result_
.LI8_:
    movsbl %al, %eax
    jmp .Lstore_result_
.LI16_:
    movswl %ax, %eax
    jmp .Lstore_result_
.LU16_:
    movzwl %ax, %eax
    jmp .Lstore_result_
.Lfloat_:
    subl $8, %esp
    fstpl (%esp)
    movl (%esp), %eax
    movl 4(%esp), %edx
    addl $8, %esp

.Lstore_result_:
    // get result ptr in %ebx
    movl 4(%ebp), %ebx
    // store value
    movl %eax, (%ebx)
    movl %edx, 4(%ebx)
    // store tag
    movl %edi, 8(%ebx)
    xorl %edi, %edi
    movl %edi, 12(%ebx)

.Lreturn_:
    leal -28(%ebp), %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %THREAD_REG:thread_pointer@NTPOFF
    popl %ebp

    addl $8, %esp
    // return and pop the pointer to the result
    retl $4

.type thread_pointer,@object
.section .tbss,"awT",@nobits
.globl thread_pointer
.p2align 2
thread_pointer:
    .long 0
    .size  thread_pointer, 4
