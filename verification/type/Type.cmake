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

set(TYPE_SOURCES
    ${VERIFICATION_SOURCES_DIR}/type/type_type.cpp
    ${VERIFICATION_SOURCES_DIR}/type/type_param.cpp
    ${VERIFICATION_SOURCES_DIR}/type/type_params.cpp
    ${VERIFICATION_SOURCES_DIR}/type/type_set.cpp
    ${VERIFICATION_SOURCES_DIR}/type/type_parametric.cpp
    ${VERIFICATION_SOURCES_DIR}/type/type_systems.cpp
)

set(TYPE_TESTS_SOURCES
    ${VERIFICATION_SOURCES_DIR}/type/tests/type_system_test.cpp
)

