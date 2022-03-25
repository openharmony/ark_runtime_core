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

project(dprof_daemon)

panda_add_executable(dprof_daemon main.cpp)
target_link_libraries(dprof_daemon dprof dprofstorage)

# Add checkers and sanitizers
panda_add_to_clang_tidy(TARGET dprof_daemon)
panda_add_sanitizers(TARGET dprof_daemon SANITIZERS ${PANDA_SANITIZERS_LIST})
add_check_style(".")

# Generate "generated/daemon_options.h" file
panda_gen_options(TARGET dprof_daemon YAML_FILE options.yaml GENERATED_HEADER daemon_options.h)
