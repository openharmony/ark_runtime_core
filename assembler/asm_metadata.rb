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

class Attritute < SimpleDelegator
  def getter_name
    r = name.capitalize
    type == 'bool' ? 'Is' + r : 'Get' + r
  end

  def setter_name
    'Set' + name.capitalize
  end

  def bool?
    type == 'bool'
  end

  def enum?
    type == 'enum'
  end

  def size?
    type == 'size'
  end

  def multiple?
    !bool? && multiple
  end

  def applicable_to?(item_type)
    applicable_to.include?(item_type)
  end

  def set_flags?
    (defined? flags) && flags.any? || enum? && values.any? { |v| v.flags && v.flags.any? }
  end
end

module Metadata
  module_function

  def attributes
    @data.attributes.map do |op|
      Attritute.new(op)
    end
  end

  def language
    @data.language || ''
  end

  def extends_default?
    !language.empty?
  end

  def item_types
    ['record', 'field', 'function', 'param']
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  Metadata.wrap_data(data)
end

module MetadataGen
  module_function

  def attribute_name(attribute)
    return attribute.name if Metadata.language.empty?
    "#{Metadata.language.downcase}.#{attribute.name}"
  end

  def class_name(item_type)
    item_type.capitalize + 'Metadata'
  end

  def validate_body(item_type, is_bool)
    attributes = Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? == is_bool }
    body = []
    indent = ' ' * 4

    attributes.each do |a|
      body << "#{indent}if (attribute == \"#{attribute_name(a)}\") {"

      unless a.multiple?
        body << "#{indent * 2}if (HasAttribute(attribute)) {"
        body << "#{indent * 3}return Error(\"Attribute '#{attribute_name(a)}' already defined\","
        body << "#{indent * 3}             Error::Type::MULTIPLE_ATTRIBUTE);"
        body << "#{indent * 2}}"
      end

      if a.enum?
        a.values.each do |v|
          body << "#{indent * 2}if (value == \"#{v.value}\") {"
          body << "#{indent * 3}return {};"
          body << "#{indent * 2}}"
          body << ""
        end

        body << "#{indent * 2}return Error(std::string(\"Attribute '#{attribute_name(a)}' has incorrect value '\").append(value) +"
        body << "#{indent * 2}             R\"('. Should be one of #{a.values.map(&:value)})\", Error::Type::INVALID_VALUE);"
      elsif a.size?
        body << "#{indent * 2}return ValidateSize(value);"
      else
        body << "#{indent * 2}return {};"
      end

      body << "#{indent}}"
      body << ""
    end

    Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? != is_bool }.each do |a|
      body << "#{indent}if (attribute == \"#{attribute_name(a)}\") {"
      body << "#{indent * 2}return Error(\"Attribute '#{attribute_name(a)}' #{is_bool ? "must" : "must not"} have a value\","
      body << "#{indent * 2}             #{is_bool ? "Error::Type::MISSING_VALUE" : "Error::Type::UNEXPECTED_VALUE"});"
      body << "#{indent}}"
      body << ""
    end

    if Metadata::extends_default?
      args = ['attribute']
      args << 'value' unless is_bool
      body << "#{indent}return pandasm::#{class_name(item_type)}::Validate(#{args.join(', ')});"
    else
      body << "#{indent}return Error(std::string(\"Unknown attribute '\").append(attribute) + \"'\","
      body << "#{indent}             Error::Type::UNKNOWN_ATTRIBUTE);"
    end

    body
  end

  def set_flags_body(item_type, is_bool)
    attributes = Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? == is_bool && a.set_flags? }
    body = []
    indent = ' ' * 4

    attributes.each do |a|
      body << "#{indent}if (attribute == \"#{attribute_name(a)}\") {"

      if defined? a.flags
        body << "#{indent * 2}SetAccessFlags(GetAccessFlags() | #{a.flags.join(' | ')});"
      end

      if a.enum?
        a.values.select { |v| v.flags && v.flags.any? }.each do |v|
          body << "#{indent * 2}if (value == \"#{v.value}\") {"
          body << "#{indent * 3}SetAccessFlags(GetAccessFlags() | #{v.flags.join(' | ')});"
          body << "#{indent * 2}}"
          body << ""
        end
      end

      body << "#{indent}}"
    end

    if Metadata::extends_default?
        args = ['attribute']
        args << 'value' unless is_bool
        body << "#{indent}pandasm::#{class_name(item_type)}::SetFlags(#{args.join(', ')});"
    end

    body
  end

  def remove_flags_body(item_type, is_bool)
    attributes = Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? == is_bool && a.set_flags? }
    body = []
    indent = ' ' * 4

    attributes.each do |a|
      body << "#{indent}if (attribute == \"#{attribute_name(a)}\") {"

      if defined? a.flags
        body << "#{indent * 2}if ((GetAccessFlags() & #{a.flags.join(' | ')}) != 0) {"
        body << "#{indent * 3}SetAccessFlags(GetAccessFlags() ^ (#{a.flags.join(' | ')}));"
        body << "#{indent * 2}}"
      end

      if a.enum?
        a.values.select { |v| v.flags && v.flags.any? }.each do |v|
          body << "#{indent * 2}if (value == \"#{v.value}\") {"
          body << "#{indent * 3}if ((GetAccessFlags() & (#{v.flags.join(' | ')})) != 0) {"
          body << "#{indent * 4}SetAccessFlags(GetAccessFlags() ^ (#{v.flags.join(' | ')}));"
          body << "#{indent * 3}}"
          body << "#{indent * 2}}"
        end
      end

      body << "#{indent}}"
    end

    if Metadata::extends_default?
      args = ['attribute']
      args << 'value' unless is_bool
      body << "#{indent}pandasm::#{class_name(item_type)}::RemoveFlags(#{args.join(', ')});"
    end

    body
  end

  def arg_list(is_bool)
    args = ['std::string_view attribute']
    args << 'std::string_view value' if !is_bool
    args
  end

  def add_unused_attribute(arg)
    "[[maybe_unused]] #{arg}"
  end

  def validate_arg_list(item_type, is_bool)
    args = arg_list(is_bool)
    return args if Metadata::extends_default?

    attributes = Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? == is_bool }
    args[0] = add_unused_attribute(args[0]) if attributes.none?
    args[1] = add_unused_attribute(args[1]) if args[1] && attributes.none? { |a| a.enum? }
    args
  end

  def flags_arg_list(item_type, is_bool)
    args = arg_list(is_bool)
    return args if Metadata::extends_default?

    attributes = Metadata::attributes.select { |a| a.applicable_to?(item_type) && a.bool? == is_bool && a.set_flags? }
    use_value = attributes.any? { |a| a.enum? && a.values.any? { |v| v.flags && v.flags.any? } }
    args[0] = add_unused_attribute(args[0]) if attributes.none?
    args[1] = add_unused_attribute(args[1]) if args[1] && !use_value
    args
  end

end
