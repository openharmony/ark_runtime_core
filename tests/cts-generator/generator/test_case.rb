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

require 'stringio'

require_relative 'test_base'

module Generator
  class TestCase < TestBase
    def initialize(command, current_case, instruction, definitions, predefined, template, skip_header)
      super instruction, command, definitions, predefined, skip_header
      LOG.debug "TestCase created with '#{command}' '#{current_case}' '#{instruction}'"

      # command - YAML:
      #   sig: Generator::TEST__SIG
      #   check-type: Generator::CASE_CHECK_TYPE
      #   cases: Generator::CASES
      #
      # current_case - YAML:
      #   values: Generator::CASE_VALUES
      #   case-check-type: Generator::CASE_CHECK_TYPE
      #   code-template: Generator::CASE_TEMPLATE

      @command = command
      @current_case = current_case
      @instruction = instruction
      @definitions = definitions
      @predefined = predefined
      @template = template
    end


    def get_header_template()
      headers = super()
      case_headers = @current_case[Generator::CASE_HEADER_TEMPLATE]
      if headers.nil? && case_headers.nil?
        nil
      else
        (case_headers||[]).concat(headers || [])
      end
    end


    def create_single_test_case(bugids, ignore, tags)
      StringIO.open do |content|
        write_test_initial_block content

        test_run_options = @command[Generator::TEST_RUN_OPTIONS]
        test_description = @command[Generator::TEST_DESCRIPTION] || ""
        case_run_options = @current_case[Generator::CASE_RUN_OPTIONS]
        run_options = case_run_options unless case_run_options.nil?
        run_options = test_run_options if run_options.nil? && !test_run_options.nil?
        run_options = [] if run_options.nil?

        ignore_test = @command[Generator::TEST_IGNORE] || false
        bugids_test = @command[Generator::TEST_BUG_ID] || []
        tags_test = @command[Generator::TEST_TAGS] || []
        ignore_case = @current_case.fetch(Generator::CASE_IGNORE, ignore_test || ignore)
        bugids_case = @current_case[Generator::CASE_BUG_ID] || []
        tags_case = @current_case[Generator::CASE_TAGS] || []
        description = @current_case[Generator::CASE_DESCRIPTION] || ""
        test_panda_options = @command[Generator::TEST_PANDA_OPTIONS] || ""

        write_runner_options content, run_options, ignore_case, bugids + bugids_test + bugids_case, tags_test + tags + tags_case,
          test_description + " " + description, test_panda_options

        write_test_main_block content

        template = if @current_case.key?(Generator::CASE_TEMPLATE)
                     LOG.debug 'Case has own template, use it'
                     @current_case[Generator::CASE_TEMPLATE]
                   else
                     LOG.debug 'Use main template for current tests'
                     @template
                   end

        if @current_case.key?(Generator::CASE_VALUES)
          LOG.debug "substitute values: #{@current_case[Generator::CASE_VALUES]}"

          begin
            values = @current_case[Generator::CASE_VALUES].map do |val|
              LOG.debug "Parse -#{val}-"
              val.to_s.each_line.map do |single_line|
                # string interpolation if needed
                if single_line.match(/\#\{.+\}/)
                  LOG.debug "'#{single_line}' evaluate"
                  single_line = eval(%Q["#{single_line}"])
                end
                if match = single_line.match(/(.+)##\*([[:digit:]]+)$/)
                  str, num = match.captures
                  LOG.debug "'#{str}'' repeat #{num} times"
                  "#{str}\n"*num.to_i
                else
                  LOG.debug "simple value `#{single_line}`"
                  single_line
                end
              end.join
            end
            updated = format template, *values

            # Remove lines marked to remove by ##- tag
            updated = updated.to_s.each_line.map do |single_line|
              if /(.*)##\-(.*)/ =~ single_line
                ""
              else
                single_line
              end
            end.join

          rescue StandardError => e
            LOG.error 'Cannot substitute values to template'
            LOG.error "Template: #{template}"
            LOG.error "Values: #{@current_case[Generator::CASE_VALUES]}"
            LOG.error "Instruction: #{@instruction}"
            LOG.error "Command: #{@command}"
            LOG.error e
            raise 'Failed to create tests'
          end

          content.puts updated
        else
          LOG.debug 'No values to substitute'
          content.puts template
        end

        test_check_type = @command.key?(Generator::TEST_CHECK_TYPE) ? @command[Generator::TEST_CHECK_TYPE] : Generator::TEMPLATE_CHECK_POSITIVE
        case_check_type = @current_case.key?(Generator::CASE_CHECK_TYPE) ? @current_case[Generator::CASE_CHECK_TYPE] : test_check_type

        # LOG.debug "case checking type is #{case_check_type}"
        content.puts @predefined.definition case_check_type

        write_test_main_wrapper_block content, run_options

        content.string
      end
    end
  end
end
