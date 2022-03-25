# Copyright (c) 2021-2022 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(isa)

set(ISA_FILE "${CMAKE_BINARY_DIR}/isa/isa.yaml")
set(ISA_CORE_FILE "${PROJECT_SOURCE_DIR}/isa.yaml")
set(ISA_ASSERTS "${PROJECT_SOURCE_DIR}/asserts.rb")
set(ISA_COMBINE "${PROJECT_SOURCE_DIR}/combine.rb")
set(ISAPI "${PROJECT_SOURCE_DIR}/isapi.rb")
set(ISA_PLUGIN_FILES "")

string(REPLACE ";" "," ISA_PLUGIN_FILES_STR "${ISA_PLUGIN_FILES}")

# ISA combine
add_custom_command(OUTPUT ${ISA_FILE}
    COMMENT "Combine ISA files"
    COMMAND ${ISA_COMBINE} -d "${ISA_CORE_FILE},${ISA_PLUGIN_FILES_STR}" -o ${ISA_FILE}
    DEPENDS ${ISA_COMBINE} ${ISA_CORE_FILE} ${ISA_PLUGIN_FILES}
)
add_custom_target(isa_combine DEPENDS ${ISA_FILE})

# Check ISA_FILE content

set(ASSERT_MARKER "${PROJECT_BINARY_DIR}/assert.ok")
set(ISA_GEN_RB "${PROJECT_SOURCE_DIR}/gen.rb")
add_custom_command(OUTPUT ${ASSERT_MARKER}
    COMMENT "Check ISA content"
    COMMAND ${ISA_GEN_RB} -d ${ISA_FILE} -r ${ISAPI} -a ${ISA_ASSERTS}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSERT_MARKER}
    DEPENDS ${ISA_FILE} ${ISA_GEN_RB} ${ISA_ASSERTS} ${ISAPI}
)
add_custom_target(isa_assert DEPENDS ${ASSERT_MARKER})
add_dependencies(isa_assert isa_combine)

# Template generation
set(TEMPLATES
    instructions.txt.erb
    isa.md.erb
)
panda_isa_gen(TEMPLATES ${TEMPLATES})
