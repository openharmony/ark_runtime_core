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

def assert(name)
  loc = caller_locations(1, 1).first
  raise "#{loc.path}:#{loc.lineno}: '#{name}' assertion failed" unless yield
end

module Enumerable
  def sorted?
    each_cons(2).all? { |a, b| (a <=> b) <= 0 }
  end

  def sorted_by?(&block)
    map(&block).sorted?
  end

  def uniq?
    uniq.size == size
  end
end

assert('Unique opcodes') { Panda.instructions.map(&:opcode).uniq? }

assert('Non-prefixed instruction opcodes and prefixes should fit one byte') do
  Panda.instructions.reject(&:prefix).size + Panda.prefixes.size <= 256
end

assert('Non-prefixed instruction opcode indexes are sorted') do
  Panda.instructions.reject(&:prefix).sorted_by?(&:opcode_idx)
end

assert('Prefix opcode indexes are sorted') do
  Panda.prefixes.sorted_by?(&:opcode_idx)
end

assert('All instructions for a prefix should fit one byte') do
  Panda.prefixes.map do |prefix|
    Panda.instructions.select { |insn| insn.prefix && (insn.prefix.name == prefix.name) }.size <= 256
  end.all?
end

assert('Prefixed instruction should have some prefix specified') do
  Panda.instructions.map do |insn|
    insn.format.prefixed? != insn.prefix.nil?
  end.all?
end

assert('Prefix should be defined') do
  Panda.instructions.map do |insn|
    next true unless insn.prefix

    Panda.prefixes.map(&:name).include?(insn.prefix.name)
  end.all?
end

assert('All prefixes should have unique name') do
  Panda.prefixes.map(&:name).uniq?
end

assert('There should be non-zero gap between non-prefixed and prefixes') do
  !Panda.dispatch_table.invalid_non_prefixed_interval.to_a.empty?
end

assert('There should be non-zero gap between public and private prefixes') do
  !Panda.dispatch_table.invalid_prefixes_interval.to_a.empty?
end

assert('All tags are unique between categories') do
  %i[verification exceptions properties].flat_map { |type| Panda.send(type).map(&:tag) }.uniq?
end

assert('All tags are used') do
  %i[verification exceptions properties].map do |type|
    uses = Panda.instructions.flat_map(&type.to_proc).uniq
    defs = Panda.send(type).map(&:tag)
    (defs - uses).size
  end.reduce(:+).zero?
end

assert('All tags are defined') do
  %i[verification exceptions properties].map do |type|
    uses = Panda.instructions.flat_map(&type.to_proc).uniq
    defs = Panda.send(type).map(&:tag)
    (uses - defs).size
  end.reduce(:+).zero?
end

assert('Format operands are parseable') { Panda.instructions.each(&:operands) }

assert('Verification, exceptions and properties are not empty for every instruction group') do
  %i[verification exceptions properties].map do |type|
    !Panda.groups.map(&type).empty?
  end.all?
end

assert('Mnemonic defines operand types') do
  Panda.instructions.group_by(&:mnemonic).map do |_, insns|
    insns.map { |insn| insn.operands.map(&:name) }.uniq.one?
  end.all?
end

assert('Dtype should be none when bytecode doesn\'t write into accumulator or registers') do
  Panda.instructions.map do |i|
    next true if i.properties.include?('language')

    i.acc_and_operands.map(&:dst?).any? == (i.dtype != 'none')
  end.all?
end

assert('Instruction::float? should play well with operand types') do
  Panda.instructions.map do |i|
    i.float? == i.acc_and_operands.any? { |op| op.type.start_with?('f') }
  end.all?
end

assert('Conditionals should be jumps') do # At least currently
  Panda.instructions.select(&:conditional?).map(&:jump?).all?
end

assert('Acc_none should not be specified along with other accumulator properties') do
  Panda.instructions.map do |i|
    props = i.properties
    # print "23333333, #{i.mnemonic}"
    props.include?('acc_none') == !(props.include?('acc_read') || props.include?('acc_write'))
  end.all?
end

assert('All calls write into accumulator') do
  Panda.instructions.select { |i| i.properties.include?('call') }.map do |i|
    i.properties.include?('acc_write')
  end.all?
end

assert('Calls should be non-prefixed') do # otherwise support in interpreter-to-compiler bridges
  Panda.instructions.select do |i|
    i.properties.include?('call') && !i.mnemonic.include?('polymorphic')
  end.select(&:prefix).empty?
end

assert('Jumps differ from other control-flow') do # At least currently
  Panda.instructions.select { |i| i.mnemonic.match?(/^(throw|call|return)/) }.map do |i|
    !i.jump?
  end.all?
end

assert('Conversions should correspond to source and destination type') do
  Panda.instructions.map do |i|
    match = i.mnemonic.match(/[ifu](\d+)to[ifu](\d+)/)
    next true unless match

    ssize, dsize = match.captures
    i.acc_and_operands.select(&:src?).first.type[1..-1].to_i >= ssize.to_i && i.dtype[1..-1].to_i >= dsize.to_i
  end.all?
end

assert('Operand type should be one of none, ref, u1, u2, i8, u8, i16, u16, i32, u32, b32, i64, u64, b64, f64, top, any') do
  types = %w[none ref u1 u2 i8 u8 i16 u16 i32 u32 b32 f32 i64 u64 b64 f64 top any]
  Panda.instructions.map do |i|
    i.acc_and_operands.all? { |op| types.include?(op.type.sub('[]', '')) }
  end.all?
end

assert('Instruction should have not more than one destination') do
  # Otherwise support it in Assembler IR
  Panda.instructions.map do |i|
    i.acc_and_operands.select(&:dst?).count <= 1
  end.all?
end

assert('Instruction should have not more than one ID operand') do
  # Otherwise support it in Assembler IR
  Panda.instructions.map do |i|
    i.operands.select(&:id?).count <= 1
  end.all?
end

assert('Register encoding width should be the same in instruction') do
  # Otherwise support it in Assembler
  Panda.instructions.map do |i|
    registers = i.operands.select(&:reg?)
    registers.empty? || registers.map(&:width).uniq.one?
  end.all?
end

assert('Calls should have call property') do
  Panda.instructions.map do |i|
    next true unless i.mnemonic.include?('call')

    i.properties.include?('call')
  end.all?
end

assert('Virtual calls should have call_virt property') do
  Panda.instructions.map do |i|
    next true unless i.mnemonic.include?('call.virt')

    i.properties.include?('call_virt')
  end.all?
end
