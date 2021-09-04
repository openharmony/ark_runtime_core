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

set(DEBUG_SOURCES
    ${VERIFICATION_SOURCES_DIR}/debug/context/context.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/allowlist/allowlist.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/breakpoint/breakpoint.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/config/config_process.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/config/config_parse.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_breakpoints.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_allowlist.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_options.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_method_options.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_method_groups.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/config_load.cpp
    ${VERIFICATION_SOURCES_DIR}/debug/default_config.cpp
)

set_source_files_properties(${VERIFICATION_SOURCES_DIR}/debug/config/config_parse.cpp
    PROPERTIES COMPILE_FLAGS -fno-threadsafe-statics)

set_source_files_properties(${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_breakpoints.cpp
    PROPERTIES COMPILE_FLAGS -fno-threadsafe-statics)

set_source_files_properties(${VERIFICATION_SOURCES_DIR}/debug/handlers/config_handler_allowlist.cpp
    PROPERTIES COMPILE_FLAGS -fno-threadsafe-statics)
