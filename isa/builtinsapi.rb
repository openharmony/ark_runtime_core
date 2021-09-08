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

require 'delegate'
require 'ostruct'

class BuiltinInstruction < SimpleDelegator
  def mnemonic
    sig.split(' ')[0]
  end

  def opcode
    mnemonic.sub('.', '_') + self[:format]
  end

  def format
    self[:format].length() == 0 ? 'NONE' : self[:format].sub('_', '')
  end
end

class Builtin < SimpleDelegator
  def mnemonic
    sig.split(' ')[0]
  end

  def opcode
    mnemonic.sub('.', '_') + self[:format]
  end

  def format
    self[:format].length() == 0 ? 'NONE' : self[:format].sub('_', '')
  end

  def stripped_mnemonic
    mnemonic.split('.')[0]
  end

  def throwing?
   throwable = ['monitorenter', 'monitorexit']
   throwable.include?(stripped_mnemonic) || self[:exception] == true
  end

  def ins
    self[:insn].sub('.', '_')
  end

  def id
    self[:id]
  end

  def language_context
    self[:space]
  end
end

module PandaBuiltins
  module_function

  def instructions
    insns = []
    @data["groups"].each do |group|
      group.instructions[0][:format].each do |f|
        props = group.instructions[0].to_h
        props[:format] = f.sub('op', '').gsub(/imm[0-9]+?/, 'imm').gsub(/v[0-9]+?/, 'v').gsub(/_([0-9]+)/, '\1')

        insns << BuiltinInstruction.new(OpenStruct.new(props))
      end
    end
    insns
  end

  def builtins
    all_builtins = []
    @data["builtins"].each do |b|
      b[:format].each do |f|
        props = b.to_h
        props[:format] = f.sub('op', '').gsub(/imm[0-9]+?/, 'imm').gsub(/v[0-9]+?/, 'v').gsub(/_([0-9]+)/, '\1')

        all_builtins << Builtin.new(OpenStruct.new(props))
      end
    end

    # Assign an ID to each builtin.
    # NB! IDs are unique only for builtins corresponding
    # to a *single* 'builtin.*' instruction!
    id = 0
    for i in 1 .. all_builtins.length() - 1
        all_builtins[i - 1][:id] = id
        id += 1

        id = 0 unless all_builtins[i - 1][:insn] == all_builtins[i][:insn]
    end
    all_builtins[all_builtins.length() - 1][:id] = id if all_builtins.any?

    all_builtins
  end

  # Delegating part of module
  #
  def wrap_data(data)
    @data = data
  end

end

def prepare_data(data)
  insn_ids = []
  mnemos   = {} # insn id => mnemonics
  formats  = {} # insn id => supported encoding formats
  counts   = {} # insn id => number of corresponding builtins (counting different formats)
  ordered  = {} # insn id => list of corresponding builtins in order of definition

  raise 'Number of builtin.* instructions is too high' if data["groups"].length() > 20

  insn_names = {}
  data["groups"].each do |group|

    raise 'Expecting exactly one instruction per builtin.* group' unless group["instructions"].length() == 1

    group["instructions"].each do |insn|

      sig = insn["sig"]
      args = sig.split(/,?\s+/)
      args_len = args.length()

      raise 'Expecting at least one argument for a builtin' if args_len < 2
      raise "Expecting 'builtin.' prefix for the mnemonic" unless /^builtin\./.match(args[0])

      raise "Expecting an imm as the builtin's first argument" unless args[1].start_with?('imm')

      raise "Duplicate builtin instruction name '#{args[0]}'" if insn_names.key?(args[0])
      insn_names[args[0]] = 1

      insn_id = args[2 .. args_len - 1].map{ |arg| arg =~ /^([^:]+):?/; $1 }.join('_')

      raise "Non-unique signature is detected while parsing instruction '#{sig}'" if formats.key?(insn_id)

      # Currently a single format per instruction is enough, so the assert below just guards status quo
      raise 'Expecting exactly one encoding format per instruction' unless insn["format"].length() == 1

      insn_ids << insn_id
      mnemos[insn_id] = args[0]
      formats[insn_id] = insn["format"]
      counts[insn_id]  = 0
      ordered[insn_id] = []
    end
  end

  builtin_names = {}
  data["builtins"].each do |builtin|
    raise "'sig' property is missing" unless builtin['sig']
    raise "'acc' property is missing" unless builtin['acc']
    raise "Unexpected 'insn' property" if builtin['insn']
    raise "Unexpected 'format' property" if builtin['format']

    sig = builtin["sig"]
    args = sig.split(/,?\s+/)
    args_len = args.length()

    raise "Malformed signature of a builtin" if args_len < 1

    raise "Duplicate builtin name '#{args[0]}'" if builtin_names.key?(args[0])
    builtin_names[args[0]] = 1

    insn_id = args[1 .. args_len - 1].map{ |arg| arg =~ /^([^:]+):?/; $1 }.join('_')

    raise "Signature for builtin '#{sig}' does not match any builtin.* instruction" unless formats.key?(insn_id)

    builtin["insn"]    = mnemos[insn_id]
    builtin["format"]  = formats[insn_id].clone
    counts[insn_id]   += builtin["format"].length()
    ordered[insn_id]  << builtin

    raise "Number of builtins for '#{insn_id}' exceeds builtin_id" if counts[insn_id] > 256
  end

  # Enforce order of builtins and add "delta" property, defined as follows:
  # builtin1.delta = 0
  # builtin2.delta = num_builtins_with_sig_of(builtin1)
  # builtin3.delta = num_builtins_with_sig_of(builtin1) + num_builtins_with_sig_of(builtin2)
  # ...
  # builtinN.delta = num_builtins_with_sig_of(builtin1) + num_builtins_with_sig_of(builtin2) + ... + num_builtins_with_sig_of(builtinN_1)
  builtins = []
  for i in 0 .. data["groups"].length() - 1
    curr_insn    = data["groups"][i]["instructions"][0]
    curr_insn_id = insn_ids[i]

    if i > 0
        prev_insn    = data["groups"][i - 1]["instructions"][0]
        prev_insn_id = insn_ids[i - 1]

        curr_insn["delta"] = prev_insn["delta"] + counts[prev_insn_id]
    else
        curr_insn["delta"] = 0
    end

    ordered[curr_insn_id].each do |builtin|
        builtins << builtin
    end
  end

  {"groups" => data["groups"], "builtins" => builtins}
end

module ExtBuiltins
  def self.load_ecma_builtins(data); end
end

def Gen.on_require(data)
  ExtBuiltins.load_ecma_builtins(data)
  prepared_data = prepare_data(data)
  PandaBuiltins.wrap_data(prepared_data)
end

