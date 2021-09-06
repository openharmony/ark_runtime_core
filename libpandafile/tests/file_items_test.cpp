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

#include "file_items.h"
#include "file_writer.h"

#include <cstdint>

#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace panda::panda_file::test {

TEST(LineNumberProgramItem, EmitSpecialOpcode)
{
    LineNumberProgramItem item;

    constexpr int32_t LINE_MAX_INC = LineNumberProgramItem::LINE_RANGE + LineNumberProgramItem::LINE_BASE - 1;
    constexpr int32_t LINE_MIN_INC = LineNumberProgramItem::LINE_BASE;

    EXPECT_FALSE(item.EmitSpecialOpcode(0, LINE_MAX_INC + 1));
    EXPECT_FALSE(item.EmitSpecialOpcode(0, LINE_MIN_INC - 1));
    EXPECT_FALSE(item.EmitSpecialOpcode(100, LINE_MAX_INC));

    std::vector<std::pair<int32_t, uint32_t>> incs = {{1, LINE_MIN_INC}, {2, LINE_MAX_INC}};
    std::vector<uint8_t> data;

    for (auto [pc_inc, line_inc] : incs) {
        ASSERT_TRUE(item.EmitSpecialOpcode(pc_inc, line_inc));
        data.push_back((line_inc - LineNumberProgramItem::LINE_BASE) + (pc_inc * LineNumberProgramItem::LINE_RANGE) +
                       LineNumberProgramItem::OPCODE_BASE);
    }

    MemoryWriter writer;
    ASSERT_TRUE(item.Write(&writer));

    EXPECT_EQ(writer.GetData(), data);
}

}  // namespace panda::panda_file::test
