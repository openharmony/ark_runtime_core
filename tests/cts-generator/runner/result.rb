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

module TestRunner

  class Result
    # Class-level mutex and test execution statistics
    @@mutex = Mutex.new
    @@stats = { passed: { cnt: 0, files: [] },
                     failed: { cnt: 0, files: [] },
                     core: { cnt: 0, files: [] },
                     compilation_error: { cnt: 0, files: [] },
                     excluded: { cnt: 0, files: [] }}
    def initialize(reporter)
      @reporter = reporter
    end

    def update_failed_compilation(output, file)
      @@mutex.synchronize do
        @@stats[:compilation_error][:cnt] += 1
        @@stats[:compilation_error][:files] << file
      end
      @reporter.log_failed_compilation output
    end

    def update_negative_passed_compilation(output, file)
      @@mutex.synchronize do
        @@stats[:compilation_error][:cnt] += 1
        @@stats[:compilation_error][:files] << file
      end
      @reporter.log_negative_passed_compilation output
    end

    def update_failed_negative_compilation(output, file)
      @@mutex.synchronize do
        @@stats[:passed][:cnt] += 1
        @@stats[:passed][:files] << file
      end
      @reporter.log_failed_negative_compilation output
    end

    def update_compilation_passed(output, file)
      # Compilation passed
      @@mutex.synchronize do
        @@stats[:passed][:cnt] += 1
        @@stats[:passed][:files] << file
      end
      @reporter.log_compilation_passed output
    end

    def update_run_negative_failure(output, status, file)
      @@mutex.synchronize do
        @@stats[:failed][:cnt] += 1
        @@stats[:failed][:files] << file
      end
      @reporter.log_run_negative_failure output, status
    end

    def update_verifier_negative_failure(output, status, file)
      @@mutex.synchronize do
        @@stats[:failed][:cnt] += 1
        @@stats[:failed][:files] << file
      end
      @reporter.log_verifier_negative_failure output, status
    end

    def update_run_failure(output, status, file, core)
      @@mutex.synchronize do
        @@stats[:failed][:cnt] += 1
        @@stats[:failed][:files] << file
        @@stats[:core][:cnt] += 1 if core
        @@stats[:core][:files] << file if core
      end
      @reporter.log_run_failure output, status, core
    end

    def update_verifier_failure(output, status, file, core)
      @@mutex.synchronize do
        @@stats[:failed][:cnt] += 1
        @@stats[:failed][:files] << file
        @@stats[:core][:cnt] += 1 if core
        @@stats[:core][:files] << file if core
      end
      @reporter.log_verifier_failure output, status, core
    end

    def update_passed(output, status, file)
      @@mutex.synchronize do
        @@stats[:passed][:cnt] += 1
        @@stats[:passed][:files] << file
      end
      @reporter.log_passed output, status
    end

    def update_excluded(file)
      @@mutex.synchronize do
        @@stats[:excluded][:cnt] += 1
        @@stats[:excluded][:files] << file
      end
      @reporter.log_excluded
    end

    def self.write_report
      TestRunner::log 3, @@stats.to_yaml
      TestRunner::log 2, "Passed: #{@@stats[:passed][:files].sort.to_yaml}"
      TestRunner::log 1, 'Failed:'
      TestRunner::log 1, "#{@@stats[:failed][:files].sort.to_yaml}"
      TestRunner::log 1, 'Failed (coredump):'
      TestRunner::log 1, "#{@@stats[:core][:files].sort.to_yaml}"
      TestRunner::log 1, 'Compilation error:'
      TestRunner::log 1, "#{@@stats[:compilation_error][:files].sort.to_yaml}"
      TestRunner::log 2, '----------------------------------------'
      TestRunner::log 1, "Run all: #{$run_all}"
      TestRunner::log 1, "Run only ignored: #{$run_ignore}"
      TestRunner::log 1, "Passed: #{@@stats[:passed][:cnt]}"
      TestRunner::log 1, "Failed: #{@@stats[:failed][:cnt]}"
      TestRunner::log 1, "Compilation error: #{@@stats[:compilation_error][:cnt]}"
      TestRunner::log 1, "Excluded: #{@@stats[:excluded][:cnt]}"
    end

    def self.failed?
      (@@stats[:failed][:cnt] + @@stats[:compilation_error][:cnt]) > 0
    end #def
  end #class
end #module
