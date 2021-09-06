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

#ifndef PANDA_DPROF_LIBDPROF_DPROF_IPC_IPC_MESSAGE_H_
#define PANDA_DPROF_LIBDPROF_DPROF_IPC_IPC_MESSAGE_H_

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "macros.h"

namespace panda::dprof::ipc {
class Message {
public:
    static const uint32_t MAX_DATA_SIZE = 1024 * 1024;  // 1MB

    enum class Id : uint8_t {
        VERSION = 0x00,
        APP_INFO = 0x01,
        FEATURE_DATA = 0x02,
        INVALID_ID = 0xff,
    };

    Message() = default;
    DEFAULT_MOVE_SEMANTIC(Message);

    template <typename T>
    Message(Id id, T &&data) : id_(id), data_(std::forward<T>(data))
    {
    }

    ~Message() = default;

    Id GetId() const
    {
        return id_;
    }

    const uint8_t *GetData() const
    {
        return data_.data();
    }

    size_t GetSize() const
    {
        return data_.size();
    }

private:
    Id id_ = Id::INVALID_ID;
    std::vector<uint8_t> data_;

    NO_COPY_SEMANTIC(Message);
};

bool SendMessage(int fd, const Message &message);
int RecvMessage(int fd, Message &message);

}  // namespace panda::dprof::ipc

#endif  // PANDA_DPROF_LIBDPROF_DPROF_IPC_IPC_MESSAGE_H_
