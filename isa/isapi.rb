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
require 'ostruct'
require 'digest'
require 'set'

module Enumerable
  def stable_sort_by
    sort_by.with_index { |x, idx| [yield(x), idx] }
  end
end

module Util
  module_function

  def parse_acc_signature(sig)
    res = []
    if sig.include?('->')
      src_type, dst_type = sig.match(/inout:(\w+)->(\w+)/).captures
      res << Operand.new('acc', 'out', dst_type)
      res << Operand.new('acc', 'in', src_type)
    elsif sig.include?(':')
      srcdst, type = sig.match(/(\w+):(\w+)/).captures
      if srcdst == 'inout'
        res << Operand.new('acc', 'out', type)
        res << Operand.new('acc', 'in', type)
      else
        res << Operand.new('acc', srcdst, type)
      end
    elsif sig != 'none'
      raise "Unexpected accumulator signature: #{sig}"
    end
    res
  end

  def parse_operand_signature(sig)
    operand_parts = sig.split(':')
    case operand_parts.size
    when 1
      name = operand_parts[0]
      srcdst = :in
      type = 'none'
    when 2
      name, type = operand_parts
      srcdst = :in
    when 3
      name, srcdst, type = operand_parts
    else
      raise "Unexpected operand signature: #{sig}"
    end
    [name, srcdst, type]
  end
end

# Methods for YAML instructions
# 'Instruction' instances are created for every format of every isa.yaml
# instruction and inherit properties of its instruction group.
#
class Instruction < SimpleDelegator
  # Signature without operands
  def mnemonic
    sig.split(' ')[0]
  end

  # Mnemonic stripped from type info
  def stripped_mnemonic
    mnemonic.split('.')[0]
  end

  # Unique (and not very long) identifier for all instructions
  def opcode
    mn = mnemonic.tr('.', '_')
    fmt = format.pretty
    if fmt == 'none'
      mn
    else
      "#{mn}_#{fmt}"
    end
  end

  def prefix
    name = dig(:prefix)
    Panda.prefixes_hash[name] if name
  end

  # Suggested handler name
  def handler_name
    opcode.upcase
  end

  def opcode_idx
    if prefix
      dig(:opcode_idx) << 8 | prefix.opcode_idx
    else
      dig(:opcode_idx)
    end
  end

  # Format instance for raw-data format name
  def format
    Panda.format_hash[dig(:format)]
  end

  # Array of explicit operands
  def operands
    return [] unless sig.include? ' '

    _, operands = sig.match(/(\S+) (.+)/).captures
    operands = operands.split(', ')
    ops_encoding = format.encoding

    operands.map do |operand|
      name, srcdst, type = Util.parse_operand_signature(operand)
      key = name
      key = 'id' if name.end_with?('id')
      Operand.new(name, srcdst, type, ops_encoding[key].width, ops_encoding[key].offset)
    end
  end

  # Used by compiler
  # Operands array preceded with accumulator as if it was a regular operand
  # Registers that are both destination and source are uncoupled
  cached def acc_and_operands
    res = Util.parse_acc_signature(acc)
    operands.each_with_object(res) do |op, ops|
      if op.dst? && op.src?
        ops << Operand.new(op.name, 'out', op.type, op.width, op.offset)
        ops << Operand.new(op.name, 'in', op.type, op.width, op.offset)
      else
        ops << op
      end
    end
  end

  def type(index)
    acc_and_operands.select(&:src?)[index].type || 'none'
  end

  # Type of single destination operand ("none" if there are no such)
  def dtype
    acc_and_operands.select(&:dst?).first&.type || 'none'
  end

  # Shortcut for querying 'float' property
  def float?
    properties.include? 'float'
  end

  # Shortcut for querying 'jump' property
  def jump?
    properties.include? 'jump'
  end

  # Shortcut for querying 'conditional' property
  def conditional?
    properties.include? 'conditional'
  end

  # Shortcut for querying 'x_none' exception
  def throwing?
    !exceptions.include? 'x_none'
  end

  def builtin?
    /^builtin\./.match(mnemonic)
  end

  # Size of source operand
  cached def op_size
    type[1..-1].to_i
  end

  # Size of destination operand
  cached def dest_op_size
    dtype[1..-1].to_i
  end

  def namespace
    dig(:namespace) || "core"
  end
end

class Prefix < SimpleDelegator
  # Suggested handler name
  def handler_name
    name.upcase
  end
end

# Dummy class for invalid handlers
class Invalid
  def handler_name
    'INVALID'
  end
end

