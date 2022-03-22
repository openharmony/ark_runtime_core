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

#ifndef PANDA_LIBPANDAFILE_EXTERNAL_PANDA_FILE_EXTERNAL_H_
#define PANDA_LIBPANDAFILE_EXTERNAL_PANDA_FILE_EXTERNAL_H_

#include "file_ext.h"
#include <vector>
#include <memory>

namespace panda_api::panda_file {

#ifdef __cplusplus
extern "C" {
#endif

void LoadPandFileExt();

#ifdef __cplusplus
}  // extern "C"
#endif

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class PandaFileWrapper {
public:
    static std::unique_ptr<PandaFileWrapper> OpenPandafileFromMemory(void *addr, uint64_t *size,
                                                                     const std::string &file_name)
    {
        if (pOpenPandafileFromMemoryExt == nullptr) {
            LoadPandFileExt();
        }

        PandaFileExt *pf_ext = nullptr;
        std::unique_ptr<PandaFileWrapper> pfw;
        auto ret = pOpenPandafileFromMemoryExt(addr, size, file_name, &pf_ext);
        if (ret) {
            // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
            pfw.reset(new PandaFileWrapper(pf_ext));
        }
        return pfw;
    }

    static std::unique_ptr<PandaFileWrapper> OpenPandafileFromFd(int fd, uint64_t offset, const std::string &file_name)
    {
        if (pOpenPandafileFromFdExt == nullptr) {
            LoadPandFileExt();
        }

        PandaFileExt *pf_ext = nullptr;
        std::unique_ptr<PandaFileWrapper> pfw;
        auto ret = pOpenPandafileFromFdExt(fd, offset, file_name, &pf_ext);
        if (ret) {
            // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
            pfw.reset(new PandaFileWrapper(pf_ext));
        }
        return pfw;
    }

    MethodSymInfoExt QueryMethodSymByOffset(uint64_t offset)
    {
        MethodSymInfoExt method_info {0, 0, std::string()};
        if (pQueryMethodSymByOffsetExt == nullptr) {
            return {0, 0, std::string()};
        }
        auto ret = pQueryMethodSymByOffsetExt(pf_ext_, offset, &method_info);
        if (ret) {
            return method_info;
        }
        return {0, 0, std::string()};
    }

    std::vector<MethodSymInfoExt> QueryAllMethodSyms()
    {
        std::vector<MethodSymInfoExt> method_infos;
        if (pQueryAllMethodSymsExt == nullptr) {
            return method_infos;
        }
        pQueryAllMethodSymsExt(pf_ext_, AppendMethodInfo, static_cast<void *>(&method_infos));
        return method_infos;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    static decltype(OpenPandafileFromFdExt) *pOpenPandafileFromFdExt;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static decltype(OpenPandafileFromMemoryExt) *pOpenPandafileFromMemoryExt;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static decltype(QueryMethodSymByOffsetExt) *pQueryMethodSymByOffsetExt;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static decltype(QueryAllMethodSymsExt) *pQueryAllMethodSymsExt;

    ~PandaFileWrapper() = default;

private:
    explicit PandaFileWrapper(PandaFileExt *pf_ext) : pf_ext_(pf_ext) {}
    PandaFileExt *pf_ext_;

    // callback
    static void AppendMethodInfo(MethodSymInfoExt *method_info, void *user_data)
    {
        auto method_infos = reinterpret_cast<std::vector<MethodSymInfoExt> *>(user_data);
        method_infos->push_back(*method_info);
    }

    friend void LoadPandFileExt();
};

}  // namespace panda_api::panda_file

#endif  // PANDA_LIBPANDAFILE_EXTERNAL_PANDA_FILE_EXTERNAL_H_
