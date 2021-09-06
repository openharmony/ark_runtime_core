# Copyright (c) 2021 Huawei Device Co., Ltd.
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

cmake_minimum_required(VERSION 3.5.2 FATAL_ERROR)

project(isa)

set(ISA_FILE "${PROJECT_SOURCE_DIR}/isa.yaml")
set(ISA_ASSERTS "${PROJECT_SOURCE_DIR}/asserts.rb")
set(ISAPI "${PROJECT_SOURCE_DIR}/isapi.rb")

# ISA YAML validation against the schema

set(ISA_SCHEMA "${PROJECT_SOURCE_DIR}/schema.json")

set(VALIDATE_MARKER "${PROJECT_BINARY_DIR}/validate.ok")
add_custom_command(OUTPUT ${VALIDATE_MARKER}
    COMMENT "Validate ISA file"
    COMMAND pajv -s ${ISA_SCHEMA} -d ${ISA_FILE} && ${CMAKE_COMMAND} -E touch ${VALIDATE_MARKER}
    DEPENDS ${ISA_SCHEMA} ${ISA_FILE}
)
add_custom_target(isa_validate DEPENDS ${VALIDATE_MARKER})

# Check ISA_FILE content

set(ASSERT_MARKER "${PROJECT_BINARY_DIR}/assert.ok")
set(ISA_GEN_RB "${PROJECT_SOURCE_DIR}/gen.rb")
add_custom_command(OUTPUT ${ASSERT_MARKER}
    COMMENT "Check ISA content"
    COMMAND ${ISA_GEN_RB} -d ${ISA_FILE} -r ${ISAPI} -a ${ISA_ASSERTS}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSERT_MARKER}
    DEPENDS ${ISA_FILE} ${ISA_GEN_RB} ${ISA_ASSERTS} ${ISAPI}
)
add_custom_target(isa_assert ALL DEPENDS ${ASSERT_MARKER})

# Template generation

set(TEMPLATES
    instructions.txt.erb
    isa.md.erb
)

panda_isa_gen(TEMPLATES ${TEMPLATES})
