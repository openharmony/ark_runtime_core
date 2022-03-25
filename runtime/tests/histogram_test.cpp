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

#include "include/histogram-inl.h"
#include "include/runtime.h"
#include <gtest/gtest.h>

namespace panda::test {

class HistogramTest : public testing::Test {
public:
    HistogramTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~HistogramTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    template <class Value>
    void CompareTwoHistogram(const Histogram<Value> &lhs, const Histogram<Value> &rhs)
    {
        ASSERT_EQ(lhs.GetSum(), rhs.GetSum());
        ASSERT_EQ(lhs.GetMin(), rhs.GetMin());
        ASSERT_EQ(lhs.GetMax(), rhs.GetMax());
        ASSERT_EQ(lhs.GetAvg(), rhs.GetAvg());
        ASSERT_EQ(lhs.GetCount(), rhs.GetCount());
    }

    struct IntWrapper {
        int element;
        IntWrapper(int new_element) : element(new_element) {}
        IntWrapper(const IntWrapper &new_wrapper) : element(new_wrapper.element) {}
        IntWrapper &operator=(const IntWrapper &new_wrapper)
        {
            element = new_wrapper.element;
            return *this;
        }
        bool operator<(const IntWrapper &other_wrapper) const
        {
            return element < other_wrapper.element;
        }
        bool operator==(const IntWrapper &other_wrapper) const
        {
            return element == other_wrapper.element;
        }
        double operator/(double divider) const
        {
            return element / divider;
        }
        const IntWrapper operator+(const IntWrapper &other_wrapper) const
        {
            return IntWrapper(element + other_wrapper.element);
        }
        void operator+=(const IntWrapper &other_wrapper)
        {
            element += other_wrapper.element;
        }
        const IntWrapper operator*(const IntWrapper &other_wrapper) const
        {
            return IntWrapper(element * other_wrapper.element);
        }

        std::ostream &operator<<(std::ostream &os)
        {
            return os << element;
        }
    };

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(HistogramTest, SimpleIntTest)
{
    std::vector<int> simple_vector = {1, 1515, -12, 130, -1, 124, 0};
    Histogram<int> hist;
    for (auto element : simple_vector) {
        hist.AddValue(element);
    }
    CompareTwoHistogram(hist, Histogram<int>(simple_vector.begin(), simple_vector.end()));
    ASSERT_EQ(hist.GetSum(), 1'757);
    ASSERT_EQ(hist.GetMin(), -12);
    ASSERT_EQ(hist.GetMax(), 1515);
    ASSERT_EQ(hist.GetAvg(), 251);
    ASSERT_EQ(hist.GetDispersion(), 269520);
    ASSERT_EQ(hist.GetCount(), 7);
}

TEST_F(HistogramTest, IntWrapperTest)
{
    Histogram<IntWrapper> hist;
    std::vector<int> simple_vector = {1, 1515, -12, 129, 0, 124, 0};
    for (auto element : simple_vector) {
        hist.AddValue(IntWrapper(element));
    }
    ASSERT_EQ(hist.GetSum(), IntWrapper(1'757));
    ASSERT_EQ(hist.GetMin(), IntWrapper(-12));
    ASSERT_EQ(hist.GetMax(), IntWrapper(1515));
    ASSERT_EQ(hist.GetAvg(), 251);
    ASSERT_EQ(hist.GetCount(), 7);
}

TEST_F(HistogramTest, CompareTwoDifferentTest)
{
    std::vector<int> simple_vector_first = {1, 1515, -12, 129, 0, 124, 0};
    std::vector<int> simple_vector_second = {1, 1515, -12, 130, 3, 120, 0};
    Histogram<int> hist_first(simple_vector_first.begin(), simple_vector_first.end());
    Histogram<int> hist_second(simple_vector_second.begin(), simple_vector_second.end());
    CompareTwoHistogram(hist_first, hist_second);
}

TEST_F(HistogramTest, CompareDifferentTypeTest)
{
    std::unordered_set<int> simple_set_first = {1, 1515, -12, 130, -1, 124, 0};
    PandaSet<int> panda_set_first = {1, 1515, -12, 129, 2, 122, 0};

    std::vector<int> simple_vector_second = {1, 1515, -12, 129, 0, 124, 0};
    PandaVector<int> panda_vector_first = {5, 1515, -12, 128, -3, 124, 0};

    Histogram<int> hist_first(simple_set_first.begin(), simple_set_first.end());
    Histogram<int> hist_second(panda_set_first.begin(), panda_set_first.end());
    Histogram<int> hist_third(simple_vector_second.begin(), simple_vector_second.end());
    Histogram<int> hist_fourth(panda_vector_first.begin(), panda_vector_first.end());

    CompareTwoHistogram(hist_first, hist_second);
    CompareTwoHistogram(hist_first, hist_third);
    CompareTwoHistogram(hist_first, hist_fourth);
    CompareTwoHistogram(hist_second, hist_third);
    CompareTwoHistogram(hist_second, hist_fourth);
    CompareTwoHistogram(hist_third, hist_fourth);
}

TEST_F(HistogramTest, CheckGetTopDumpTest)
{
    std::vector<int> simple_vector = {1, 1, 0, 12, 0, 1, 12};
    Histogram<int> hist(simple_vector.begin(), simple_vector.end());
    ASSERT_EQ(hist.GetTopDump(), "0:2,1:3,12:2");
    ASSERT_EQ(hist.GetTopDump(2U), "0:2,1:3");
    ASSERT_EQ(hist.GetTopDump(1), "0:2");
    ASSERT_EQ(hist.GetTopDump(0), "");
}

}  // namespace panda::test
