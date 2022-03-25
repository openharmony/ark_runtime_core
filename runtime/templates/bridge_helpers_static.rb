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

def get_insn_type(insn)
  insn.stripped_mnemonic == "initobj" ? "initobj" : "call"
end

def get_format_for(insn)
  fmt = insn.format.pretty
  if fmt == "v4_v4_v4_v4_id16"
    # Merge v4_v4_v4_v4_id16 and v4_v4_id16 since they have the same handling code
    fmt = "v4_v4_id16"
  end
  return "#{get_insn_type(insn)}_#{fmt}"
end

def get_call_insns()
  Panda::instructions.reject(&:prefix).select do |insn|
    ((insn.properties.include?("call") || insn.stripped_mnemonic == "initobj") && !(insn.properties.include? "dynamic"))
  end
end

