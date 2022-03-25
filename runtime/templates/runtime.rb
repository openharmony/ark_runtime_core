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

def array_type?(type)
  type[-1] == '['
end

def get_object_type(type)
  return 'panda::coretypes::Array *' if array_type?(type)
  t = Runtime::coretypes.find { |t| t.managed_class == type }
  return '%s *' % t.mirror_class if t
  'panda::ObjectHeader *'
end

def get_type(type)
  @type_map ||= {
    'void' => ['void'],
    'u1' => ['uint8_t'],
    'i8' => ['int8_t'],
    'u8' => ['uint8_t'],
    'i16' => ['int16_t'],
    'u16' => ['uint16_t'],
    'i32' => ['int32_t'],
    'u32' => ['uint32_t'],
    'i64' => ['int64_t'],
    'u64' => ['uint64_t'],
    'f32' => ['float'],
    'f64' => ['double'],
    'any' => ['int64_t', 'int64_t'],
    'acc' => ['int64_t', 'int64_t'],
    'string_id' => ['uint32_t'],
    'method_id' => ['uint32_t'],
  }
  @type_map[type] || get_object_type(type)
end

def get_ret_type(type)
  @ret_type_map ||= {
    'void' => 'void',
    'u1' => 'uint8_t',
    'i8' => 'int8_t',
    'u8' => 'uint8_t',
    'i16' => 'int16_t',
    'u16' => 'uint16_t',
    'i32' => 'int32_t',
    'u32' => 'uint32_t',
    'i64' => 'int64_t',
    'u64' => 'uint64_t',
    'f32' => 'float',
    'f64' => 'double',
    'any' => 'DecodedTaggedValue',
    'acc' => 'DecodedTaggedValue',
    'string_id' => 'uint32_t',
    'method_id' => 'uint32_t',
  }
  @ret_type_map[type] || get_object_type(type)
end

def get_effective_type(type)
  get_type(type)
end

def get_ret_effective_type(type)
  get_ret_type(type)
end

class Intrinsic < SimpleDelegator
  def need_abi_wrapper?
    Object.send(:get_ret_type, signature.ret) != Object.send(:get_ret_effective_type, signature.ret) ||
    signature.args.any? { |arg| Object.send(:get_ret_type, arg) != Object.send(:get_ret_effective_type, arg) }
  end

  def enum_name
    res = name.gsub(/([A-Z]+)([A-Z][a-z])/,'\1_\2')
    res.gsub(/([a-z\d])([A-Z])/,'\1_\2').upcase
  end

  def wrapper_impl
    return impl + 'AbiWrapper' if need_abi_wrapper?
    impl
  end
end

module Runtime
  module_function

  def intrinsics
    @data.intrinsics.map do |intrinsic|
      Intrinsic.new(intrinsic)
    end
  end

  def intrinsics_namespace
    @data.intrinsics_namespace
  end

  def coretypes
    @data.coretypes
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  Runtime.wrap_data(data)
end
