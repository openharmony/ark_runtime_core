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

# frozen_string_literal: true

require 'optparse'
require 'yaml'
require 'erb'

class UncoveredMd
  attr_accessor :uncovered

  def initialize(uncovered)
    @template_file = File.join(__dir__, 'uncovered_md.erb')
    @uncovered = uncovered['groups']
  end

  def generate(file)
    File.open(file, 'w+') do |f|
      f.write(render)
    end
  end

  private 

  def format_array(i)
    i.nil? ? '' : i['format'].join(', ').gsub(/\_/, '\\_')
  end

  def props(properties)
    '[' + md(properties.join(', ')) + ']'
  end

  def md(s)
    s.gsub(/\_/, '\\_').gsub(/\n/, ' ').rstrip
  end

  def render
    @template = File.read(@template_file)
    ERB.new(@template, nil, '%-').result(binding)
  end
end
