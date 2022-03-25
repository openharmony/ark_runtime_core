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

require 'optparse'
require 'yaml'
require 'erb'

class FullMd
  attr_accessor :full, :properties_hash, :exceptions_hash, :verification_hash

  def initialize(spec)
    @template_file = File.join(__dir__, 'full_md.erb')
    @full = spec
    @exceptions_hash = convert_to_hash(@full['exceptions'])
    @properties_hash = convert_to_hash(@full['properties'])
    @verification_hash = convert_to_hash(@full['verification'])
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

  def covered_description(d)
    md(d['assertion']) + 
      ' [' + (d['tests'].any? ? d['tests'].join(', ') : "\`not covered\`") + ']' + 
      (d['non_testable'] ? ' - Non-testable' : '')
  end

  def props(properties)
    '[' + md(properties.join(', ')) + ']'
  end

  def verification_entry(v)
    md(@verification_hash[v['verification']]) + ' [' + (v['tests'].any? ? v['tests'].join(', ') : "\`not covered\`") + ']'
  end

  def exception_entry(v)
    md(@exceptions_hash[v['exception']]) + ' [' + (v['tests'].any? ? v['tests'].join(', ') : "\`not covered\`") + ']'
  end

  def md(s)
    s.gsub(/\_/, '\\_').gsub(/\n/, ' ').rstrip
  end

  def render
    @template = File.read(@template_file)
    ERB.new(@template, nil, '%-').result(binding)
  end

  def convert_to_hash(a)
    hash = a.map { |i| [i['tag'], i['description']] }.to_h
    # hash.default_proc = proc { |_, k| raise KeyError, "#{k} not found" }
    hash
  end
end
