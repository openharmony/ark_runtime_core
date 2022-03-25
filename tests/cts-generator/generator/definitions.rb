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
  class Definitions
    def initialize(data)
      # YAML:
      #   - name: Generator::DEF_TEMPLATE_NAME
      #     template: Generator::DEF_TEMPLATE
      @data = data
    end

    def definition(item_name)
      LOG.debug "search for '#{item_name}' definition"
      @data.each do |item|
        LOG.debug "check #{item}"

        if item.key?(Generator::DEF_TEMPLATE_NAME) && item[Generator::DEF_TEMPLATE_NAME] == item_name
          return item[Generator::DEF_TEMPLATE]
        end
      end
      raise "Definition of '#{item_name}' is not found"
    end

    def exist? (item_name)
      LOG.debug "check #{item_name} exists in definitions"
      @data.each do |item|
        if item.key?(Generator::DEF_TEMPLATE_NAME) && item[Generator::DEF_TEMPLATE_NAME] == item_name
          return true
        end
      end
      return false
    end
  end
end
