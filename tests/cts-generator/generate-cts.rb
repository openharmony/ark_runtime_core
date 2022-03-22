#!/usr/bin/env ruby
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

# frozen_string_literal: true

require 'yaml'
require 'logger'
require 'optparse'
require 'ostruct'

require_relative 'generator/generator'


def check_option(optparser, options, key)
  return if options[key]

  puts "Missing option: --#{key}"
  puts optparser
  exit false
end

options = OpenStruct.new

optparser = OptionParser.new do |opts|
  opts.banner = 'Usage: generate-cts.rb [options]'
  opts.on('-t', '--template FILE', 'Path to template yaml file to generate tests (required)')
  opts.on('-s', '--schema FILE', 'Path to json schema for template yaml (required)')
  opts.on('-k', '--skip', 'Skip yaml schema validation')
  opts.on('-o', '--output DIR', 'Path to directory where tests will be generated (required)')
  opts.on('--skip-header', 'Do not generate test headers')
  opts.on('-h', '--help', 'Prints this help') do
    puts opts
    exit
  end
end

optparser.parse!(into: options)
check_option(optparser, options, 'template')
check_option(optparser, options, 'schema')
check_option(optparser, options, 'output')
template_path=options['template']
schema_path=options['schema']
output=options['output']
skip=options['skip']
skip_header=options['skip-header']

LOG = Logger.new(STDOUT)
LOG.level = Logger::DEBUG

LOG.info "Loading '#{template_path}'"

data = YAML.load_file(template_path)

generator = Generator::Parser.new data, output, File.dirname(template_path), skip_header

# Validate test template

if !skip
  require 'json-schema'
  res = JSON::Validator.fully_validate(schema_path, data)
  unless res.empty?
    puts "Template '#{template_path}' contains several errors:"
    puts res
    raise 'Schema validation error, please update template to match schema to generate tests'
  end
end

generator.parse skip
