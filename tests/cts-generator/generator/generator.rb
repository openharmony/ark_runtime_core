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

require 'logger'
require 'fileutils'
require 'date'

require_relative 'command'
require_relative 'definitions'
require_relative 'test'
require_relative 'test_case'
require_relative 'parser'

module Generator
  LOG = Logger.new(STDOUT)
  LOG.level = Logger::INFO

  DEFINITIONS =         'definitions'
  DEF_TEMPLATE =        'template'
  DEF_MAIN =            'main'
  DEF_TEMPLATE_NAME =   'name'

  TESTS =                'tests'
  TESTS_INCLUDE =        'include'
  TEST_SKIP =            'skip'
  TEST_ONLY =            'only'
  TEST_NAME =            'file-name'

  TEST_ISA =             'isa'
  TEST_ISA_TITLE =       'title'
  TEST_ISA_DESCRIPTION = 'description'
  TEST_COMMAND_ISA =     'isa'
  TEST_COMMAND_ISA_DESCR = 'description'
  TEST_CMD_ISA_INSTR =   'instructions'

  TEST_COMMANDS =        'commands'
  TEST_DESCRIPTION=      'description'
  TEST_PANDA_OPTIONS =   'panda-options'
  TEST_RUN_OPTIONS =     'runner-options'
  TEST_COMMAND_SKIP =    'skip'
  TEST_COMMAND_ONLY =    'only'
  COMMAND_FILE_NAME =    'file-name'
  TEST_CODE_TEMPLATE =   'code-template'
  TEST_HEADER_TEMPLATE = 'header-template'
  TEST_CHECK_TYPE =      'check-type'
  TEST_IGNORE =          'ignore'
  TEST_BUG_ID =          'bugid'
  TEST_TAGS =            'tags'
  TEST_TEMPLATE_CASES =  'template-cases'
  TEST_TEMPLATE_CASE_VALUES = 'values'
  TEST_TEMPLATE_CASE_EXCLUDE_VAL = 'exclude'
  TEST_TEMPLATE_CASE_BUGID = 'bugid'
  TEST_TEMPLATE_CASE_IGNORE = 'ignore'
  TEST_TEMPLATE_CASE_TAG = 'tags'

  CASES =            'cases'
  CASE_VALUES =      'values'
  CASE_ID =          'id'
  CASE_RUN_OPTIONS = 'runner-options'
  CASE_CHECK_TYPE =  'case-check-type'
  CASE_TEMPLATE =    'case-template'
  CASE_HEADER_TEMPLATE = 'case-header-template'
  CASE_IGNORE =      'ignore'
  CASE_BUG_ID =      'bugid'
  CASE_TAGS =        'tags'
  CASE_DESCRIPTION = 'description'

  TEMPLATE_CHECK_POSITIVE = 'check-positive'

end
