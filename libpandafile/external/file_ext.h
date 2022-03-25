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

#ifndef PANDA_LIBPANDAFILE_EXTERNAL_FILE_EXT_H_
#define PANDA_LIBPANDAFILE_EXTERNAL_FILE_EXT_H_

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

struct MethodSymInfoExt {
    uint64_t offset_;
    uint64_t length_;
    std::string name_;
};

struct PandaFileExt;

bool OpenPandafileFromMemoryExt(void *addr, const uint64_t *size, const std::string &file_name,
                                struct PandaFileExt **panda_file_ext);

bool OpenPandafileFromFdExt([[maybe_unused]] int fd, [[maybe_unused]] uint64_t offset, const std::string &file_name,
                            struct PandaFileExt **panda_file_ext);

bool QueryMethodSymByOffsetExt(struct PandaFileExt *pf, uint64_t offset, struct MethodSymInfoExt *method_info);

using MethodSymInfoExtCallBack = void(struct MethodSymInfoExt *, void *);

void QueryAllMethodSymsExt(PandaFileExt *pf, MethodSymInfoExtCallBack callback, void *user_data);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PANDA_LIBPANDAFILE_EXTERNAL_FILE_EXT_H_
