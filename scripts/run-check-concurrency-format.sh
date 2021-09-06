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

set -e

FILE=$1
PATTERNS=(
    '<mutex>' 'pthread' '<shared_mutex>' '<condition_variable>' '<future>' '<stop_token>' 'this_thread'
    'std::mutex' 'recursive_mutex' 'lock_guard'
)

if [[ "$FILE" == *"libpandabase/os"* ]]; then
    # Do not check files with primitives wrappers.
    exit 0
fi

if [[ "$FILE" == *"runtime/tests"* ]]; then
    # Usage of this_thread::sleep
    exit 0
fi

if [[ "$FILE" == *"runtime/profilesaver"* ]]; then
    # Usage of this_thread::sleep
    exit 0
fi

for pattern in "${PATTERNS[@]}"; do
    if grep ${pattern} ${FILE}; then
        echo "File ${FILE} contains '${pattern}' usage. Please use wrapper functions from 'os/mutex.h' instead."
        exit 1
    fi
done
