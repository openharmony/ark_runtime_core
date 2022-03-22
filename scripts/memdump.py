#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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

"""
A tool to view memory usage reports.

To get a memory usage report build Panda with -DPANDA_TRACK_INTERNAL_ALLOCATIONS=2 cmake option.
Panda runtime writes memory reports into memdump.bin file at runtime destruction and at SIGUSR2
signal. This script is aimed to analyse memdump.bin file.
To view the report run the script as follow:
    python3 scripts/memdump.py memdump.bin
The report contains:
    * total allocated memory (not considering free)
    * peak allocated memory (maximum amount of allocated memory)
    * detailed information about each allocation point:
        * total number of bytes allocated (not considering free)
        * number of allocations
        * range of allocation sizes
        * stacktrace

To view only live allocations (allocations which are not free at the moment of dump) --live
option is used. If the dump is collected during runtime destruction this report will contains
memory leaks

It is possible to filter and sort data (run the script with -h option)
"""

import sys
import argparse
import struct

# must be aligned with SpaceType enum in
# libpandabase/mem/space.h
SPACES = {
    1: 'object',
    2: 'humongous',
    3: 'nonmovable',
    4: 'internal',
    5: 'code',
    6: 'compiler'
}

TAG_ALLOC = 1
TAG_FREE = 2


class AllocInfo:
    """Contains information about all allocated memory"""

    def __init__(self, stacktrace):
        self.stacktrace = stacktrace
        self.allocated_size = 0
        self.sizes = []

    def alloc(self, size):
        """Handles allocation of size bytes"""

        self.allocated_size += size
        self.sizes.append(size)

    def free(self, size):
        """Handles deallocation of size bytes"""

        self.allocated_size -= size


# pylint: disable=too-few-public-methods
class Filter:
    """Filter by space and substring"""

    def __init__(self, space, strfilter):
        self.space = space
        self.strfilter = strfilter

    def filter(self, space, stacktrace):
        """Checks that space and stacktrace matches filter"""

        if self.space != 'all' and SPACES[space] != self.space:
            return True
        if self.strfilter is not None and self.strfilter not in stacktrace:
            return True
        return False


def validate_space(value):
    """Validates space value"""

    if value not in SPACES.values():
        print('Invalid value {} of --space option'.format(value))
        sys.exit(1)


def read_string(file):
    """Reads string from file"""

    num = struct.unpack('I', file.read(4))[0]
    return file.read(num).decode('utf-8')


def sort(data):
    """Sorts data by allocated size and number of allocations"""

    return sorted(
        data, key=lambda info: (info.allocated_size, len(info.sizes)),
        reverse=True)


def pretty_alloc_sizes(sizes):
    """Prettifies allocatation sizes"""

    min_size = sizes[0]
    max_size = min_size
    for size in sizes:
        if size < min_size:
            min_size = size
        elif size > max_size:
            max_size = size

    if min_size == max_size:
        return 'all {} bytes'.format(min_size)

    return 'from {} to {} bytes'.format(min_size, max_size)


def pretty_stacktrace(stacktrace):
    """Prettifies stacktrace"""

    if not stacktrace:
        return "<No stacktrace>"
    return stacktrace


def get_args():
    """Gets cli arguments"""

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--live', action='store_true', default=False,
        help='Dump only live allocations (for which "free" is not called)')
    parser.add_argument(
        '--space',
        help='Report only allocations for the specific space. Possible values: {}'.
        format(', '.join(SPACES.values())))
    parser.add_argument(
        '--filter',
        help='Filter allocations by a string in stacktrace (it may be a function of file and line)')
    parser.add_argument('input_file', default='memdump.bin')

    args = parser.parse_args()

    return args


# pylint: disable=too-many-locals
def get_allocs(args, space_filter):
    """Prints and returns statistic: stacktrace -> allocation info"""

    total_allocated = 0
    max_allocated = 0
    cur_allocated = 0
    with open(args.input_file, "rb") as file:
        num_items, num_stacktraces = struct.unpack('II', file.read(8))
        stacktraces = {}
        stacktrace_id = 0
        while stacktrace_id < num_stacktraces:
            stacktraces[stacktrace_id] = read_string(file)
            stacktrace_id += 1

        allocs = {}
        id2alloc = {}
        while num_items > 0:
            tag = struct.unpack_from("I", file.read(4))[0]
            if tag == TAG_ALLOC:
                identifier, size, space, stacktrace_id = struct.unpack(
                    "IIII", file.read(16))
                stacktrace = stacktraces[stacktrace_id]
                if not space_filter.filter(space, stacktrace):
                    info = allocs.setdefault(
                        stacktrace_id, AllocInfo(stacktrace))
                    info.alloc(size)
                    id2alloc[identifier] = (info, size)
                    total_allocated += size
                    cur_allocated += size
                    max_allocated = max(cur_allocated, max_allocated)
            elif tag == TAG_FREE:
                alloc_id = struct.unpack("I", file.read(4))[0]
                res = id2alloc.pop(alloc_id, None)
                if res is not None:
                    info = res[0]
                    size = res[1]
                    info.free(size)
                    cur_allocated -= size
            else:
                raise Exception("Invalid file format")

            num_items -= 1

    print("Total allocated: {}, peak allocated: {}, current allocated {}".format(
        total_allocated, max_allocated, cur_allocated))

    return allocs


def main():
    """Script's entrypoint"""

    args = get_args()
    live_allocs = args.live

    space_filter = 'all'
    if args.space is not None:
        validate_space(args.space)
        space_filter = args.space

    allocs = get_allocs(args, Filter(space_filter, args.filter))
    allocs = allocs.values()
    allocs = sort(allocs)

    for info in allocs:
        if not live_allocs or (live_allocs and info.allocated_size > 0):
            print("Allocated: {} bytes. {} allocs {} from:\n{}".format(
                info.allocated_size, len(info.sizes),
                pretty_alloc_sizes(info.sizes),
                pretty_stacktrace(info.stacktrace)))


if __name__ == "__main__":
    main()
