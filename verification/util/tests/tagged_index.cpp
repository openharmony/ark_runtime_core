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

#include "util/tagged_index.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST(VerifierTest_TaggedIndex, size_t)
{
    enum class Tag { TAG0, TAG1, TAG2, __LAST__ = TAG2 };
    using I = TaggedIndex<Tag>;

    I idx;
    EXPECT_FALSE(idx.IsValid());
    idx = Tag::TAG0;
    EXPECT_TRUE(idx.IsValid());
    size_t val = (static_cast<size_t>(1) << (sizeof(size_t) * 8 - 2ULL)) - 1ULL;
    idx = val;
    EXPECT_TRUE(idx.IsValid());
    EXPECT_EQ(idx, val);
    EXPECT_EQ(idx, Tag::TAG0);
    idx.Invalidate();
    EXPECT_FALSE(idx.IsValid());
}

TEST(VerifierTest_TaggedIndex, int)
{
    enum class Tag { TAG0, TAG1, TAG2, __LAST__ = TAG2 };
    using I = TaggedIndex<Tag, int>;

    I idx;
    EXPECT_FALSE(idx.IsValid());
    idx = Tag::TAG2;
    EXPECT_TRUE(idx.IsValid());
    int val = (static_cast<int>(1) << (sizeof(int) * 8 - 3ULL)) - 1ULL;
    idx = val;
    EXPECT_TRUE(idx.IsValid());
    EXPECT_EQ(idx, val);
    EXPECT_EQ(idx, Tag::TAG2);
    idx.Invalidate();
    EXPECT_FALSE(idx.IsValid());
    val = -val;
    idx = Tag::TAG2;
    EXPECT_TRUE(idx.IsValid());
    idx = val;
    EXPECT_TRUE(idx.IsValid());
    EXPECT_EQ(idx, val);
    EXPECT_EQ(idx.GetTag(), Tag::TAG2);
    idx.Invalidate();
    EXPECT_FALSE(idx.IsValid());
}

}  // namespace panda::verifier::test
