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

# Assembler-specific extension of ISAPI
Instruction.class_eval do
  def asm_token
    mnemonic.tr('.', '_').upcase
  end

  def builtin?
    stripped_mnemonic == 'builtin'
  end

  def call?
    properties.include?('call') || stripped_mnemonic == 'initobj'
  end

  def range?
    mnemonic.split('.')[-1] == 'range'
  end

  def simple_call?
    call? && !range?
  end

  def return?
    stripped_mnemonic == 'return'
  end

  def return_obj?
    mnemonic == 'return.obj'
  end

  def return64?
    mnemonic == 'return.64'
  end

  def return32?
    mnemonic == 'return'
  end

  def return_void?
    mnemonic == 'return.void'
  end
end

def bit_cast(what, to_type, from_type)
  "bit_cast<#{to_type}, #{from_type}>(static_cast<#{from_type}>(std::get<double>(#{what})))"
end

def index_of_max(a)
  a.each_with_index.max[1] # returns index of `last` max value
end

def max_number_of_src_regs
  Panda::instructions.map do |insn|
    insn.operands.select(&:reg?).select(&:src?).size
  end.max
end

IR = Struct.new(:opcode, :flags, :dst_idx, :use_idxs)

module Panda
  def self.pseudo_instructions
    insns = []
    insns << IR.new('MOVX',       ['InstFlags::PSEUDO'], 0, [1])
    insns << IR.new('LDAX',       ['InstFlags::PSEUDO', 'InstFlags::ACC_WRITE'], 'INVALID_REG_IDX', [0])
    insns << IR.new('STAX',       ['InstFlags::PSEUDO', 'InstFlags::ACC_READ'], 0, [])
    insns << IR.new('NEWX',       ['InstFlags::PSEUDO'], 0, [])
    insns << IR.new('INITOBJX',   ['InstFlags::PSEUDO', 'InstFlags::CALL', 'InstFlags::ACC_WRITE'], 'INVALID_REG_IDX', [])
    insns << IR.new('CALLX',      ['InstFlags::PSEUDO', 'InstFlags::CALL', 'InstFlags::ACC_WRITE'], 'INVALID_REG_IDX', [])
    insns << IR.new('CALLX_VIRT', ['InstFlags::PSEUDO', 'InstFlags::CALL', 'InstFlags::ACC_WRITE'], 'INVALID_REG_IDX', [])
    insns << IR.new('B_P_CALLIX', ['InstFlags::PSEUDO', 'InstFlags::CALL', 'InstFlags::ACC_READ'], 'INVALID_REG_IDX', [])
    insns << IR.new('B_P_CALLIEX',['InstFlags::PSEUDO', 'InstFlags::CALL', 'InstFlags::ACC_READ'], 'INVALID_REG_IDX', [])
    insns
  end
end

# returns array of OpenStruct with fields
# name - name of variable in emitter code
# type - type of variable in emitter code
def assembler_signature(group, is_jump)
  insn = group.first
  sig = format_ops(insn.format)
  sig.each do |o|
    if o.name.start_with?('imm')
      if insn.asm_token.start_with?('F')
        o.type, o.name = is_jump ? ['const std::string &', 'label'] : ["double", o.name]
      else
        o.type, o.name = is_jump ? ['const std::string &', 'label'] : ["int64_t", o.name]
      end
    elsif o.name.start_with?('id')
      o.type, o.name = ['const std::string &', 'id']
    else
      o.type = "uint16_t"
    end
  end
end
