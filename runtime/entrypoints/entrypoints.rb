#!/usr/bin/env ruby
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

require 'yaml'

class String
  def snakecase
    self.gsub(/::/, '/').
    gsub(/([A-Z]+)([A-Z][a-z])/,'\1_\2').
    gsub(/([a-z\d])([A-Z])/,'\1_\2').
    tr("-", "_").
    downcase
  end
end

class Entrypoint
  def initialize(dscr)
    @dscr = dscr
  end

  def name
    @dscr['name']
  end

  def enum_name
    @dscr['name'].snakecase.upcase
  end

  def bridge_name
    @dscr.entrypoint.nil? ? "#{name}Bridge" : @dscr.entrypoint
  end

  def entrypoint_name
    @dscr.entrypoint.nil? ? "#{name}Entrypoint" : @dscr.entrypoint
  end

  def get_entry
    "#{name}Entrypoint"
  end

  def signature
    @dscr['signature']
  end

  def external?
    has_property? 'external'
  end

  def has_property? prop
    @dscr['properties']&.include? prop
  end

end

module Compiler
  module_function

  def entrypoints
    @entrypoints ||= @data['entrypoints'].map {|x| Entrypoint.new x }
  end

  def entrypoints_crc32
    require "zlib"
    Zlib.crc32(entrypoints.map(&:signature).join)
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  Compiler.wrap_data(data)
end
