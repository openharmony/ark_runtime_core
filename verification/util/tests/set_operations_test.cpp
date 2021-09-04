/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/set_operations.h"
#include "util/range.h"

#ifdef PANDA_CATCH2
#include <catch2/catch.hpp>
#include <rapidcheck/catch.h>
#include "util/tests/environment.h"
#endif

#ifdef PANDA_GTEST
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#endif

#include <initializer_list>
#include <utility>
#include <string>

using namespace panda::verifier;

namespace panda::verifier::test {

using namespace rc;

#ifdef PANDA_CATCH2

namespace {

const EnvOptions Options {"VERIFIER_TEST"};

using Interval = Range<size_t>;
using Intervals = std::initializer_list<Interval>;

void ClassifySize(size_t size, const Intervals &intervals)
{
    for (const auto &i : intervals) {
        RC_CLASSIFY(i.Contains(size), std::to_string(i));
    }
}

}  // namespace

TEST_CASE("Test operations over sets0", "verifier_set_operations0")
{
    using set = std::set<int>;

    Intervals intervals = {{0, 10}, {11, 30}, {31, 10000}};

    auto stat = [&intervals](const std::initializer_list<size_t> &sizes) {
        if (Options.Get<bool>("verbose", false)) {
            for (size_t size : sizes) {
                ClassifySize(size, intervals);
            }
        }
    };

    SECTION("Conversion to set")
    {
        prop("ToSet", [&stat](std::vector<int> &&vec) {
            stat({vec.size()});
            set result = ToSet<set>(vec);
            for (const auto &elt : vec) {
                RC_ASSERT(result.count(elt) > 0U);
            }
        });
    }
}

TEST_CASE("Test operations over sets1", "verifier_set_operations1")
{
    using set = std::set<int>;

    SECTION("2 sets")
    {
        prop("Intersection", [](set &&set1, set &&set2) {
            set result = SetIntersection(set1, set2);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U && set2.count(elt) > 0U);
            }
            for (int elt : set1) {
                RC_ASSERT(result.count(elt) == 0U || set2.count(elt) > 0U);
            }
            for (int elt : set2) {
                RC_ASSERT(result.count(elt) == 0U || set1.count(elt) > 0U);
            }
        });
        prop("Union", [](set &&set1, set &&set2) {
            set result = SetUnion(set1, set2);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U || set2.count(elt) > 0U);
            }
            for (int elt : set1) {
                RC_ASSERT(result.count(elt) > 0U);
            }
            for (int elt : set2) {
                RC_ASSERT(result.count(elt) > 0U);
            }
        });
        prop("Difference", [](set &&set1, set &&set2) {
            set result = SetDifference(set1, set2);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U && set2.count(elt) == 0U);
            }
        });
    }
}

TEST_CASE("Test operations over sets2", "verifier_set_operations2")
{
    using set = std::set<int>;

    SECTION("3 sets")
    {
        prop("Intersection", [](set &&set1, set &&set2, set &&set3) {
            set result = SetIntersection(set1, set2, set3);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U && set2.count(elt) > 0U && set3.count(elt) > 0U);
            }
            for (int elt : set1) {
                RC_ASSERT(result.count(elt) == 0U || (set2.count(elt) > 0U && set3.count(elt) > 0U));
            }
            for (int elt : set2) {
                RC_ASSERT(result.count(elt) == 0U || (set1.count(elt) > 0U && set3.count(elt) > 0U));
            }
            for (int elt : set3) {
                RC_ASSERT(result.count(elt) == 0U || (set2.count(elt) > 0U && set1.count(elt) > 0U));
            }
        });
        prop("Union", [](set &&set1, set &&set2, set &&set3) {
            set result = SetUnion(set1, set2, set3);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U || set2.count(elt) > 0U || set3.count(elt) > 0U);
            }
            for (int elt : set1) {
                RC_ASSERT(result.count(elt) > 0U);
            }
            for (int elt : set2) {
                RC_ASSERT(result.count(elt) > 0U);
            }
            for (int elt : set3) {
                RC_ASSERT(result.count(elt) > 0U);
            }
        });
        prop("Difference", [](set &&set1, set &&set2, set &&set3) {
            set result = SetDifference(set1, set2, set3);
            for (int elt : result) {
                RC_ASSERT(set1.count(elt) > 0U && set2.count(elt) == 0U && set3.count(elt) == 0U);
            }
        });
    }
}

#endif  // !PANDA_CATCH2

}  // namespace panda::verifier::test
