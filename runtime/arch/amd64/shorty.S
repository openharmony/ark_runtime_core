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

#define DEFAULT_SHORTY_PTR_REG rax
#define DEFAULT_SHORTY_REG r10d

.macro INIT_SHORTY_REG shorty_reg = %DEFAULT_SHORTY_REG, shorty_ptr_reg = %DEFAULT_SHORTY_PTR_REG
    movzwl (\shorty_ptr_reg), \shorty_reg // read new shorty value and advance shorty_ptr
    addq $2, \shorty_ptr_reg
.endm

.macro NEXT_SHORTY out_reg, shorty_reg = %DEFAULT_SHORTY_REG, shorty_ptr_reg = %DEFAULT_SHORTY_PTR_REG
    cmpl $-1, \shorty_reg // check \shorty_reg = 0xFFFFFFFF
    jne 1f
    movzwl (\shorty_ptr_reg), \shorty_reg // read new shorty value and advance shorty_ptr
    addq $2, \shorty_ptr_reg
1:
    movl \shorty_reg, \out_reg
    andl $0xF, \out_reg // extract next shorty value
    shrl $4, \shorty_reg
    orl $0xFFFFF000, \shorty_reg // fill the high 20 bits by 0xFFFFF
.endm

.macro SKIP_SHORTY shorty_reg = %DEFAULT_SHORTY_REG, shorty_ptr_reg = %DEFAULT_SHORTY_PTR_REG
    cmpl $-1, \shorty_reg // check \shorty_reg = 0xFFFFFFFF
    jne 1f
    movzwl (\shorty_ptr_reg), \shorty_reg // read new shorty value and advance shorty_ptr
    addq $2, \shorty_ptr_reg
1:
    shrl $4, \shorty_reg
    orl $0xFFFFF000, \shorty_reg // fill the high 20 bits by 0xFFFFF
.endm
