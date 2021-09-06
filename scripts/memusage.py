#!/usr/bin/env python3
# -- coding: utf-8 --
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

"""
A tool to get memory usage reports
"""

import argparse
from operator import attrgetter
from time import sleep

PANDA_REGION_RE = r"""\[panda:([^\]]+)\]"""


def read_file(path):
    """Reads file"""

    with open(path) as file:
        return file.readlines()


# pylint: disable=too-few-public-methods
class MemInfo:
    """Memory region information: name, size, rss, pss"""

    def __init__(self, name):
        self.name = name
        self.size = 0
        self.rss = 0
        self.pss = 0


def is_hex_digit(char):
    """Checks whether char is hexadecimal digit"""

    return '0' <= char <= '9' or 'a' <= char <= 'f'


def is_start_of_map(line):
    """Checks whether line is the start of map"""

    return len(line) > 0 and is_hex_digit(line[0])


def is_stack_region(name):
    """Checks whether memory region is stack"""

    return name == '[stack]'


def is_heap_region(name, remote):
    """Checks whether memory region is heap"""

    if remote:
        return name == '[anon:libc_malloc]'
    return name == '[heap]'


def is_file_path(name):
    """Checks whether name is file path"""

    return name.startswith('/')


def get_name(line):
    """Parses line from /proc/pid/maps and returns name.

    The line has format as follow:
    55e88a5ab000-55e88a7c8000 r-xp 00068000 08:02 14434818                   /usr/bin/nvim
    The last element can contain spaces so we cannot use split here.
    Find it manually
    """
    pos = 0
    for _ in range(5):
        pos = line.index(' ', pos) + 1
    return line[pos:].strip()


# pylint: disable=too-many-instance-attributes
class Mem:
    """Represents panda memory regions"""

    def __init__(self):
        self.stack = MemInfo("Stack")
        self.heap = MemInfo("Heap")
        self.so_files = MemInfo(".so files")
        self.abc_files = MemInfo(".abc files")
        self.an_files = MemInfo(".an files")
        self.other_files = MemInfo("Other files")
        self.other = MemInfo("Other")
        # This list must be synchronized with panda list in
        # libpandabase/mem/space.h
        self.panda_regions = {
            "[anon:Object Space]": MemInfo("Object Space"),
            "[anon:Humongous Space]": MemInfo("Humongous Space"),
            "[anon:Non Movable Space]": MemInfo("Non Movable Space"),
            "[anon:Internal Space]": MemInfo("Internal Space"),
            "[anon:Code Space]": MemInfo("Code Space"),
            "[anon:Compiler Space]": MemInfo("Compiler Space")
        }

    def get_mem_info(self, name, remote):
        """Gets memory region information by name"""

        info = self.other
        if is_stack_region(name):
            info = self.stack
        elif is_heap_region(name, remote):
            info = self.heap
        elif self.panda_regions.get(name) is not None:
            info = self.panda_regions.get(name)
        elif is_file_path(name):
            if name.endswith('.so'):
                info = self.so_files
            elif name.endswith('.abc'):
                info = self.abc_files
            elif name.endswith('.an'):
                info = self.an_files
            else:
                info = self.other_files
        return info

    def gen_report(self, smaps, remote):
        """Parses smaps and returns memory usage report"""

        info = self.other
        for line in smaps:
            if is_start_of_map(line):
                # the line of format
                # 55e88a5ab000-55e88a7c8000 r-xp 00068000 08:02 14434818    /usr/bin/nvim
                name = get_name(line)
                info = self.get_mem_info(name, remote)
            else:
                # the line of format
                # Size:               2164 kB
                elems = line.split()
                if elems[0] == 'Size:':
                    if len(elems) < 3:
                        raise Exception('Invalid file format')
                    info.size += int(elems[1])
                elif elems[0] == 'Rss:':
                    if len(elems) < 3:
                        raise Exception('Invalid file format')
                    info.rss += int(elems[1])
                elif elems[0] == 'Pss:':
                    if len(elems) < 3:
                        raise Exception('Invalid file format')
                    info.pss += int(elems[1])

        memusage = [self.stack, self.heap, self.so_files, self.abc_files,
                    self.an_files, self.other_files, self.other]
        memusage.extend(self.panda_regions.values())

        return memusage


def aggregate(reports):
    """Aggregates memory usage reports"""

    count = len(reports)
    memusage = reports.pop(0)
    while reports:
        for left, right in zip(memusage, reports.pop(0)):
            left.size = left.size + right.size
            left.rss = left.rss + right.rss
            left.pss = left.pss + right.pss

    for entry in memusage:
        entry.size = int(float(entry.size) / float(count))
        entry.rss = int(float(entry.rss) / float(count))
        entry.pss = int(float(entry.pss) / float(count))

    return memusage


def print_report_row(col1, col2, col3, col4):
    """Prints memory usage report row"""

    print("{: >20} {: >10} {: >10} {: >10}".format(col1, col2, col3, col4))


def print_report(report):
    """Prints memory usage report"""

    print('Memory usage')
    print_report_row('Region', 'Size', 'RSS', 'PSS')
    for record in report:
        print_report_row(record.name, record.size, record.rss, record.pss)


def main():
    """Script's entrypoint"""

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-f', '--follow', action='store_true',
        help='Measure memory for a process periodically until it gets died')
    parser.add_argument(
        '-i', '--interval', default=200, type=int,
        help='Interval in ms between following process ping')
    parser.add_argument('pid', type=int)

    args = parser.parse_args()

    reports = []
    smaps = read_file('/proc/{}/smaps'.format(args.pid))
    report = Mem().gen_report(smaps, False)
    reports.append(report)
    cont = args.follow
    while cont:
        sleep(args.interval / 1000)
        try:
            smaps = read_file('/proc/{}/smaps'.format(args.pid))
            report = Mem().gen_report(smaps, False)
            reports.append(report)
        except FileNotFoundError:
            cont = False

    report = aggregate(reports)
    report.sort(key=attrgetter('size'), reverse=True)
    print_report(report)


if __name__ == "__main__":
    main()
