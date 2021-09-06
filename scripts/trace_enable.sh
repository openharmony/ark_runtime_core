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

TRACKING_PATH=/sys/kernel/debug/tracing

OUTPUT=$1

print_usage() {
    echo "Usage: $0 <trace_output> <tracing_time> <trace_buffer_size_kb>"
}

# Set write permission for all users on trace_marker
setup_permissions() {
    chmod +rx $(dirname $TRACKING_PATH)
    chmod +rx $TRACKING_PATH
    chmod o+w $TRACKING_PATH/trace_marker
}

clear_trace() {
    echo > $TRACKING_PATH/trace
}

category_enable() {
    echo 1 > $TRACKING_PATH/events/sched/sched_switch/enable
    echo 1 > $TRACKING_PATH/events/sched/sched_wakeup/enable
}

category_disable() {
    echo 0 > $TRACKING_PATH/events/sched/sched_switch/enable
    echo 0 > $TRACKING_PATH/events/sched/sched_wakeup/enable
}

start_trace() {
    echo "Warning: If the PandaVM aborted while writing trace, try to enlarge the trace buffer size here."
    echo $BUFF_SIZE > $TRACKING_PATH/buffer_size_kb
    echo 'global' > $TRACKING_PATH/trace_clock
    echo 'nop' > $TRACKING_PATH/current_tracer
    echo 0 > $TRACKING_PATH/options/overwrite

    category_enable
    clear_trace
    echo 1 > $TRACKING_PATH/tracing_on
}

dump_trace() {
    cat $TRACKING_PATH/trace > $OUTPUT
    chmod o+w $OUTPUT
    clear_trace
}

stop_trace() {
    echo 0 > $TRACKING_PATH/tracing_on
    category_disable
    dump_trace
    echo 4 > $TRACKING_PATH/buffer_size_kb
}

sigint_handler() {
    stop_trace
    echo
    echo "Stopped"
    exit 0
}

# main

if [ $(id -u) != 0 ] ; then
    echo 'Run this script as root or use sudo';
    exit 1;
fi;

if [ -z "$OUTPUT" ] || [ ! -z "$4" ]; then
    print_usage
    exit 1
fi;

TIME_TO_WAIT=$2

if [ -z "$2" ]; then
    echo "Warning: default tracing time is not specified, set to 60 seconds"
    TIME_TO_WAIT=60
fi;

BUFF_SIZE=$3

if [ -z "$3" ]; then
    echo "Warning: default trace buffer size is not specified, set to 8 MB"
    BUFF_SIZE=8192
fi;

setup_permissions
trap sigint_handler INT # Tracing will be interrupted on SIGINT

start_trace
echo "Tracing has started. Press ^C to finish"

sleep $TIME_TO_WAIT

stop_trace
echo "Stopped by timeout"
