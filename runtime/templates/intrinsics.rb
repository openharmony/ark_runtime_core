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

require 'ostruct'
require 'set'
require 'delegate'

def get_shorty_type(type)
  @shorty_type_map ||= {
    'void' => 'VOID',
    'u1' => 'U1',
    'i8' => 'I8',
    'u8' => 'U8',
    'i16' => 'I16',
    'u16' => 'U16',
    'i32' => 'I32',
    'u32' => 'U32',
    'i64' => 'I64',
    'u64' => 'U64',
    'f32' => 'F32',
    'f64' => 'F64',
    'any' => 'TAGGED',
    'acc' => 'TAGGED',
    'string_id' => 'U32',
    'method_id' => 'U32',
  }
  @shorty_type_map[type] || 'REFERENCE'
end

def primitive_type?(type)
  @primitive_types ||= Set[
    'void',
    'u1',
    'i8',
    'u8',
    'i16',
    'u16',
    'i32',
    'u32',
    'i64',
    'u64',
    'f32',
    'f64',
    'any',
    'acc',
    'string_id',
    'method_id',
  ]
  @primitive_types.include?(type)
end

def get_primitive_descriptor(type)
  @primitive_type_map ||= {
    'u1' => 'Z',
    'i8' => 'B',
    'u8' => 'H',
    'i16' => 'S',
    'u16' => 'C',
    'i32' => 'I',
    'u32' => 'U',
    'i64' => 'J',
    'u64' => 'Q',
    'f32' => 'F',
    'f64' => 'D',
    'any' => 'A',
    'acc' => 'A',
    'string_id' => 'S',
    'method_id' => 'M',
  }
  @primitive_type_map[type]
end

def object_type?(type)
  !primitive_type?(type)
end

def get_object_descriptor(type)
  rank = type.count('[')
  type = type[0...-(rank * 2)] if rank > 0
  return '[' * rank + get_primitive_descriptor(type) if primitive_type?(type)
  '[' * rank + 'L' + type.gsub('.', '/') + ';'
end

def floating_point_type?(type)
  @fp_types ||= Set[
    'f32',
    'f64',
  ]
  @fp_types.include?(type)
end

class Intrinsic < SimpleDelegator
  def enum_name
    res = name.gsub(/([A-Z]+)([A-Z][a-z])/,'\1_\2')
    res.gsub(/([a-z\d])([A-Z])/,'\1_\2').upcase
  end

  def need_abi_wrapper?
    defined? orig_impl
  end
end

module Runtime
  module_function

  def intrinsics
    @data.map do |intrinsic|
      Intrinsic.new(intrinsic)
    end
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  Runtime.wrap_data(data)
end
