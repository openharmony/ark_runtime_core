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

require_relative './single_test'

module Generator
  class Command
    def initialize(command, isa, test_name, definitions, predefined, output, skip_header)

      # command - YAML:
      #   isa: Generator::TEST_ISA
      #       title: Generator::TEST_ISA_TITLE
      #       description: Generator::TEST_ISA_DESCRIPTION
      #   file-name:
      #   skip: Generator::TEST_COMMAND_SKIP
      #   code-template: Generator::TEST_CODE_TEMPLATE
      #   cases: Generator::CASES

      @command = command
      @isa = isa
      @test_name = test_name
      @predefined = predefined
      @definitions = definitions
      @output = output
      @skip_header = skip_header
      LOG.debug 'Command:initialize'
      LOG.debug command

      @skip = command.key?(Generator::TEST_COMMAND_SKIP) ? command[Generator::TEST_COMMAND_SKIP] : false

    end

    def parse()
      if @skip
        LOG.info("Skip test '#{@test_name}' due to \"skip\": true")
        return
      end
      if @command.key?(Generator::CASES)
        LOG.debug "Test '#{@test_name}' contains several cases"
        create_test_cases
      else
        LOG.debug "Test '#{@test_name}' has no cases"
        create_single_test
      end
    end

    def create_test_cases
      sig_name = @command[Generator::COMMAND_FILE_NAME]

      test_dir = "#{@output}/#{@test_name}"

      FileUtils.mkdir_p test_dir unless File.exist? test_dir

      if @command.key?(Generator::TEST_TEMPLATE_CASES)
        LOG.debug "#{@test_name} has template cases"
        @command[Generator::TEST_TEMPLATE_CASES].each_with_index do |template_case, idx1|
          values = template_case[Generator::TEST_TEMPLATE_CASE_VALUES]
          excludes = template_case[Generator::TEST_TEMPLATE_CASE_EXCLUDE_VAL] || []
          bugids = template_case[Generator::TEST_TEMPLATE_CASE_BUGID] || []
          ignore = template_case[Generator::TEST_TEMPLATE_CASE_IGNORE] || false
          tags = template_case[Generator::TEST_TEMPLATE_CASE_TAG] || []

          template = format @command[Generator::TEST_CODE_TEMPLATE], *values
          template = template.gsub('*s', '%s')
          index = 0
          @command[Generator::CASES].each do |current_case|
            if !excludes.include? current_case[Generator::CASE_ID]
              LOG.debug "Process case '#{@test_name}_#{sig_name}_#{idx1}_#{index}'"
              file_name = "#{test_dir}/#{@test_name}_#{sig_name}_#{idx1}_#{index}.pa"
              process_case_values file_name, current_case, index, sig_name, test_dir, template, bugids, ignore, tags
              index = index + 1
            end
          end
        end
      else
        @command[Generator::CASES].each_with_index do |current_case, index|
          LOG.debug "Process case '#{@test_name}_#{sig_name}_#{index}'"
          file_name = "#{test_dir}/#{@test_name}_#{sig_name}_#{index}.pa"
          template = @command[Generator::TEST_CODE_TEMPLATE]
          process_case_values file_name, current_case, index, sig_name, test_dir, template, [], false, []
        end
      end


    end

    def process_case_values(file_name, current_case, index, sig_name, test_dir, template, bugids, ignore, tags)

      File.open(file_name, 'w') do |test_file|
        test_case = TestCase.new @command, current_case, @isa, @definitions, @predefined, template, @skip_header
        output = test_case.create_single_test_case bugids, ignore, tags
        test_file.puts output
      end
    end

    def create_single_test
      test_dir = "#{@output}/#{@test_name}"
      FileUtils.mkdir_p test_dir unless File.exist? test_dir
      file_name = @command[Generator::COMMAND_FILE_NAME]

      File.open("#{test_dir}/#{@test_name}_#{file_name}.pa", 'w') do |test_file|
        test = SingleTest.new @command, @isa, @definitions, @predefined, @skip_header
        test_file.puts test.create_single_test
        # test_file.puts @predefined.definition check_type
      end
    end
  end
end
