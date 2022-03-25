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

# PandaFile specific extension of ISAPI

Instruction.class_eval do
  def emitter_name
    mnemonic.split('.').map { |p| p == '64' ? 'Wide' : p.capitalize }.join
  end

  def each_operand
    getters = {:reg? => 0, :imm? => 0, :id? => 0}
    operands.each do |op|
      key = getters.keys.find { |x| op.send(x) }
      yield op, getters[key]
      getters[key] += 1
    end
  end

  def jcmp?
    jump? && conditional? && stripped_mnemonic[-1] != 'z'
  end

  def jcmpz?
    jump? && conditional? && stripped_mnemonic[-1] == 'z'
  end
end

Operand.class_eval do
  def to_h
    instance_variables.map { |v| [v.to_s[1..-1], instance_variable_get(v)] }.to_h
  end
end

def storage_width(bits)
  (bits + 7) / 8 * 8
end

def format_ops(format)
  format.encoding.values
end

# returns array of OpenStruct with fields
# name - name of variable in emitter code
# type - type of variable in emitter code
# width - bit width
# tag - the same as in Operand isapi class
def emitter_signature(group, is_jump)
  sig = format_ops(group.first.format)
  sig.each { |o| o.width = storage_width(o.width) }
  group.map do |insn|
    operands = insn.operands
    operands.each_with_index do |o, i|
      sig[i].width = [o.width, sig[i].width].max
    end
  end
  sig.each do |o|
    if o.name.start_with?('imm')
      o.type, o.name = is_jump ? ['const Label &', 'label'] : ["int#{o.width}_t", o.name]
    else
      o.type = "uint#{o.width}_t"
    end
  end
end

def insns_uniq_sort_fmts
  Panda.instructions.uniq { |i| i.format.pretty }.sort_by { |insn| insn.format.pretty }
end
