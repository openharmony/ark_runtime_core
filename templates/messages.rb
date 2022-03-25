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

require 'strscan'

Message = Struct.new(:name, :component, :number, :level, :args, :short_message, :message, keyword_init: true) do
  def macro_name
    ['LOG', component, Messages.split_words(name)].flatten.join('_').upcase
  end

  def scan_until_exclusive(scanner, pattern)
    scanner.scan_until(Regexp.new(Regexp.escape(pattern)))&.delete_suffix(pattern)
  end

  def escape_string_literal(string)
    %("#{string.gsub('"', '\\"').gsub("\n", '\\n').gsub("\t", '\\t').gsub('\\', '\\\\')}")
  end

  def stream_ops(is_short)
    msg = is_short ? "#{component} #{level} #{number}: #{short_message}" : message
    scanner = StringScanner.new(msg)
    parts = []
    until scanner.eos?
      str_part = scan_until_exclusive(scanner, '${')
      if str_part
        parts << escape_string_literal(str_part) unless str_part.empty?
        expr_part = scan_until_exclusive(scanner, '}')
        if expr_part
          parts << expr_part
        else
          raise "Message template '#{msg}' has a '${' not followed by a '}'"
        end
      elsif !scanner.eos?
        parts << escape_string_literal(scanner.rest)
        scanner.terminate
      end
    end
    parts.join(' << ')
  end
end

module Messages
  module_function

  def split_words(string)
    scanner = StringScanner.new(string)
    words = []
    until scanner.eos?
      word = scanner.scan(/[A-Z0-9][a-z0-9]*/)
      if word.empty?
        raise "Message name '#{string}' is not in UpperCamelCase"
      else
        words << word
      end
    end
    words
  end

  def load_messages(data)
    @component = data.component.capitalize
    @namespace = data.namespace
    @enum_name = data.enum_name
    @messages = data.messages.each_pair.map do |name, msg_data|
      name = name.to_s
      level = msg_data.level || data.default_level
      short_message = msg_data.short_message&.strip || split_words(name).join(' ').capitalize
      Message.new(
        name: name,
        component: @component,
        number: msg_data.number,
        level: level,
        args: msg_data.args || '',
        short_message: short_message,
        message: msg_data.message&.strip
      )
    end
  end

  ATTRS = %i[component enum_name namespace messages].freeze

  attr_reader(*ATTRS)

  module_function(*ATTRS)
end

def Gen.on_require(data)
  Messages.load_messages(data)
end
