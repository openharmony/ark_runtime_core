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

require_relative "runner"
require_relative "result"
require_relative "reporters/test_reporter"

module TestRunner
  class SingleTestRunner
    def initialize(file, id, reporter_class, root_dir, report_dir)
      @pa_file = file
      @bin_file = "#{$tmp_file}#{id}"
      @id = id
      @reporter = reporter_class.new root_dir, @pa_file, report_dir
      @result = TestRunner::Result.new @reporter
      lines = File.foreach(@pa_file)
                  .grep(/^\s*##\s*runner-option\s*:\s*[\S*\s*]*\s*$/)
                  .flat_map { |s| s.split(':', 2).map(&:strip) }
                  .reject { |s| s.match(/^#/) }
                  .uniq
      @runner_options = lines.each_with_object({}) { |k, h|
        if (k.include? 'tags:')
          tags = TestRunner.split_separated_by_colon k
          h['tags'] = tags
        elsif (k.include? 'bugid:')
          bugs = TestRunner.split_separated_by_colon k
          h['bug_ids'] = bugs
          @reporter.log_bugids(bugs)
        else
          h[k] = true
        end
      }
      @java_options = if @runner_options.include? 'use-java'
        "--boot-class-spaces \"core:java\" --runtime-type \"java\" " \
        "--boot-panda-files " \
        "#{$path_to_panda}/pandastdlib/arkstdlib.abc:" \
        "#{$path_to_panda}/class2panda/java_libbase/java_libbase.abc"
      else
          ''
      end

      @verifier_options = '--verification-enabled=true --verification-options=only-verify '
      @verifier_options << '--log-components=verifier --log-level=debug ' if $verbose_verifier

      @verifier_config = if @runner_options.key?('verifier-debug-config') && !$verifier_debug_config.empty?
        "--verification-debug-config-file=#{$verifier_debug_config} "
      else
        ''
      end

      if @runner_options.include? 'main-exitcode-wrapper'
        @main_function = '_GLOBAL::main_exitcode_wrapper'
        # Exit code value for wrapped main
        # this value is used to determine false-positive cases
        @expected_exit_code = 80
      else
        @main_function = '_GLOBAL::main'
        # Default exit code value for main function
        @expected_exit_code = 0
      end

      @test_panda_options = ''
      previous_line_empty = false # exit loop on two consecutive empty lines
      File.foreach(@pa_file) do |line|
        if match = line.match(/^## panda-options: (.+)\s*$/)
          @test_panda_options = match.captures[0]
          break  # the line we are looking for is found
        elsif line.strip.empty?
          break if previous_line_empty
          previous_line_empty = true
        else
          previous_line_empty = false
        end
      end
    end

    def test_is_error(status)
      return [
        ERROR_NODATA,
        ERROR_CANNOT_CREATE_PROCESS,
        ERROR_TIMEOUT
      ].include? status
    end

    def test_is_in_exclude_list
      return !(@runner_options['tags'] & $exclude_list || []).empty?
    end

    def test_is_in_include_list
      return !(@runner_options['tags'] & $include_list || []).empty?
    end

    def test_is_in_bugid_list
      return !(@runner_options['bug_ids'] & $bug_ids || []).empty?
    end

    def test_is_verifier_only
      return (@runner_options.include? 'verifier-failure' or
        @runner_options.include? 'verifier-only')
    end

    def test_is_verifier_forced
      return ($force_verifier and
        !(@runner_options.include? 'verifier-failure' or
          @runner_options.include? 'verifier-only' or
          @runner_options.include? 'compile-failure'))
    end

    def cleanup
      @reporter.verbose_log "# Cleanup - remove #{@bin_file}, if exists"
      FileUtils.rm(@bin_file) if File.exist? @bin_file
      if $paoc
        FileUtils.rm("#{@bin_file}.aot") if File.exist? "#{@bin_file}.aot"
      end
    end

    def run_pandasm
      return TestRunner::CommandRunner.new(
        "#{$pandasm} #{@pa_file} #{@bin_file}",
        @reporter).run_cmd
    end

    def run_verifier
      return TestRunner::CommandRunner.new("#{$verifier} #{@verifier_options} #{@verifier_config} " \
        "#{@java_options} #{$panda_options.join ' '} #{@test_panda_options} #{@bin_file} _GLOBAL::main",
        @reporter).run_cmd
    end

    def run_panda
      aot = if $paoc
        "--aot-file=#{@bin_file}.aot"
      else
        ""
      end
      return TestRunner::CommandRunner.new("#{$panda} #{@java_options} " \
        "#{$panda_options.join ' '} #{@test_panda_options} " \
        "#{aot} #{@bin_file} #{@main_function}", @reporter).run_cmd
    end

    def run_paoc
      return TestRunner::CommandRunner.new("#{$paoc} #{@java_options} " \
        "--paoc-panda-files #{@bin_file} " \
        "--paoc-output #{@bin_file}.aot", @reporter).run_cmd
    end

    def process_single
      @reporter.prologue
      process_single_inner
      @reporter.epilogue
    end

    def process_single_inner

      @reporter.verbose_log '# List of runner options'
      @reporter.verbose_log "verifier-failure      = #{@runner_options["verifier-failure"]}"
      @reporter.verbose_log "verifier-only         = #{@runner_options["verifier-only"]}"
      @reporter.verbose_log "compiler-failure      = #{@runner_options["compile-failure"]}"
      @reporter.verbose_log "compiler-only         = #{@runner_options["compile-only"]}"
      @reporter.verbose_log "failure               = #{@runner_options["run-failure"]}"
      @reporter.verbose_log "use-java              = #{@runner_options["use-java"]}"
      @reporter.verbose_log "bugid                 = #{@runner_options["bug_ids"]}"
      @reporter.verbose_log "tags                  = #{@runner_options["tags"]}"
      @reporter.verbose_log "ignore                = #{@runner_options["ignore"]}"
      @reporter.verbose_log "verifier-debug-config = #{@runner_options["verifier-debug-config"]}"
      @reporter.verbose_log "main-exitcode-wrapper = #{@runner_options['main-exitcode-wrapper']}"

      # 1) Check step
      if test_is_in_exclude_list
        @reporter.log_exclusion
        @result.update_excluded @pa_file
        return
      end

      if $include_list != [] && !test_is_in_include_list
        @reporter.log_skip_include
        return
      end

      # Check for bugid
      if $bug_ids != [] && !test_is_in_bugid_list
        @reporter.log_skip_bugid
        return
      end

      run_all_and_ignored = $run_all | $run_ignore

      if @runner_options['ignore'] & !run_all_and_ignored
        @reporter.log_skip_ignore
        return
      end

      if !@runner_options['ignore'] & $run_ignore & !$run_all
        @reporter.log_skip_only_ignore
        return
      end

      if @runner_options['ignore'] & ($run_all | $run_ignore)
        @reporter.log_ignore_ignored
      end

      # 2) Compilation step
      @reporter.verbose_log ''
      cleanup

      output, status, core = run_pandasm

      # Failed positive compilation
      if status != 0 && !@runner_options['compile-failure']
        @result.update_failed_compilation output, @pa_file
        return
      end

      # Passed negative compilation
      if status == 0 && @runner_options['compile-failure']
        @result.update_negative_passed_compilation output, @pa_file
        return
      end

      # Failed negative compilation - test passed
      if status != 0  && @runner_options['compile-failure']
        @result.update_failed_negative_compilation output, @pa_file
        return
      end

      if status == 0 && @runner_options['compile-only']
        @result.update_compilation_passed output, @pa_file
        return
      end

      # 3) Verification step
      if test_is_verifier_only
        output, status, core = run_verifier
        if test_is_error(status)
          @result.update_verifier_failure output, status, @pa_file, core
        elsif status == 0 && @runner_options['verifier-failure']
          @result.update_verifier_negative_failure output, status, @pa_file
        elsif status != 0 && !@runner_options['verifier-failure']
          @result.update_verifier_failure output, status, @pa_file, core
        else
          @result.update_passed output, status, @pa_file
        end
        return
      elsif test_is_verifier_forced
        output, status, core = run_verifier
        if test_is_error(status)
          @result.update_verifier_failure output, status, @pa_file, core
        elsif status != 0 && $force_verifier
          @result.update_verifier_failure output, status, @pa_file, core
        else
          @result.update_passed output, status, @pa_file
        end
        return
      end

      # 4) AOT compilation step

      if $paoc
        if @runner_options['run-failure']
          @reporter.log_exclusion
          @result.update_excluded @pa_file
          return
        end
        output, status, core = run_paoc
        if status != 0
          @result.update_failed_compilation output, @pa_file
          return
        end
      end

      # 5) Execution step
      output, status, core = run_panda

      if test_is_error(status)
        @result.update_run_failure output, status, @pa_file, core
      elsif status == @expected_exit_code && @runner_options['run-failure']
        @result.update_run_negative_failure output, status, @pa_file
      elsif status != @expected_exit_code && !@runner_options['run-failure']
        @result.update_run_failure output, status, @pa_file, core
      else
        @result.update_passed output, status, @pa_file
      end
    end
  end
end
