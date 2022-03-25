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

def get_format_for(insn)
  fmt = insn.format.pretty
  if fmt == "imm4_v4_v4_v4_v4_v4"
    # Merge imm4_v4_v4_v4_v4_v4 and imm4_v4_v4_v4 since they haave the same handling code
    fmt = "imm4_v4_v4_v4"
  end
  return "call_#{fmt}"
end

def get_call_insns()
  insns = Panda::instructions.select {|insn| (insn.properties.include? "call" and insn.properties.include? "dynamic")}
  # The following instructions are js specific call instructions
  insns = insns.concat(Panda::instructions.select {|insn|
    insn.mnemonic == "builtin.bin2" ||
    insn.mnemonic == "builtin.tern3" ||
    insn.mnemonic == "builtin.quatern4" ||
    insn.mnemonic == "builtin.quin5" ||
    insn.mnemonic == "builtin.r2i"
  })
  return insns
end
