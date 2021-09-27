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

TOP=$(pwd)
CMD=${TOP}/panda/isa/gen.rb
ISA_DATA=${TOP}/panda/isa/isa.yaml
ISA_REQUIRE=${TOP}/panda/isa/isapi.rb

# generated file with multiple template files
# getopts:
# -O: output directory
# -D: date file
# -R: require files
# -I: use ISA_DATA, ISA_REQUIRE as default

if [ $? != 0  ] ; then echo "Terminating..." >&2 ; exit 1 ; fi

while getopts "O:D:R:I" arg
do
    case "$arg" in
        O)
            OUTPUT=${OPTARG}
            ;;
        D)
            DATA=${OPTARG}
            ;;
        R)
            REQUIRE=${OPTARG}
            ;;
        I)
            HAS_ISA=true
            ;;
        *)
            echo "unknown argument"
            exit 1
            ;;
    esac
done
shift $(($OPTIND - 1))

if [ "${HAS_ISA}" ];then
    DATA=${ISA_DATA}
    REQUIRE=${ISA_REQUIRE},${REQUIRE}
fi

for TEMPLATE_ARG in "$@"
do
    TARGET_FILE=$(basename $TEMPLATE_ARG .erb)
    ${CMD} --template ${TEMPLATE_ARG} --data ${DATA} --output ${OUTPUT}/${TARGET_FILE} --require ${REQUIRE}
done
