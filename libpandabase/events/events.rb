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
require 'delegate'

class String
  def camelize
    self.split('_').collect(&:capitalize).join
  end
end

class Field
  attr_reader :name
  attr_reader :type

  def initialize(event, dscr)
    @dscr = dscr
    @name = dscr['name']
    if dscr['type'] =='enum'
      @type = event.name.camelize + dscr['name'].camelize
    else
      @type = dscr['type']
    end
    @need_move = ['std::string'].include? @type
  end

  def arg_type
    @need_move ? "const #{@type}&" : @type
  end

  def is_enum?
    @dscr['type'] =='enum'
  end

  def enums
    return @dscr['enum']
  end
end

class Event
  attr_reader :name

  def initialize(dscr)
     @dscr = dscr
     @name = dscr['name']
  end

  def fields
    @fields ||= @dscr['fields'].map { |field| Field.new(self, field) }
  end

  def enable?
    @dscr.respond_to?('enable') ? @dscr['enable'] : true
  end

end

module EventsData
  module_function

  def events
    @events
  end

  def wrap_data(data)
    @events = data.events.map { |op| Event.new(op) }
  end
end

def Gen.on_require(data)
  EventsData.wrap_data(data)
end
