/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "util/relation.h"

#include "util/tests/verifier_test.h"

#include "runtime/include/mem/panda_containers.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST_F(VerifierTest, Relation0)
{
    Relation relation;
    using Set = PandaSet<size_t>;
    relation.EnsureMinSize(8);

    /*
    +--> 2 --+
    |        |
    |        v
    1        4
    |        ^
    |        |
    +--> 3 --+
    */
    relation += {{1, 2}, {1, 3}, {2, 4}, {3, 4}};

    auto get_set_from = [&relation](size_t from) {
        Set result;
        relation.ForAllFrom(from, [&result](size_t to) {
            result.insert(to);
            return true;
        });
        return result;
    };

    auto get_set_to = [&relation](size_t to) {
        Set result;
        relation.ForAllTo(to, [&result](size_t from) {
            result.insert(from);
            return true;
        });
        return result;
    };

    auto lhs = get_set_from(1);
    auto rhs = (Set {2, 3, 4});
    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(get_set_from(1), (Set {2, 3, 4}));
    EXPECT_EQ(get_set_from(2), (Set {4}));
    EXPECT_EQ(get_set_from(3), (Set {4}));
    EXPECT_EQ(get_set_to(4), (Set {1, 2, 3}));
    EXPECT_EQ(get_set_to(2), (Set {1}));
    EXPECT_EQ(get_set_to(3), (Set {1}));
}

TEST_F(VerifierTest, Relation1)
{
    Relation relation;
    using Set = PandaSet<size_t>;
    relation.EnsureMinSize(8);
    // check classes of equivalence, formed by loops in relation
    // object in loop are relationally indistinguishable

    /*   +-----------+
         v           |
    +--> 2 --+       |
    |        |  +--> 5
    |        v /
    1        4 -----> 6
    |        ^
    |        |
    +--> 3 --+
    */
    relation += {{1, 2}, {1, 3}, {2, 4}, {3, 4}, {4, 5}, {5, 2}, {4, 6}};

    auto get_set_from = [&relation](size_t from) {
        Set result;
        relation.ForAllFrom(from, [&result](size_t to) {
            result.insert(to);
            return true;
        });
        return result;
    };

    auto get_set_to = [&relation](size_t to) {
        Set result;
        relation.ForAllTo(to, [&result](size_t from) {
            result.insert(from);
            return true;
        });
        return result;
    };

    EXPECT_EQ(get_set_to(4), get_set_to(5));
    EXPECT_EQ(get_set_to(4), get_set_to(2));
    EXPECT_EQ(get_set_from(4), get_set_from(5));
    EXPECT_EQ(get_set_from(4), get_set_from(2));
}

TEST_F(VerifierTest, Relation2)
{
    Relation relation;

    using Set = PandaSet<size_t>;

    relation.EnsureMinSize(8);
    /*   +-----------+
         v           |
    +--> 2 --+       |
    |        |  +--> 5
    |        v /
    1        4 -----> 6 -----> 7
    |        ^                 ^
    |        |                /
    +--> 3 --+               /
         \------------------/
    */

    relation += {{6, 7}, {3, 7}, {1, 2}, {1, 3}, {2, 4}, {3, 4}, {4, 5}, {5, 2}, {4, 6}};

    auto get_set_between = [&relation](size_t from, size_t to) {
        Set result;
        relation.ForAllBetween(from, to, [&result](size_t elt) {
            result.insert(elt);
            return true;
        });
        return result;
    };

    EXPECT_EQ(get_set_between(3, 7), Set({2, 4, 5, 6}));
}
}  // namespace panda::verifier::test
