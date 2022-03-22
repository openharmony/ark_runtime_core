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

require 'stringio'

require_relative 'test_base'

module Generator
  class SingleTest < TestBase
    def initialize(command, isa, definitions, predefined, skip_header)
      super isa, command, definitions, predefined, skip_header

      # command - YAML:
      #   sig: Generator::TEST__SIG
      #   check-type: Generator::CASE_CHECK_TYPE
      #   cases: nil

      @command = command
      @isa = isa
      @definitions = definitions
      @predefined = predefined
    end

    def create_single_test
      StringIO.open do |content|
        write_test_initial_block content

        test_run_options = @command[Generator::TEST_RUN_OPTIONS]
        run_options = test_run_options
        run_options = [] if run_options.nil?
        ignore = @command[Generator::TEST_IGNORE] || false
        bugids = @command[Generator::TEST_BUG_ID] || []
        tags = @command[Generator::TEST_TAGS] || []
        description = @command[Generator::TEST_DESCRIPTION] || ""
        test_panda_options = @command[Generator::TEST_PANDA_OPTIONS] || ""

        write_runner_options content, run_options, ignore, bugids, tags, description, test_panda_options

        write_test_main_block content

        content.puts @command[Generator::TEST_CODE_TEMPLATE]

        case_check_type = @command.key?(Generator::TEST_CHECK_TYPE) ? @command[Generator::TEST_CHECK_TYPE] : Generator::TEMPLATE_CHECK_POSITIVE
        # LOG.debug "case checking type is #{case_check_type}"
        content.puts @predefined.definition case_check_type

        write_test_main_wrapper_block content, run_options

        content.string
      end
    end
  end
end
