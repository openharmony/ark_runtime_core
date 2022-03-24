#!/usr/bin/env ruby
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

require 'optparse'
require 'yaml'
require 'json'
require 'erb'

# Extend Module to implement a decorator for ISAPI methods
class Module
  def cached(method_name)
    noncached_method = instance_method(method_name)
    define_method(method_name) do
      unless instance_variable_defined? "@#{method_name}"
        instance_variable_set("@#{method_name}", noncached_method.bind(self).call)
      end
      instance_variable_get("@#{method_name}")
    end
  end
end

# Gen.on_require will be called after requiring scripts.
# May (or even should) be redefined in scripts.
module Gen
  def self.on_require(data); end
end

def create_sandbox
  # nothing but Ruby core libs and 'required' files
  binding
end

def check_option(optparser, options, key)
  return if options[key]

  puts "Missing option: --#{key}"
  puts optparser
  exit false
end

def check_version
  major, minor, = RUBY_VERSION.split('.').map(&:to_i)
  major > 2 || (major == 2 && minor >= 5)
end

raise "Update your ruby version, #{RUBY_VERSION} is not supported" unless check_version

options = OpenStruct.new

optparser = OptionParser.new do |opts|
  opts.banner = 'Usage: gen.rb [options]'

  opts.on('-t', '--template FILE', 'Template for file generation (required)')
  opts.on('-d', '--data FILE', 'Source data in YAML format (required)')
  opts.on('-o', '--output FILE', 'Output file (default is stdout)')
  opts.on('-a', '--assert FILE', 'Go through assertions on data provided and exit')
  opts.on('-r', '--require foo,bar,baz', Array, 'List of files to be required for generation')

  opts.on('-h', '--help', 'Prints this help') do
    puts opts
    exit
  end
end
optparser.parse!(into: options)

check_option(optparser, options, :data)
data = YAML.load_file(File.expand_path(options.data))
data = JSON.parse(data.to_json, object_class: OpenStruct)

options&.require&.each { |r| require File.expand_path(r) } if options.require
Gen.on_require(data)

if options.assert
  require options.assert
  exit
end

check_option(optparser, options, :template)
template = File.read(File.expand_path(options.template))
output = options.output ? File.open(File.expand_path(options.output), 'w') : STDOUT
t = ERB.new(template, nil, '%-')
t.filename = options.template
output.write(t.result(create_sandbox))
output.close
