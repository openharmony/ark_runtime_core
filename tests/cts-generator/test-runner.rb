#!/usr/bin/env ruby
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

require 'optparse'
require 'fileutils'
require 'ostruct'
require 'yaml'
require 'open3'
require 'pathname'
require 'thread'
require 'timeout'
require 'securerandom'

require_relative 'runner/runner'
require_relative 'runner/single_test_runner'
require_relative 'runner/reporters/test_reporter'
require_relative 'runner/reporters/jtr_reporter'

def check_option(optparser, options, key)
  return if options[key]

  puts "Missing option: --#{key}"
  puts optparser
  exit false
end

def check_option_limit(optparser, options, key, min, max)
  return unless options[key]
  return if options[key] >= min && options[key] <= max

  puts "Incorrect value for option: --#{key} [#{min}, #{max}]"
  puts optparser
  exit false
end

def check_option_enum(optparser, options, key, enum)
  return unless options[key]
  return if enum.include?(options[key])
  puts "Incorrect value for option: --#{key} #{enum}"
  puts optparser
  exit false
end

Encoding.default_external = Encoding::UTF_8
Encoding.default_internal = Encoding::UTF_8

options = OpenStruct.new
options[:exclude_tag] = []
options[:include_tag] = []
options[:bug_ids] = []
options[:panda_options] = []

optparser = OptionParser.new do |opts|
  opts.banner = 'Usage: test-runner.rb [options]'
  opts.on('-p', '--panda-build DIR', 'Path to panda build directory (required)')
  opts.on('-t', '--test-dir DIR', 'Path to test directory to search tests recursively, or path to single test (required)')
  opts.on('-v', '--verbose LEVEL', Integer, 'Set verbose level 1..5')
  opts.on('--verbose-verifier', 'Allow verifier to produce extended checking log')
  opts.on('--aot-mode', 'Perform AOT compilation on test sources')
  opts.on('--timeout SECONDS', Integer, 'Set process timeout, default is 30 seconds')
  opts.on('--dump-timeout SECONDS', Integer, 'Set process completion timeout, default is 30 seconds')
  opts.on('--enable-core-dump', 'Enable core dumps')
  opts.on('--verify-tests', 'Run verifier against positive tests (option for test checking)')
  opts.on('--global-timeout SECONDS', Integer, 'Set testing timeout, default is 0 (ulimited)')
  opts.on('-a', '--run-all', 'Run all tests, ignore "runner-option: ignore" tag in test definition')
  opts.on('--run-ignored', 'Run ignored tests, which have "runner-option: ignore" tag in test definition')
  opts.on('--reporter TYPE', "Reporter for test results (default 'log', available: 'log', 'jtr', 'allure')")
  opts.on('--report-dir DIR', "Where to put results, applicable for 'jtr' and 'allure' logger")
  opts.on('--verifier-debug-config PATH', "Path to verifier debug config file")
  opts.on('-e', '--exclude-tag TAG', Array, 'Exclude tags for tests') do |f|
    options[:exclude_tag] |= [*f]
  end
  opts.on('-o', '--panda-options OPTION', Array, 'Panda options') do |f|
    options[:panda_options] |= [*f]
  end
  opts.on('-i', '--include-tag TAG', Array, 'Include tags for tests') do |f|
    options[:include_tag] |= [*f]
  end
  opts.on('-b', '--bug_id BUGID', Array, 'Include tests with specified bug ids') do |f|
    options[:bug_ids] |= [*f]
  end
  opts.on('-j', '--jobs N', 'Amount of concurrent jobs for test execution (default 8)', Integer)
  opts.on('--prlimit OPTS', "Run panda via prlimit with options")
  # Device-specific options:
  opts.on('-H', '--host-toolspath PATH', 'directory with host-tools')
  opts.on('-h', '--help', 'Prints this help') do
    puts opts
    exit
  end
end


optparser.parse!(into: options)

check_option_enum(optparser, options, 'reporter', ['log', 'jtr', 'allure'])

check_option(optparser, options, 'panda-build')
check_option(optparser, options, 'test-dir')
check_option_limit(optparser, options, 'jobs', 1, 20)
check_option_limit(optparser, options, 'timeout', 1, 1000)
options['verbose'] = 1 unless options['verbose']
options['timeout'] = 30 unless options['timeout']
options['dump-timeout'] = 30 unless options['dump-timeout']
options['global-timeout'] = 0 unless options['global-timeout']
options['jobs'] = 8 unless options['jobs']
options['reporter'] = 'log'  unless options['reporter']

$VERBOSITY = options['verbose']
$TIMEOUT = options['timeout']
$DUMP_TIMEOUT = options['dump-timeout']
$GLOBAL_TIMEOUT = options['global-timeout']
$CONCURRENCY = options['jobs']

$path_to_panda = options['panda-build']
$pandasm = "#{$path_to_panda}/bin/ark_asm"
$panda = if options['prlimit']
  "prlimit #{options['prlimit']} #{$path_to_panda}/bin/ark"
else
  "#{$path_to_panda}/bin/ark"
end
# Now verifier is integrated to panda!
$verifier = "#{$panda}"
$verifier_debug_config = options['verifier-debug-config'] || ''
$paoc = if options['aot-mode']
  # Use paoc on host for x86
  "#{$path_to_panda}/bin/ark_aot"
else
  false
end

TestRunner.log 2, "Path to panda: #{$path_to_panda}"
TestRunner.log 2, "Path to verifier debug config: #{$verifier_debug_config}"

