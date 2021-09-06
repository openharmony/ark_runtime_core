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

#include "util/int_set.h"

#ifdef PANDA_CATCH2
#include <rapidcheck/catch.h>
#include "util/tests/environment.h"
#endif

using namespace panda::verifier;

namespace panda::verifier::test {

#ifdef PANDA_CATCH2

namespace {

const EnvOptions Options {"VERIFIER_TEST"};

using T = size_t;
// to actually get to the threshold in tests
constexpr size_t THRESHOLD = 32;
using StdSetT = std::set<T>;
using IntSetT = IntSet<T, THRESHOLD>;

void AssertSetsEqual(const StdSetT &model, const IntSetT &sut)
{
    RC_ASSERT(model.size() == sut.Size());
    for (auto x : model) {
        RC_ASSERT(sut.Contains(x));
    }
    RC_TAG(sut.Size() < THRESHOLD ? "sorted vector" : "bitvector");
}

template <typename StreamT>
void AssertLazySetsEqual(const StdSetT &model, StreamT &&sut)
{
    Index<size_t> tmp = sut();
    size_t size = 0;
    while (tmp.IsValid()) {
        RC_ASSERT(model.find(tmp) != model.end());
        size++;
        tmp = sut();
    }
    RC_ASSERT(model.size() == size);
}

IntSetT MakeIntSet(const StdSetT &model)
{
    IntSetT result;
    for (T x : model) {
        result.Insert(x);
    }
    return result;
}

}  // namespace

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
TEST_CASE("Test IntSet behaves like std::set", "verifier_IntSetT")
{
    T max_value = 2048;
    auto value_gen = rc::gen::inRange<T>(0, max_value);

    rc::prop("Insert", [&]() {
        StdSetT set = *rc::gen::container<StdSetT>(value_gen);
        bool pick_from_set = *rc::gen::arbitrary<bool>();
        T value = pick_from_set ? *rc::gen::elementOf(set) : *value_gen;
        RC_PRE(Index(value).IsValid());
        RC_TAG(set.find(value) == set.end() ? "value not in set" : "value in set");
        IntSetT int_set {MakeIntSet(set)};

        set.insert(value);
        int_set.Insert(value);

        AssertSetsEqual(set, int_set);
    });

    rc::prop("InsertMany", [&]() {
        StdSetT set = *rc::gen::container<StdSetT>(value_gen);
        auto values = *rc::gen::container<std::vector<T>>(value_gen);
        bool sorted = *rc::gen::arbitrary<bool>();
        IntSetT int_set {MakeIntSet(set)};

        set.insert(values.begin(), values.end());
        if (sorted) {
            std::sort(values.begin(), values.end());
            int_set.Insert<true>(values.begin(), values.end());
        } else {
            int_set.Insert(values.begin(), values.end());
        }

        AssertSetsEqual(set, int_set);
    });

    rc::prop("Intersect/IntersectionSize", [&]() {
        StdSetT set1 = *rc::gen::container<StdSetT>(value_gen), set2 = *rc::gen::container<StdSetT>(value_gen);
        size_t num_common_elems = *rc::gen::inRange<size_t>(0, 2 * THRESHOLD);
        std::vector<T> common_elems = *rc::gen::unique<std::vector<T>>(num_common_elems, value_gen);

        for (T value : common_elems) {
            set1.insert(value);
            set2.insert(value);
        }

        IntSetT int_set1 {MakeIntSet(set1)}, int_set2 {MakeIntSet(set2)};

        StdSetT std_intersection;
        std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                              std::inserter(std_intersection, std_intersection.begin()));
        IntSetT int_set_intersection = int_set1 & int_set2;

        AssertSetsEqual(std_intersection, int_set_intersection);

        AssertLazySetsEqual(std_intersection, int_set1.LazyIntersect(int_set2));

        int_set1 &= int_set2;
        AssertSetsEqual(std_intersection, int_set1);
    });

    rc::prop("Union", [&]() {
        StdSetT set1 = *rc::gen::container<StdSetT>(value_gen), set2 = *rc::gen::container<StdSetT>(value_gen);
        size_t num_common_elems = *rc::gen::inRange<size_t>(0, 2 * THRESHOLD);
        std::vector<T> common_elems = *rc::gen::unique<std::vector<T>>(num_common_elems, value_gen);
        for (T value : common_elems) {
            set1.insert(value);
            set2.insert(value);
        }

        IntSetT int_set1 {MakeIntSet(set1)}, int_set2 {MakeIntSet(set2)};

        StdSetT std_union;
        std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::inserter(std_union, std_union.begin()));
        IntSetT int_set_union = int_set1 | int_set2;

        AssertSetsEqual(std_union, int_set_union);

        int_set1 |= int_set2;
        AssertSetsEqual(std_union, int_set1);
    });
}

#endif  // !PANDA_CATCH2

}  // namespace panda::verifier::test
