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

require 'delegate'

module Verification
  module_function

  def load_data(data)
    @data = data
  end

  def compatibility_checks
    @data.compatibility_checks
  end

  def domain_types(check)
    check._domains.map do |d|
      d1 = compatibility_checks.domains[d.to_sym]
      d1.new_enum || d1.existing_enum
    end
  end
end

def Gen.on_require(data)
  Verification.load_data(data)
end
