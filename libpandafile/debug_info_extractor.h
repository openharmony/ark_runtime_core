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

#ifndef PANDA_LIBPANDAFILE_DEBUG_INFO_EXTRACTOR_H_
#define PANDA_LIBPANDAFILE_DEBUG_INFO_EXTRACTOR_H_

#include "file.h"

#include <string>
#include <vector>
#include <list>

namespace panda::panda_file {

struct LineTableEntry {
    uint32_t offset;
    size_t line;
};

using LineNumberTable = std::vector<LineTableEntry>;

struct LocalVariableInfo {
    std::string name;
    std::string type;
    std::string type_signature;
    int32_t reg_number;
    uint32_t start_offset;
    uint32_t end_offset;
};

using LocalVariableTable = std::vector<LocalVariableInfo>;

class DebugInfoExtractor {
public:
    explicit DebugInfoExtractor(const File *pf);

    ~DebugInfoExtractor() = default;

    DEFAULT_COPY_SEMANTIC(DebugInfoExtractor);
    DEFAULT_MOVE_SEMANTIC(DebugInfoExtractor);

    const LineNumberTable &GetLineNumberTable(File::EntityId method_id) const;

    const LocalVariableTable &GetLocalVariableTable(File::EntityId method_id) const;

    const std::vector<std::string> &GetParameterNames(File::EntityId method_id) const;

    const char *GetSourceFile(File::EntityId method_id) const;

    const char *GetSourceCode(File::EntityId method_id) const;

    std::vector<File::EntityId> GetMethodIdList() const;

private:
    void Extract(const File *pf);

    struct MethodDebugInfo {
        std::string source_file;
        std::string source_code;
        File::EntityId method_id;
        LineNumberTable line_number_table;
        LocalVariableTable local_variable_table;
        std::vector<std::string> param_names;
    };

    std::list<MethodDebugInfo> methods_;
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_DEBUG_INFO_EXTRACTOR_H_
