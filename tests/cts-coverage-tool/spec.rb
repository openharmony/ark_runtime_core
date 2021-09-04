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

# frozen_string_literal: true

require 'pathname'

class Spec
  attr_reader :spec, :orphaned

  def initialize(spec, non_testable, testdir)
    @spec = spec
    @orphaned = []
    @non_testable = non_testable
    @testdir = Pathname.new(testdir)
    prepare_spec
    prepare_non_testable
  end

  def add_testfile(testfile)
    proc_testfile(testfile)
  rescue StandardError => e
    @orphaned << { 'file' => rel_path(testfile), 'error' => e, 'comment' => '' }
  end

  def compute_coverage
    result = { 'tests' => compute_tests, 'assertions' => compute_assertions, 'coverage_by_groups' => [] }
    result['coverage_metric'] = (result['assertions']['covered'].to_f / result['assertions']['testable']).round(2)
    @spec['groups'].each do |g|
      result['coverage_by_groups'] << { 'title' => g['title'], 'coverage_metric' => g['coverage_metric'] }
    end
    result
  end

  def uncovered
    groups = []
    @spec['groups'].each do |g|
      not_covered_i = g['instructions'].reject { |i| i['tests'].length.positive? || i['non_testable'] }.map { |i| except(i, %w[tests non_testable]) }
      not_covered_d = g['description_tests'].reject { |d| d['tests'].length.positive? || d['non_testable'] }
      not_covered_e = g['exceptions_tests'].reject { |d| d['tests'].length.positive? || d['non_testable'] }
      not_covered_v = g['verification_tests'].reject { |d| d['tests'].length.positive? || d['non_testable'] }
      next unless not_covered_i.any? || not_covered_d.any? || not_covered_e.any? || not_covered_v.any?

      result = { 'title' => g['title'] }
      if not_covered_d.any?
        result['description'] = not_covered_d.map { |d| d['assertion'] }.join('. ').gsub(/\n/, ' ').rstrip
      end
      result['instructions'] = not_covered_i if not_covered_i.any?
      result['exceptions'] = not_covered_e.map { |e| e['exception'] } if not_covered_e.any?
      result['verification'] = not_covered_v.map { |v| v['verification'] } if not_covered_v.any?
      groups << result
    end
    { 'groups' => groups }
  end

  private

  def rel_path(testfile)
    Pathname.new(testfile).relative_path_from(@testdir).to_s
  end

  def prepare_non_testable
    @non_testable['groups']&.each do |ntg|
      spec_group = @spec['groups'].find { |sg| sg['title'] == ntg['title'] }
      next if spec_group.nil?

      ntg['description'] && split(ntg['description']).map do |ntda|
        spec_description = spec_group['description_tests'].find { |sd| same?(sd['assertion'], ntda) }
        next if spec_description.nil?

        spec_description['non_testable'] = true
      end

      ntg['instructions']&.each do |nti|
        spec_instruction = spec_group['instructions'].find { |si| except(si, %w[tests non_testable]) == nti }
        next if spec_instruction.nil?

        spec_instruction['non_testable'] = true
      end

      ntg['exceptions']&.each do |nte|
        spec_exception = spec_group['exceptions_tests'].find { |se| se['exception'] == nte }
        next if spec_exception.nil?

        spec_exception['non_testable'] = true
      end

      ntg['verification']&.each do |ntv|
        spec_verification = spec_group['verification_tests'].find { |sv| sv['verification'] == ntv }
        next if spec_verification.nil?

        spec_verification['non_testable'] = true
      end
    end
  end

  def prepare_spec
    @spec['groups'].each do |g|
      g['instructions'].each do |i|
        i['tests'] = []
        i['non_testable'] = false
      end
      g['description_tests'] = split(g['description']).map do |da|
        { 'assertion' => da, 'tests' => [], 'non_testable' => false }
      end
      g['exceptions_tests'] = g['exceptions'].map { |e| { 'exception' => e, 'tests' => [], 'non_testable' => false } }
      g['verification_tests'] = g['verification'].map { |v| { 'verification' => v, 'tests' => [], 'non_testable' => false } }
    end
  end

  def split(description)
    result = []
    small = false
    description.split(/\./).each do |p|
      if small
        result[-1] += '.' + p
        small = false if p.length > 5
      elsif p.length > 5
        result << p.lstrip
      else
        if result.length.zero?
          result << p.lstrip
        else
          result[-1] += '.' + p
        end
        small = true
      end
    end
    result
  end

  def same?(str1, str2)
    str1.tr('^A-Za-z0-9', '').downcase == str2.tr('^A-Za-z0-9', '').downcase
  end

  def proc_testfile(testfile)
    raw_data = read_test_data(testfile)
    tdata = YAML.safe_load(raw_data)
    if tdata.class != array
      @orphaned << { 'file' => rel_path(testfile), 'error' => 'Bad format, expected array of titles', 'comment' => raw_data }
      return
    end

    tdata.each do |tg|
      group = @spec['groups'].find { |x| x['title'] == tg['title'] }
      if group.nil?
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Group with given title not found in the spec', 'comment' => tg }
        next
      end

      proc_test_instructions(tg, group, testfile)
      proc_test_description(tg, group, testfile)
      proc_test_exceptions(tg, group, testfile)
      proc_test_verification(tg, group, testfile)
    end
  end

  def proc_test_instructions(testgroup, specgroup, testfile)
    testgroup['instructions']&.each do |ti|
      gi = specgroup['instructions'].find { |x| except(x, %w[tests non_testable]) == ti }
      if gi.nil?
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given instruction not found in the spec', 'comment' => ti }
        next
      end
      if gi['non_testable']
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given instruction is non-testable', 'comment' => ti }
        next
      end
      gi['tests'] << rel_path(testfile)
    end
  end

  def proc_test_description(testgroup, specgroup, testfile)
    testgroup['description'] && split(testgroup['description']).map do |tda|
      sd = specgroup['description_tests']&.find { |sda| same?(sda['assertion'], tda) }
      if sd.nil?
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given description assertion not found in the spec', 'comment' => tda }
        next
      end
      if sd['non_testable']
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given description is non-testable', 'comment' => tda }
        next
      end
      sd['tests'] << rel_path(testfile)
    end
  end

  def proc_test_exceptions(testgroup, specgroup, testfile)
    testgroup['exceptions']&.each do |te|
      se = specgroup['exceptions_tests'].find { |x| x['exception'] == te }
      if se.nil?
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given exception assertion not found in the spec', 'comment' => te }
        next
      end
      if se['non_testable']
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given exception assertion is non-testable', 'comment' => te }
        next
      end
      se['tests'] << rel_path(testfile)
    end
  end

  def proc_test_verification(testgroup, specgroup, testfile)
    testgroup['verification']&.each do |tv|
      sv = specgroup['verification_tests'].find { |x| x['verification'] == tv }
      if sv.nil?
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given verification assertion not found in the spec', 'comment' => tv }
        next
      end
      if sv['non_testable']
        @orphaned << { 'file' => rel_path(testfile), 'error' => 'Given verification assertion is non-testable', 'comment' => tv }
        next
      end
      sv['tests'] << rel_path(testfile)
    end
  end

  def read_test_data(filename)
    lines_array = []
    started = false
    File.readlines(filename).each do |line|
      if started
        break if line[0] != '#'

        lines_array << line[1..-1]
      else
        started = true if line[0..3] == '#---'
      end
    end
    lines_array.join("\n")
  end

  def except(hash, keys)
    hash.dup.tap do |x|
      keys.each { |key| x.delete(key) }
    end
  end

  def compute_tests
    # Count mismatched tests
    orphaned = @orphaned.map { |x| x['file'] }.uniq

    # Count accepted tests
    accepted = []
    @spec['groups'].each { |g| g['instructions']&.each { |i| accepted += i['tests'] } }
    @spec['groups'].each { |g| g['description_tests']&.each { |i| accepted += i['tests'] } }
    accepted = accepted.uniq

    { 'counted_for_coverage' => accepted.length, 'orphaned' => orphaned.length, 'total' => (orphaned + accepted).uniq.length }
  end

  def compute_assertions
    res = { 'testable' => 0, 'non_testable' => 0, 'covered' => 0, 'not_covered' => 0 }

    @spec['groups'].each do |g|
      g_testable = 0
      g_non_testable = 0
      g_covered = 0
      g_not_covered = 0

      %w[instructions description_tests exceptions_tests verification_tests].each do |k|
        g[k].each do |i|
          if i['non_testable']
            g_non_testable += 1
          else
            g_testable += 1
            i['tests'].length.positive? ? g_covered += 1 : g_not_covered += 1
          end
        end
      end

      if g_testable > 0
        g['coverage_metric'] = (g_covered.to_f / g_testable).round(2)
      else
        g['coverage_metric'] = 'Not testable'
      end

      res['testable'] += g_testable
      res['non_testable'] += g_non_testable
      res['covered'] += g_covered
      res['not_covered'] += g_not_covered
    end

    res
  end
end
