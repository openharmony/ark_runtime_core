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

require_relative "string_logger"
require_relative "base_test_reporter"

module TestRunner

  # LogTestReporter is reporter to console, default reporter
  class LogTestReporter < BaseTestReporter

    @@mutex = Mutex.new

    def initialize(root_dir, pa_file, report_dir)
      @pa_file = pa_file
      @logger = Reporters::StringLogger.new
    end

    def prologue()

      @logger.log 2, ''
      @logger.log 2, '----------------------------------------'
      @logger.log 2, ''
      @logger.log 2, "Test file: #{@pa_file}"
    end

    def epilogue()
      output = @logger.string
      @@mutex.synchronize do
        puts output
      end if !output.empty?
      @logger.close
    end

    def log_exclusion
      @logger.log 2, "Skip excluded test #{@pa_file} by tag"
    end

    def log_skip_include
      @logger.log 2, "Skip not included test #{@pa_file} by tag"
    end

    def log_skip_bugid
      @logger.log 2, "Skip test #{@pa_file} since bug id do not match"
    end

    def log_skip_ignore
      @logger.log 2, "Skip test #{@pa_file}, because 'runner_option: ignore' tag is defined"
    end

    def log_skip_only_ignore
      @logger.log 2, "Skip test #{@pa_file}, because run only test with 'runner_option: ignore' tag"
    end

    def log_ignore_ignored
      @logger.log 2, "Execute test #{@pa_file}, since 'runner_option: ignore' tag is ignored"
    end

    def log_start_command(cmd)
      @logger.log 3, "Start: #{cmd}"
    end

    def log_failed_compilation(output)
      @logger.log 1, "Test failed: #{@pa_file}"
      @logger.log 1, "Failed to compile #{@pa_file}"
      @logger.log 1, output
    end


    def log_negative_passed_compilation(output)
      @logger.log 1, "Test failed: #{@pa_file}"
      @logger.log 1, "Test is compiled, but should be rejected #{@pa_file}"
      @logger.log 1, output
    end

    def log_failed_negative_compilation(output)
      @logger.log 3, output
      @logger.log 2, "Test passed: #{@pa_file}"
      @logger.log 2, "Test failed to compile, as expected. #{@pa_file}"
    end

    def log_compilation_passed(output)
      @logger.log 3, output
      @logger.log 2, "Test passed: #{@pa_file}"
      @logger.log 2, "Compilation-only test. #{@pa_file}"
    end

    def log_run_negative_failure(output, status)
      @logger.log 1, "Negative test failed: #{@pa_file}"
      @logger.log 1, "panda exit code: #{status}, but expected test failure."
      @logger.log 1, output
    end

    def log_verifier_negative_failure(output, status)
      @logger.log 1, "Negative test failed: #{@pa_file}"
      @logger.log 1, "verifier exit code: #{status}, but expected test failure."
      @logger.log 1, output
    end

    def log_run_failure(output, status, core)
      @logger.log 1, "Test failed: #{@pa_file}"
      @logger.log 1, "Panda exit code: #{status}"
      @logger.log 1, "core dump was created" if core
      @logger.log 1, output
    end

    def log_verifier_failure(output, status, core)
      @logger.log 1, "Test failed: #{@pa_file}"
      @logger.log 1, "verifier exit code: #{status}"
      @logger.log 1, "core dump was created" if core
      @logger.log 1, output
    end

    def log_passed(output, status)
      @logger.log 3, output
      @logger.log 2, "Test passed: #{@pa_file}"
      @logger.log 2, "panda exit code: #{status}"
    end

    def log_excluded
      @logger.log 3, "Test excluded: #{@pa_file}"
    end

    def verbose_log(status)
      @logger.log 3, status
    end

    def log_bugids(bugids)
    end

  end
end
