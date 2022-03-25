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

module Generator
  class Parser
    def initialize(data, output, src_dir, skip_header)
      @data = data
      @output = output
      @src_dir = src_dir

      LOG.info 'Generator created'
      LOG.debug "@data first 3 keys are: '#{data.keys.first(3)}'"

      # data - YAML:
      #  tests: Generator::TESTS
      #  definitions: Generator::DEFINITIONS

      # Tests definitions
      @tests = @data[Generator::TESTS]
      # some predefined templates as headers, prefixes, postfixes, etc.
      raise "'#{Generator::DEFINITIONS}' is not found in main yaml" unless @data.key? Generator::DEFINITIONS
      @predefined = Definitions.new @data[Generator::DEFINITIONS]
      @skip_header = skip_header
    end

    def parse(skip)
      start_time = Time.now
      updated_tests = @tests.flat_map do |test|
        if test.key? Generator::TESTS_INCLUDE
          file_name = test[Generator::TESTS_INCLUDE]
          included_data = YAML.load_file("#{@src_dir}/#{file_name}")
          if !skip
            res = JSON::Validator.fully_validate("#{@src_dir}/yaml-schema.json", included_data)
            unless res.empty?
              puts "Template '#{file_name}' contains several errors:"
              puts res
              raise "Schema validation error, please update template '#{file_name}' to match schema to generate tests"
            end
          end

          # Add definitions to each test group from file.
          included_data[Generator::TESTS].map do |single_test|
            single_test[Generator::DEFINITIONS] = included_data[Generator::DEFINITIONS]
            single_test
          end
          included_data[Generator::TESTS]
        else
          test
        end
      end

      @tests = updated_tests

      test_has_only_key = false
      command_has_only_key = false
      @tests.each do |raw_test|
        test_has_only_key ||= raw_test.key?(Generator::TEST_ONLY) && raw_test[Generator::TEST_ONLY]
        raw_test[Generator::TEST_COMMANDS].each do |command|
          command_has_only_key ||= command.key?(Generator::TEST_COMMAND_ONLY) && command[Generator::TEST_COMMAND_ONLY]
        end
      end
      LOG.debug "test_has_only_key = #{test_has_only_key}"
      LOG.debug "command_has_only_key = #{command_has_only_key}"
      options = { test_has_only_key: test_has_only_key, command_has_only_key: command_has_only_key }

      LOG.debug options

      # Generated test directory
      FileUtils.rm_rf @output if File.exist? @output
      FileUtils.mkdir_p @output unless File.exist? @output
      iterate_tests options
      LOG.info "Tests generation done in #{Time.now - start_time} sec"
    end

    def iterate_tests(options)
      LOG.debug('Iterate over all tests')
      @tests.each do |raw_test|
        if options[:test_has_only_key]
          process = raw_test.key?(Generator::TEST_ONLY) && raw_test[Generator::TEST_ONLY] || options[:command_has_only_key]
          LOG.debug "'only:' is defined for some test, process current: #{process}"
          if process
            # Process all commands
            test = Test.new raw_test, @predefined, options[:command_has_only_key], @output, @skip_header
            test.parse
          end
        else
          LOG.debug "'only:' is not defined for test, process current: true"
          # Process commands, if it has only, to only them should be processed
          test = Test.new raw_test, @predefined, options[:command_has_only_key], @output, @skip_header
          test.parse
        end

      end
    end
  end
end