# Check if tests are going to be executed on Device
if options['host-toolspath']
  $pandasm = "LD_LIBRARY_PATH=#{options['host-toolspath']}/assembler:" \
             "#{options['host-toolspath']}/bytecode_optimizer/:" \
             "#{options['host-toolspath']}/libpandabase/:" \
             "#{options['host-toolspath']}/compiler/:#{options['host-toolspath']}/libpandafile/:" \
             "#{options['host-toolspath']}/libziparchive/:" \
             "#{options['host-toolspath']}/libc_sec/ " \
             "#{options['host-toolspath']}/assembler/ark_asm"
end

$run_all = options['run-all']
$run_ignore = options['run-ignored']
$enable_core = !!options['enable-core-dump']
$force_verifier = !!options['verify-tests']
$verbose_verifier = !!options['verbose-verifier']
$exclude_list = options[:exclude_tag]
$include_list = options[:include_tag]
$bug_ids = options[:bug_ids]
$panda_options = options[:panda_options]
$root_dir = options['test-dir']
$report_dir = options['report-dir'] || ''
$reporter = options['reporter']

# path_to_tests = '/mnt/d/linux/work/panda/tests/cts-generator/cts-generated/'
path_to_tests = $root_dir
TestRunner::log 2, "Path to tests: #{path_to_tests}"

TestRunner::log 2, "pandasm: #{$pandasm}"
TestRunner::log 2, "panda: #{$panda}"
TestRunner::log 2, "verifier: #{$verifier}"

$tmp_dir  = ".#{File::SEPARATOR}.bin#{File::SEPARATOR}#{SecureRandom.uuid}#{File::SEPARATOR}"
$tmp_file = "#{$tmp_dir}pa.bin"
TestRunner::log 2, "tmp_dir: #{$tmp_dir}"
TestRunner::log 2, "tmp_file: #{$tmp_file}"
TestRunner::log 3, "Make dir - #{$tmp_dir}"
FileUtils.mkdir_p $tmp_dir unless File.exist? $tmp_dir

interrupted = false
timeouted = false

TestRunner::log 2, 'Walk through directories'

files = if File.file?(path_to_tests)
  Dir.glob("#{path_to_tests}")
else
  Dir.glob("#{path_to_tests}/**/*.pa")
end

reporter_factory = if $reporter == 'jtr'
    TestRunner::JtrTestReporter
  elsif $reporter == 'allure'
    TestRunner::AllureTestReporter
  else
    TestRunner::LogTestReporter
  end

FileUtils.rm_r $report_dir, force: true if File.exist? $report_dir

start_time = Time.now
queue = Queue.new
files.each { |x| queue.push x}

# TestRunner::Result holds execution statistic

def create_executor_threads(queue, id, reporter_factory)
  Thread.new do
    begin
      while file = queue.pop(true)
        runner = TestRunner::SingleTestRunner.new(
          file, id, reporter_factory, $root_dir, $report_dir)
        runner.process_single
      end
    rescue ThreadError => e # for queue.pop, suppress
    rescue Interrupt => e
      TestRunner.print_exception e
      interrupted = true
    rescue SignalException => e
      TestRunner.print_exception e
      interrupted = true
    rescue Exception => e
      TestRunner.print_exception e
      interrupted = true
    end
  end
end

if $CONCURRENCY > 1
  runner_threads = (1..$CONCURRENCY).map do |id|
    create_executor_threads queue, id, reporter_factory
  end
  begin
    if $GLOBAL_TIMEOUT == 0
      runner_threads.map(&:join)
    else
      has_active_tread = false
      loop do
        # Wait a bit
        sleep 1
        # Check if there are any working thread
        has_active_tread = false
        runner_threads.each do |t|
          has_active_tread = true if t.status != false
        end
        # If we reach timeout or there no active threads, break
        if (Time.now - start_time >= $GLOBAL_TIMEOUT) | !has_active_tread
          break
        end
      end

      # We have active treads, kill them
      if has_active_tread == true
        runner_threads.each do |t|
          status = t.status
          if status != false
            timeouted = true
             TestRunner::log 1, "Kill test executor tread #{t}"
            t.kill
          end
        end
      end
    end
  rescue SignalException => e
    interrupted = true
    TestRunner.print_exception e
  end
else
  begin
    while file = queue.pop(true)
      runner = TestRunner::SingleTestRunner.new(
        file, 1, reporter_factory, $root_dir, $report_dir)
      end
      runner.process_single
      if ($GLOBAL_TIMEOUT > 0 && (Time.now - start_time >= $GLOBAL_TIMEOUT))
        puts "Global timeout reached, finish test execution"
        break
      end
    end
  rescue ThreadError => e # for queue.pop, suppress
  rescue Interrupt => e
    TestRunner.print_exception e
    interrupted = true
  rescue SignalException => e
    TestRunner.print_exception e
    interrupted = true
  rescue Exception => e
    TestRunner.print_exception e
    interrupted = true
  end
end

# Write result report

TestRunner::log 1, '----------------------------------------'
TestRunner::log 1, "Testing done in #{Time.now - start_time} sec"
TestRunner::log 2, "Remove tmp dir:#{$tmp_dir}"
FileUtils.mkdir_p $tmp_dir unless File.exist? $tmp_dir

TestRunner::Result.write_report

TestRunner::log 1, "Testing timeout reached, so testing failed" if timeouted
TestRunner::log 1, "Testing interrupted and failed" if interrupted

exit 1 if interrupted || timeouted || TestRunner::Result.failed?
