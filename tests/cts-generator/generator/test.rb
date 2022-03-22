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

module Generator
  class Test
    def initialize(test, predefined, process_only, output, skip_header)
      # test - YAML:
      #   name: Generator::TEST_NAME
      #   instruction: Generator::TEST_INSTRUCTION
      #   command: Generator::TEST_COMMANDS
      @test = test
      @predefined = predefined

      @test_name = test[Generator::TEST_NAME]
      @isa = test[Generator::TEST_ISA]
      @commands = test[Generator::TEST_COMMANDS]
      @skip = @test[Generator::TEST_SKIP]
      @process_only = process_only
      @output = output
      @definitions = Definitions.new @test[Generator::DEFINITIONS]
      @skip_header = skip_header
    end

    def parse
      LOG.info "START parsing test '#{@test_name}'"

      if !@skip
        if @test.key?(Generator::TEST_COMMANDS)
          parse_commands
        else
          LOG.error "Test '#{@test_name}' does not have definition of instruction commands"
        end
      else
        LOG.warn "Skip test '#{@test_name}' generation due to skip property is set"
      end
    end

    def parse_commands
      @commands.each do |raw_command|
        if @process_only
          process = raw_command.key?(Generator::TEST_COMMAND_ONLY) && raw_command[Generator::TEST_COMMAND_ONLY]
          LOG.debug "Some command has 'only' key, process command: #{process}"
        else
          process = true
          LOG.debug "No 'only' key is defined for any command, process command: #{process}"
        end
        if process
          command = Command.new raw_command, @isa, @test_name, @definitions, @predefined, @output, @skip_header
          command.parse
        end

      end
    end
  end
end
