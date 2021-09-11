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

cmake_minimum_required(VERSION 3.10)

project(pandaverification_gen)

set(ABS_INT_EXTRA_HEADERS)
set(ABS_INT_TEMPLATES
    abs_int_inl_compat_checks
)
foreach(TEMPLATE_NAME ${ABS_INT_TEMPLATES})
    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/${TEMPLATE_NAME}.h")
    panda_gen_file(
        DATAFILE "${PANDA_ROOT}/verification/verification.yaml"
        REQUIRES "${PANDA_ROOT}/verification/verification.rb"
        TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/templates/${TEMPLATE_NAME}.h.erb"
        OUTPUTFILE ${OUTPUT}
    )
    list(APPEND ABS_INT_EXTRA_HEADERS ${OUTPUT})
endforeach()

set(ISA_TEMPLATES
    cflow_iterate_inl_gen.h.erb
    abs_int_inl_gen.h.erb
    job_fill_gen.h.erb
)
set(GEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY ${GEN_INCLUDE_DIR})
panda_isa_gen(
    TEMPLATES ${ISA_TEMPLATES}
    SOURCE ${CMAKE_CURRENT_LIST_DIR}/templates
    DESTINATION ${GEN_INCLUDE_DIR}
    EXTRA_DEPENDENCIES ${ABS_INT_EXTRA_HEADERS}
)

panda_gen_messages(
    YAML_FILE ${PANDA_ROOT}/verification/messages.yaml
    GENERATED_HEADER ${GEN_INCLUDE_DIR}/verifier_messages.h
)
