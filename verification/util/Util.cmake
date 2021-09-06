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

set(UTIL_SOURCES
)

set(UTIL_TESTS_SOURCES
    ${VERIFICATION_SOURCES_DIR}/util/tests/environment.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/equiv_classes_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/lazy_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/relation_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/addr_map_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/tagged_index.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/flags.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/obj_pool_test.cpp
)

set(UTIL_RAPIDCHECK_TESTS_SOURCES
    ${VERIFICATION_SOURCES_DIR}/util/tests/environment.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/bit_vector_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/set_operations_test.cpp
    ${VERIFICATION_SOURCES_DIR}/util/tests/int_set_test.cpp
)
