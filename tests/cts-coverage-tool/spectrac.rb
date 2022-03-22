#!/usr/bin/env ruby
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

# frozen_string_literal: true

require 'optparse'
require 'yaml'
require 'ostruct'
require_relative 'spec'
require_relative 'templates/full_md'
require_relative 'templates/orphaned_md'
require_relative 'templates/uncovered_md'
require_relative 'templates/report_md'

def check_option(opts, key)
  return unless opts[key].empty?
  puts "Missing option: --#{key}"
  puts opts
  exit false
end

options = Hash.new { |h, k| h[k] = [] }

OptionParser.new do |opts|
  opts.banner = 'Usage: spectrac.rb [options]'
  opts.on('-r', '--report FILE', 'Output the test coverage summary report in yaml')
  opts.on('-d', '--testdir DIR', 'Directory with the test files (required)')
  opts.on('-g', '--testglob GLOB', 'Glob(s) for finding test files in testdir (required, could be multiple)') do |g|
    options[:testglob] << g
  end
  opts.on('-s', '--spec FILE', 'ISA spec file (required)')
  opts.on('-n', '--non_testable FILE', 'Non testable assertions')
  opts.on('-u', '--uncovered FILE', 'Output yaml document with ISA spec areas not covered by tests')
  opts.on('-U', '--uncovered_md FILE', 'Output markdown document with ISA spec areas not covered by tests')
  opts.on('-o', '--orphaned FILE', 'Output yaml file with the list of tests not relevant to the spec')
  opts.on('-O', '--orphaned_md FILE', 'Output markdown file with the list of tests not relevant to the spec')
  opts.on('-f', '--full FILE', 'Output spec file with additional coverage-specific fields in yaml')
  opts.on('-F', '--full_md FILE', 'Output spec file with additional coverage-specific fields in markdown')
  opts.on('-h', '--help', 'Prints this help') do
    puts opts
    exit
  end
end.parse!(into: options)

check_option(options, :testglob)
check_option(options, :testdir)
check_option(options, :spec)

unless File.directory? options[:testdir]
  puts "Testdir dir #{options[:testdir]} does not exist!"
  exit false
end

isa = YAML.load_file(options[:spec])
non_testable = options[:non_testable].empty? ? {} : YAML.load_file(options[:non_testable])

spec = Spec.new(isa, non_testable, options[:testdir])
options[:testglob].each { |g|
  path = File.join(options[:testdir], g)
  testfiles = Dir.glob(path)
  unless 0 < testfiles.length
    puts "No files found by #{path}"
    exit false
  end
  testfiles.each { |tf| spec.add_testfile(tf) }
}

report = spec.compute_coverage
ReportMd.new(report).generate

File.write(options[:report], report.to_yaml) unless options[:report].empty?
File.write(options[:orphaned], spec.orphaned.to_yaml) unless options[:orphaned].empty?
File.write(options[:full], spec.spec.to_yaml) unless options[:full].empty?
File.write(options[:uncovered], spec.uncovered.to_yaml) unless options[:uncovered].empty?

FullMd.new(spec.spec).generate(options[:full_md]) unless options[:full_md].empty?
OrphanedMd.new(spec.orphaned).generate(options[:orphaned_md]) unless options[:orphaned_md].empty?
UncoveredMd.new(spec.uncovered).generate(options[:uncovered_md]) unless options[:uncovered_md].empty?
