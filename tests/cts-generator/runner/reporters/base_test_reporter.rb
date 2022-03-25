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

  # Methods should be redefined in derived class
  class BaseTestReporter
    def prologue()
      raise NotImplementedError, "#{self.class} does not implement prologue()."
    end

    def epilogue()
      raise NotImplementedError, "#{self.class} does not implement epilogue()."
    end

    def log_exclusion
      raise NotImplementedError, "#{self.class} does not implement log_exclusion()."
    end

    def log_skip_include
      raise NotImplementedError, "#{self.class} does not implement log_skip_include()."
    end

    def log_skip_bugid
      raise NotImplementedError, "#{self.class} does not implement log_skip_bugid()."
    end

    def log_skip_ignore
      raise NotImplementedError, "#{self.class} does not implement log_skip_ignore()."
    end

    def log_skip_only_ignore
      raise NotImplementedError, "#{self.class} does not implement log_skip_only_ignore()."
    end

    def log_ignore_ignored
      raise NotImplementedError, "#{self.class} does not implement log_ignore_ignored()."
    end

    def log_start_command(cmd)
     raise NotImplementedError, "#{self.class} does not implement log_start_command()."
    end

    def log_failed_compilation(output)
      raise NotImplementedError, "#{self.class} does not implement log_failed_compilation()."
    end

    def log_negative_passed_compilation(output)
      raise NotImplementedError, "#{self.class} does not implement log_negative_passed_compilation()."
    end

    def log_failed_negative_compilation(output)
      raise NotImplementedError, "#{self.class} does not implement log_failed_negative_compilation()."
    end

    def log_compilation_passed(output)
      raise NotImplementedError, "#{self.class} does not implement log_compilation_passed()."
    end

    def log_run_negative_failure(output, status)
      raise NotImplementedError, "#{self.class} does not implement log_run_negative_failure()."
    end

    def log_verifier_negative_failure(output, status)
      raise NotImplementedError, "#{self.class} does not implement log_verifier_negative_failure()."
    end

    def log_run_failure(output, status, core)
      raise NotImplementedError, "#{self.class} does not implement log_run_failure()."
    end

    def log_verifier_failure(output, status, core)
      raise NotImplementedError, "#{self.class} does not implement  log_verifier_failure()."
    end

    def log_passed(output, status)
      raise NotImplementedError, "#{self.class} does not implement log_passed()."
    end

    def log_excluded
      raise NotImplementedError, "#{self.class} does not implement log_excluded()."
    end

    def verbose_log(status)
      raise NotImplementedError, "#{self.class} does not implement verbose_log()."
    end

    def log_bugids(bugids)
      raise NotImplementedError, "#{self.class} does not implement log_bugids()."
    end

  end

end
