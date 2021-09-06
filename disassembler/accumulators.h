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

#ifndef PANDA_DISASSEMBLER_ACCUMULATORS_H_
#define PANDA_DISASSEMBLER_ACCUMULATORS_H_

#include <map>
#include <string>
#include <vector>
#include <utility>

namespace panda::disasm {
using LabelTable = std::map<size_t, std::string>;
using IdList = std::vector<panda::panda_file::File::EntityId>;
using AnnotationList = std::vector<std::pair<std::string, std::string>>;

struct MethodInfo {
    std::string method_info;

    std::vector<std::string> instructions_info;
};

struct RecordInfo {
    std::string record_info;

    std::vector<std::string> fields_info;
};

struct ProgInfo {
    std::map<std::string, RecordInfo> records_info;
    std::map<std::string, MethodInfo> methods_info;
};

struct RecordJavaAnnotations {
    AnnotationList ann_list;
    std::map<std::string, AnnotationList> field_annotations;
};

struct ProgJavaAnnotations {
    std::map<std::string, AnnotationList> method_annotations;
    std::map<std::string, RecordJavaAnnotations> record_annotations;
};
}  // namespace panda::disasm

#endif  // PANDA_DISASSEMBLER_ACCUMULATORS_H_