# Methods over format names
#
class Format
  attr_reader :name

  def initialize(name)
    @name = name
  end

  cached def pretty
    pretty_helper.gsub(/imm[0-9]?/, 'imm').gsub(/v[0-9]?/, 'v').gsub(/_([0-9]+)/, '\1')
  end

  def prefixed?
    name.start_with?('pref_')
  end

  cached def size
    bits = pretty.gsub(/[a-z]/, '').split('_').map(&:to_i).sum
    raise "Incorrect format name #{name}" if bits % 8 != 0

    opcode_bytes = prefixed? ? 2 : 1
    bits / 8 + opcode_bytes
  end

  cached def encoding
    return {} if name.end_with?('_none')

    offset = prefixed? ? 16 : 8
    encoding = {}
    encoding.default_proc = proc { |_, k| raise KeyError, "#{k} not found" }
    name.sub('pref_', '').sub('op_', '').split('_').each_slice(2).map do |name, width|
      op = OpenStruct.new
      op.name = name
      op.width = width.to_i
      op.offset = offset
      offset += op.width
      encoding[name] = op
    end
    encoding
  end

  private

  # pretty but with dst/src info
  cached def pretty_helper
    name.sub('op_', '')
  end
end

# Operand types and encoding
#
class Operand
  attr_reader :name, :type, :offset, :width

  def initialize(name, srcdst, type, width = 0, offset = 0)
    @name = name.to_s.gsub(/[0-9]/, '').to_sym
    unless %i[v acc imm method_id type_id field_id string_id literalarray_id].include?(@name)
      raise "Incorrect operand #{name}"
    end

    @srcdst = srcdst.to_sym || :in
    types = %i[none u1 u2 i8 u8 i16 u16 i32 u32 b32 f32 i64 u64 b64 f64 ref top any]
    raise "Incorrect type #{type}" unless types.include?(type.sub('[]', '').to_sym)

    @type = type
    @width = width
    @offset = offset
  end

  def reg?
    @name == :v
  end

  def acc?
    @name == :acc
  end

  def imm?
    @name == :imm
  end

  def id?
    %i[method_id type_id field_id string_id literalarray_id].include?(@name)
  end

  def dst?
    %i[inout out].include?(@srcdst)
  end

  def src?
    %i[inout in].include?(@srcdst)
  end

  def size
    @type[1..-1].to_i
  end
end

# Helper class for generating dispatch tables
class DispatchTable
  # Canonical order of dispatch table consisting of
  # * non-prefixed instructions handlers
  # * invalid handlers
  # * prefix handlers that re-dispatch to prefixed instruction based on second byte of opcode_idx
  # * prefixed instructions handlers, in the order of prefixes
  # Return array with proposed handler names
  def handler_names
    handlers = Panda.instructions.reject(&:prefix) +
               Array.new(invalid_non_prefixed_interval.size, Invalid.new) +
               Panda.prefixes.select(&:public?) +
               Array.new(invalid_prefixes_interval.size, Invalid.new) +
               Panda.prefixes.reject(&:public?) +
               Panda.instructions.select(&:prefix).stable_sort_by { |i| Panda.prefixes_hash[i.prefix.name].opcode_idx }

    handlers.map(&:handler_name)
  end

  def invalid_non_prefixed_interval
    (Panda.instructions.reject(&:prefix).map(&:opcode_idx).max + 1)..(Panda.prefixes.map(&:opcode_idx).min - 1)
  end

  def invalid_prefixes_interval
    max_invalid_idx = Panda.prefixes.reject(&:public?).map(&:opcode_idx).min || 256
    (Panda.prefixes.select(&:public?).map(&:opcode_idx).max + 1)..(max_invalid_idx - 1)
  end

  # Maximum value for secondary dispatch index for given prefix name
  def secondary_opcode_bound(prefix)
    @prefix_data ||= prefix_data
    @prefix_data[prefix.name][:number_of_insns] - 1
  end

  # Offset in dispatch table for handlers of instructions for given prefix name
  def secondary_opcode_offset(prefix)
    @prefix_data ||= prefix_data
    256 + @prefix_data[prefix.name][:delta]
  end

  private

  def prefix_data
    cur_delta = 0
    Panda.prefixes.each_with_object({}) do |p, obj|
      prefix_instructions_num = Panda.instructions.select { |i| i.prefix && (i.prefix.name == p.name) }.size
      obj[p.name] = { delta: cur_delta, number_of_insns: prefix_instructions_num }
      cur_delta += prefix_instructions_num
    end
  end
end

# Auxilary classes for opcode assignment
class OpcodeAssigner
  def initialize
    @table = Hash.new { |h, k| h[k] = Set.new }
    @all_opcodes = Set.new(0..255)
  end

  def consume(item)
    raise 'Cannot consume instruction without opcode' unless item.opcode_idx

    @table[prefix(item)] << item.opcode_idx
  end

  def yield_opcode(item)
    return item.opcode_idx if item.opcode_idx

    opcodes = @table[prefix(item)]
    choose_opcode(opcodes)
  end

  private

  def choose_opcode(occupied_opcodes)
    (@all_opcodes - occupied_opcodes).min
  end

  def prefix(item)
    item.prefix.nil? ? 'non_prefixed' : item.prefix
  end
