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

class ReportMd
  attr_accessor :rep

  def initialize(rep)
    @template_file = File.join(__dir__, 'report.erb')
    @rep = rep
  end

  def generate
    STDOUT.write(render)
  end

  private

  def render
    @template = File.read(@template_file)
    ERB.new(@template, nil, '%-').result(binding)
  end
end
