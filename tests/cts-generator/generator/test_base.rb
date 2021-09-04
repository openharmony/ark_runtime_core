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

require 'stringio'

module Generator
  class TestBase
    def initialize(isa, command, definitions, predefined, skip_header)
      @isa = isa
      @command = command
      @predefined = predefined
      @definitions = definitions
      @skip_header = skip_header
    end

    def write_test_initial_block(content)
      if !@skip_header
        isa = @isa.clone
        isa.merge! @command[Generator::TEST_COMMAND_ISA]
        # Currently generator supports only single instruction per test. However coverage tool supports array.
        # So convert to yaml as array of single element.

        content.puts '# Copyright (c) 2021 Huawei Device Co., Ltd.
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
# limitations under the License.'
        content.puts '#'
        content.puts '# The following comment until the empty line must be a valid YAML document'
        content.puts "# containing exact copies of ISA specification assertions relevant to this test.\n"
        content.puts [isa].to_yaml.split("\n").map { |line| '#%s' % line }.join("\n")
        content.puts "\n"
      end
    end

    def write_runner_options(content, run_options, ignore, bugids, tags, description, test_panda_options)
      if !@skip_header
        run_options.each do |s|
          content.puts "## runner-option: #{s}"
        end
        content.puts "## runner-option: ignore" if ignore
        content.puts "## runner-option: bugid: #{bugids.join ', '}" if bugids.length > 0
        content.puts "## runner-option: tags: #{tags.join ', '}" if tags.length > 0
        content.puts "## panda-options: #{test_panda_options}" if test_panda_options.length > 0

        # Options for test with main wrapper function to avoid false-positive cases, for executable tests
        if !(run_options.include?('compile-failure') || run_options.include?('compile-only') ||
             run_options.include?('verifier-failure') || run_options.include?('verifier-only'))
          content.puts "## runner-option: main-exitcode-wrapper"
        end

        if description != ""
          content.puts "\n# Test description:"
          description.split("\n").each {|t| content.puts "\#   #{t}" }
          content.puts ""
        end
      end
    end

    def get_header_template()
      @command[Generator::TEST_HEADER_TEMPLATE]
    end

    def write_test_main_block(content)
      # Mandatory empty line after header
      ## TEST_HEADER_TEMPLATE

      content.puts "\n"
      header_template = get_header_template
      if header_template != nil
        header_template.each do |template|
          if @definitions.exist?(template)
            content.puts @definitions.definition template
          else
            content.puts @predefined.definition template
          end
        end
      else
        content.puts @predefined.definition Generator::DEF_MAIN
      end
    end

    def write_test_main_wrapper_block(content, run_options)
      # Add main wrapper to executable tests
      if !(run_options.include?('compile-failure') || run_options.include?('compile-only') ||
           run_options.include?('verifier-failure') || run_options.include?('verifier-only'))
        content.puts @predefined.definition 'main-exitcode-wrapper'
      end
    end

  end
end
