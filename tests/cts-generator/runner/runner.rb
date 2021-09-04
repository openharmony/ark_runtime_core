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

module TestRunner

  ERROR_NODATA = 86
  ERROR_CANNOT_CREATE_PROCESS = 87
  ERROR_TIMEOUT = 88

  def self.log(level, *args)
    puts args if level <= $VERBOSITY
  end

  def self.print_exception(exception)
    puts "Exception: exception class   : #{exception.class}"
    puts "           exception message : #{exception.message}"
    exception.backtrace.each do |t|
      puts "                       trace : #{t}"
    end
  end

  def self.split_separated_by_colon(string)
    string.split(':')
                   .drop(1)
                   .flat_map { |s| s.split(',').map(&:strip) }
  end

  class CommandRunner
    def initialize(command, reporter)
      @command = command
      @reporter = reporter
    end

    def dump_output(t, output_err, output)
      start = Time.now

      while (Time.now - start) <= $TIMEOUT and t.alive?
        Kernel.select([output_err], nil, nil, 1)
        begin
          output << output_err.read_nonblock(256)
        rescue IO::WaitReadable
        rescue EOFError
          return true
        end
      end
      !t.alive?
    end

    def start_process_timeout
      begin
        input, output_err, t = if $enable_core
          Open3.popen2e(@command, :pgroup => true)
        else
          Open3.popen2e(@command, :pgroup => true, :rlimit_core => 0)
        end
        pid = t[:pid]
        output = ""
        finished = dump_output t, output_err, output

        input.close
        output_err.close

        if !finished
          output << "Process hangs for #{$TIMEOUT}s '#{@command}'\n" \
                    "Killing pid:#{pid}"
          begin
            Process.kill('-TERM', Process.getpgid(pid))
          rescue Errno::ESRCH
          rescue Exception => e
            TestRunner.print_exception e
          end
          return output, ERROR_TIMEOUT, false
        end

        exitstatus = if t.value.coredump?
                           t.value.termsig + 128
                         else
                           t.value.exitstatus
                         end

        return output, exitstatus, t.value.coredump?
      rescue Errno::ENOENT  => e
        return "Failed to start #{@command} - no executable",
          ERROR_CANNOT_CREATE_PROCESS, false
      rescue
        return "Failed to start #{@command}",
          ERROR_CANNOT_CREATE_PROCESS, false
      end
    end

    def run_cmd()
      @reporter.log_start_command @command
      StringIO.open do |content|
        content.puts "# Start command: #{@command}"
        output, exitstatus, core = start_process_timeout
        content.puts output
        return content.string, exitstatus, core
      end
    end

  end # Runner
end # module
