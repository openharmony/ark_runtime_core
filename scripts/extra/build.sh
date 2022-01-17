#!/bin/bash
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

set -ex

if [[ "${BUILD_TOOL}" = "ninja" ]]; then
    GENERATOR="Ninja"
    BUILD_STR="ninja -k1"
else
    GENERATOR="Unix Makefiles"
    BUILD_STR="make"
fi

ccache --zero-stats || true
ccache --show-stats || true

mkdir -p $ARTIFACTS_DIR/out && cd $ARTIFACTS_DIR/out

cmake $ROOT_DIR \
      -G"${GENERATOR}" \
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
      -DPANDA_ENABLE_CLANG_TIDY=false \
      ${CMAKE_OPTIONS};

${BUILD_STR} -j${NPROC_PER_JOB} ${BUILD_TARGETS}

ccache --show-stats || true