end

class PrefixOpcodeAssigner < OpcodeAssigner
  private

  # override opcodes assignment for prefixes
  def choose_opcode(occupied_opcodes)
    (@all_opcodes - occupied_opcodes).max
  end
end

# A bunch of handy methods for template generating
#
# All yaml properties are accessible by '.' syntax,
# e.g. 'Panda::groups[0].instruction[0].format'
#
module Panda
  module_function

  # Hash with exception tag as a key and exception description as a value
  cached def exceptions_hash
    convert_to_hash(exceptions)
  end

  # Hash with property tag as a key and property description as a value
  cached def properties_hash
    convert_to_hash(properties)
  end

  # Hash with verification tag as a key and verification description as a value
  cached def verification_hash
    convert_to_hash(verification)
  end

  cached def prefixes_hash
    hash = prefixes.map { |p| [p.name, p] }.to_h
    hash.default_proc = proc { |_, k| raise KeyError, "#{k} not found" }
    hash
  end

  # Hash from format names to Format instances
  cached def format_hash
    each_data_instruction.with_object([]) do |instruction, fmts|
      fmt_name = instruction.format
      fmts << [fmt_name, Format.new(fmt_name)]
    end.to_h
  end

  # Array of Instruction instances for every possible instruction
  def instructions
    unless defined? @instructions
      opcodes = OpcodeAssigner.new
      tmp_public = initialize_instructions(opcodes) { |ins| !ins.opcode_idx.nil? }
      tmp_private = initialize_instructions(opcodes) { |ins| ins.opcode_idx.nil? }
      tmp = tmp_public + tmp_private
      @instructions = tmp.sort_by(&:opcode_idx)
    end
    @instructions
  end

  def prefixes
    unless defined? @prefixes
      opcodes = PrefixOpcodeAssigner.new
      tmp_public = initialize_prefixes(opcodes) { |p| !p.opcode_idx.nil? }
      tmp_private = initialize_prefixes(opcodes) { |p| p.opcode_idx.nil? }
      tmp = tmp_public + tmp_private
      @prefixes = tmp.sort_by(&:opcode_idx)
    end
    @prefixes
  end

  cached def dispatch_table
    @dispatch_table ||= DispatchTable.new
  end

  # Array of all Format instances
  cached def formats
    format_hash.values.uniq(&:pretty).sort_by(&:pretty)
  end

  # delegating part of module
  #
  def wrap_data(data)
    @data = data
  end

  def respond_to_missing?(method_name, include_private = false)
    @data.respond_to?(method_name, include_private)
  end

  def method_missing(method, *args, &block)
    if respond_to_missing? method
      @data.send(method, *args, &block)
    else
      super
    end
  end

  # private functions
  #
  private_class_method def convert_to_hash(arr)
    hash = arr.map { |i| [i.tag, i.description] }.to_h
    hash.default_proc = proc { |_, k| raise KeyError, "#{k} not found" }
    hash
  end

  private_class_method def each_data_instruction
    # create separate instance for every instruction format and inherit group properties
    @each_data_instruction ||= groups.each_with_object([]) do |g, obj|
      g.instructions.each do |i|
        props = g.to_h
        props.delete(:instructions)
        data_insn = props.merge(i.to_h) # instruction may override group props
        if data_insn[:opcode_idx] && (data_insn[:opcode_idx].size != data_insn[:format].size)
          raise 'format and opcode_idx arrays should have equal size'
        end

        data_insn[:format].each_with_index do |f, idx|
          insn = data_insn.dup
          insn[:format] = f
          insn[:opcode_idx] = data_insn[:opcode_idx][idx] if data_insn[:opcode_idx]
          obj << OpenStruct.new(insn)
        end
      end
    end.to_enum
  end

  private_class_method def initialize_instructions(opcodes, &block)
    each_data_instruction.select(&block).each_with_object([]) do |instruction, insns|
      instruction[:public?] = !instruction.opcode_idx.nil?
      instruction.opcode_idx = opcodes.yield_opcode(instruction)
      opcodes.consume(instruction)
      insns << Instruction.new(instruction)
    end
  end

  private_class_method def initialize_prefixes(opcodes, &block)
    dig(:prefixes).select(&block).each_with_object([]) do |p, res|
      p[:public?] = !p.opcode_idx.nil?
      p.opcode_idx = opcodes.yield_opcode(p)
      opcodes.consume(p)
      res << Prefix.new(p)
    end
  end
end

def Gen.on_require(data)
  Panda.wrap_data(data)
end
