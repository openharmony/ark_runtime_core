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

#define DEFAULT_SHORTY_PTR_REG x0
#define DEFAULT_SHORTY_REG w1

.macro INIT_SHORTY_REG shorty_reg = DEFAULT_SHORTY_REG, shorty_ptr_reg = DEFAULT_SHORTY_PTR_REG
    ldrh \shorty_reg, [\shorty_ptr_reg], #2 // read new shorty value and advance shorty_ptr
.endm

.macro NEXT_SHORTY out_reg, shorty_reg = DEFAULT_SHORTY_REG, shorty_ptr_reg = DEFAULT_SHORTY_PTR_REG
    cmn \shorty_reg, #1 // check shorty_reg = 0xFFFFFFFF
    bne 1f
    ldrh \shorty_reg, [\shorty_ptr_reg], #2 // read new shorty value and advance shorty_ptr
1:
    and \out_reg, \shorty_reg, #0xF // extract next shorty value
    lsr \shorty_reg, \shorty_reg, #4
    orr \shorty_reg, \shorty_reg, #0xFFFFF000 // fill the high 20 bits by 0xFFFFF
.endm

.macro SKIP_SHORTY shorty_reg = DEFAULT_SHORTY_REG, shorty_ptr_reg = DEFAULT_SHORTY_PTR_REG
    cmn \shorty_reg, #1 // check shorty_reg = 0xFFFFFFFF
    bne 1f
    ldrh \shorty_reg, [\shorty_ptr_reg], #2 // read new shorty value and advance shorty_ptr
1:
    lsr \shorty_reg, \shorty_reg, #4
    orr \shorty_reg, \shorty_reg, #0xFFFFF000 // fill the high 20 bits by 0xFFFFF
.endm
