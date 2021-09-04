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

require "pathname"
require_relative "string_logger"
require_relative "base_test_reporter"

module TestRunner

  # JtrTestReporter is reporter to separate file of each test.
  class JtrTestReporter < BaseTestReporter

    def initialize(root_dir, pa_file, report_dir)
      @root = Pathname.new(root_dir)
      file = Pathname.new(pa_file) # path to file
      actual_file = file.relative_path_from(@root)
      report_dir = Pathname.new(report_dir)
      actual_file = @root.basename if @root.file?
      @pa_file = actual_file.to_s
      report_file = report_dir + Pathname.new("#{@pa_file.gsub(/.pa$/, '.jtr')}")
      report_file.dirname.mkpath
      FileUtils.rm report_file if report_file.exist?
      @logger = Reporters::SeparateFileLogger.new(report_file)
      @status = 'Failed.'
      @output = ''
    end

    def jtr_time
      # on farm timezone %Z got 'Europe' which is rejected by uploader
      # so MSK set as a workaround
      Time.now.strftime('%a %b %d %H:%M:%S MSK %Y')
    end

    def prologue()
      @logger.log 1, '#Test Results (version 2)'
      @logger.log 1, '#-----testdescription-----'
      @logger.log 1, "$file=#@root/CTS/conformance/tests/#@pa_file"
      @logger.log 1, "$root=#@root"
      @logger.log 1, ''
      @logger.log 1, '#-----testresult-----'
      @logger.log 1, "test=CTS/conformance/tests/#@pa_file"
      @logger.log 1, 'suitename=CTS'
      @logger.log 1, "start=#{jtr_time}"
    end

    def epilogue()
      @logger.log 1, "end=#{jtr_time}"
      @logger.log 1, "execStatus=#@status"
      # add output section in case of failure
      if !(@status =~ /Passed\.|Not run\./) and !(@output.empty?)
        @logger.log 1, "sections=output"
        @logger.log 1, ''
        @logger.log 1, '#section:output'
        lines = @output.split("\n").map(&:strip).reject { |l| l.match(/^$/) }
        @output = lines.join("\n")
        @logger.log 1, "----------messages:(#{lines.length}/#{@output.length + 1})----------"
        @logger.log 1, @output
        @logger.log 1, "result: Failed."
      end
      @logger.close
    end

    def log_exclusion
      @status = "Not run. Excluded by tag"
    end

    def log_skip_include
      @status = "Not run. Not included by tag"
    end

    def log_skip_bugid
      @status = "Not run. Not mtched bugid"
    end

    def log_skip_ignore
      @status = "Not run. runner_option is ignore"
    end

    def log_skip_only_ignore
      @status = "Not run. Running only ignored"
    end

    def log_ignore_ignored
      @output << "\nRun all requested"
    end

    def log_start_command(cmd)
      @output << "\ncommand = #{cmd}"
    end

    def log_failed_compilation(output)
      @status = "Failed. Failed to compile"
      @output << "\n" << output
    end


    def log_negative_passed_compilation(output)
      @status = "Failed. Test is compiled, but should be rejected."
      @output << "\n" << output
    end

    def log_failed_negative_compilation(output)
      @status = "Passed. Test failed to compile, as expected."
      @output << "\n" << output
    end

    def log_compilation_passed(output)
      @status = "Passed. Compilation-only test."
      @output << "\n" << output
    end

    def log_run_negative_failure(output, status)
      @status = "Failed. Exit code: #{status}, but expected failure."
      @output << "\n" << output
    end

    def log_verifier_negative_failure(output, status)
      @status = "Failed. Verifier exit code: #{status}, but expected failure."
      @output << "\n" << output
    end

    def log_run_failure(output, status, core)
      @status = "Failed. Exit code: #{status}."
      @output << "\nCore dump was created." if core
      @output << "\n" << output
    end

    def log_verifier_failure(output, status, core)
      @status = "Failed. Verifier exit code: #{status}."
      @output << "\nCore dump was created." if core
      @output << "\n" << output
    end

    def log_passed(output, status)
      @status = "Passed. Exit code: #{status}."
      @output << "\n" << output
    end

    def log_excluded
      @status = "Not run."
    end

    def verbose_log(status)
      @output << "\n" << status
    end

    def log_bugids(bugids)
    end

  end
end
