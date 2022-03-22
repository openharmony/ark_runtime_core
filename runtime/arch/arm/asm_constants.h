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

#ifndef PANDA_RUNTIME_ARCH_ARM_ASM_CONSTANTS_H_
#define PANDA_RUNTIME_ARCH_ARM_ASM_CONSTANTS_H_

// +---+----------------+
// |   |       LR       |
// |   |       FP       |
// |   |     Method *   |
// | c |      FLAGS     |
// | f +----------------+
// | r |       ...      |
// | a |      locals    |
// | m |       ...      |
// | e +--------+-------+ <-- CFRAME_CALLEE_REGS_START_SLOT
// | f |        | d15.1 |
// | r |        | d15.0 |
// | a |        | d14.1 |
// | m |        | d14.0 |
// | e |        | d13.1 |
// |   |        | d13.0 |
// |   |        | d12.1 |
// |   |        | d12.0 |
// |   |        | d11.1 |
// |   |        | d11.0 |
// |   |        | d10.1 |
// |   | callee | d10.0 |
// |   |  saved |  d9.1 |
// |   |        |  d9.0 |
// |   |        |  d8.1 |
// |   |        |  d8.0 |
// |   |        |   r11 |
// |   |        |   r10 |
// |   |        |    r9 |
// |   |        |    r8 |
// |   |        |    r7 |
// |   |        |    r6 |
// |   |        |    r5 |
// |   |        |    r4 |
// +---+--------+-------+ <-- CFRAME_ARM_SOFTFP_CALLEE_REGS_OFFSET

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFRAME_ARM_SOFTFP_CALLEE_REGS_COUNT (16 + 8) /* [d8..d15] + [r4..r11] */
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFRAME_ARM_SOFTFP_CALLEE_REGS_OFFSET (4 * (CFRAME_CALLEE_REGS_START_SLOT + CFRAME_ARM_SOFTFP_CALLEE_REGS_COUNT))

// +---+----------------+
// |   |       LR       |
// |   |       FP       |
// |   |     Method *   |
// |   |      FLAGS     |
// |   +----------------+
// |   |      ...       |
// |   |     locals     |
// |   |      ...       |
// | c +--------+-------+ <-- CFRAME_CALLEE_REGS_START_SLOT
// | f |        | d15.1 |
// | r |        | d15.0 |
// | a |        | d14.1 |
// | m |        | d14.0 |
// | e |        | d13.1 |
// |   |        | d13.0 |
// |   |        | d12.1 |
// |   |        | d12.0 |
// |   |        | d11.1 |
// |   |        | d11.0 |
// |   |        | d10.1 |
// |   | callee | d10.0 |
// |   |  saved |  d9.1 |
// |   |        |  d9.0 |
// |   |        |  d8.1 |
// |   |        |  d8.0 |
// |   |        |   r11 |
// |   |        |   r10 |
// |   |        |    r9 |
// |   |        |    r8 |
// |   |        |    r7 |
// |   |        |    r6 |
// |   |        |    r5 |
// |   |        |    r4 |
// +---+--------+-------+ <-- CFRAME_ARM_HARD_CALLEE_REGS_OFFSET

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFRAME_ARM_HARD_CALLEE_REGS_COUNT (16 + 8) /* [d8..d15] + [r4..r11] */
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFRAME_ARM_HARD_CALLEE_REGS_OFFSET (4 * (CFRAME_CALLEE_REGS_START_SLOT + CFRAME_ARM_HARD_CALLEE_REGS_COUNT))

#endif  // PANDA_RUNTIME_ARCH_ARM_ASM_CONSTANTS_H_
