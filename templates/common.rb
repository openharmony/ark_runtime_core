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

require 'ostruct'
require 'delegate'

class Option < SimpleDelegator
  def field_name
    name.tr('-', '_').tr('.', '_') + '_'
  end

  def getter_name
    r = name.split(Regexp.union(['-', '.'])).map(&:capitalize).join
    type == 'bool' ? 'Is' + r : 'Get' + r
  end

  def setter_name
    'Set' + name.split(Regexp.union(['-', '.'])).map(&:capitalize).join
  end

  def deprecated?
    respond_to?(:deprecated) && deprecated
  end

  def default_value
    return default_constant_name if need_default_constant
    return '{' + default.map { |e| expand_string(e) }.join(', ') + '}' if type == 'arg_list_t'
    return expand_string(default) if type == 'std::string'
    default
  end

  def full_description
    full_desc = description
    full_desc.prepend("[DEPRECATED] ") if deprecated?
    if defined? possible_values
      full_desc += '. Possible values: ' + possible_values.inspect
    end
    Common::to_raw(full_desc + '. Default: ' + default.inspect)
  end

  def expand_string(s)
    @expansion_map ||= {
      '$ORIGIN' => 'exe_dir_'
    }
    @expansion_map.each do |k, v|
      ret = ""
      if s.include?(k)
        split = s.split(k);
        for i in 1..split.length() - 1
          ret += v + ' + ' + Common::to_raw(split[i])+ ' + '
        end
        return ret.delete_suffix(' + ')
      end
    end
    Common::to_raw(s)
  end

  def need_default_constant
    type == 'int' || type == 'double' || type == 'uint32_t' || type == 'uint64_t'
  end

  def default_constant_name
    name.tr('-', '_').tr('.', '_').upcase + '_DEFAULT'
  end
end

class Event < SimpleDelegator
  def method_name  
    'Event' + name.split('-').map(&:capitalize).join
  end  

  def args_list
    args = ''
    delim = ''
    fields.each do |field|
      args.concat(delim, field.type, ' ', field.name)           
      delim = ', '       
    end
    return args
  end
  
  def print_line
    qoute = '"'
    line = 'events_file'
    delim = ' << '
    fields.each do |field|        
      line.concat(delim, qoute, field.name, qoute) 
      delim = ' << ":" << ' 
      line.concat(delim, field.name)  
      delim = ' << "," << '    
    end 
    return line
  end  
   
end

module Common
  module_function

  def options
    @data.options.each_with_object([]) do |option, options|
      option_hash = option.to_h
      if option_hash.include?(:lang)
        new_option = option_hash.clone
        new_option.delete(:lang)
        option.lang.each do |lang|
          new_option[:name] = "#{lang}.#{option.name}"
          options << Option.new(OpenStruct.new(new_option))
        end
        new_option[:name] = "#{option.name}"
        options << Option.new(OpenStruct.new(new_option))
      else
        options << Option.new(option)
      end
    end
  end

  def events
    @data.events.map do |op|
      Event.new(op)
    end
  end

  def module
    @data.module
  end

  def to_raw(s)
    'R"(' + s + ')"'
  end

  def wrap_data(data)
    @data = data
  end
end

def Gen.on_require(data)
  Common.wrap_data(data)
end
