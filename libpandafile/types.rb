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

require 'delegate'

module PandaFile
  module_function

  def types
    @data.types.sort_by(&:code)
  end

  def asm_name(type)
    type.asm_name ? type.asm_name : type.name
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  PandaFile.wrap_data(data)
end
