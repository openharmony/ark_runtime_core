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

#ifndef PANDA_ASSEMBLER_DEFINE_H_
#define PANDA_ASSEMBLER_DEFINE_H_

/* Implementation-specific definitions */

constexpr char PARSE_COMMENT_MARKER = '#';

constexpr char PARSE_AREA_MARKER = '.';

#define PANDA_ASSEMBLER_TYPES(_) \
    _("void", VOID)              \
    _("u1", U1)                  \
    _("u8", U8)                  \
    _("i8", I8)                  \
    _("u16", U16)                \
    _("i16", I16)                \
    _("u32", U32)                \
    _("i32", I32)                \
    _("u64", U64)                \
    _("i64", I64)                \
    _("f32", F32)                \
    _("f64", F64)                \
    _("any", TAGGED)

#define KEYWORDS_LIST(_)     \
    _(".catch", CATCH)       \
    _(".catchall", CATCHALL) \
    _(".language", LANG)     \
    _(".function", FUN)      \
    _(".record", REC)        \
    _(".field", FLD)

#endif  // PANDA_ASSEMBLER_DEFINE_H_
