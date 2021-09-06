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

#include "ipc_message.h"
#include "utils/logger.h"
#include "ipc_unix_socket.h"

namespace panda::dprof::ipc {
bool SendMessage(int fd, const Message &message)
{
    Message::Id messageId = message.GetId();
    if (!SendAll(fd, &messageId, sizeof(messageId))) {
        PLOG(ERROR, DPROF) << "Cannot send message id";
        return false;
    }

    uint32_t size = message.GetSize();
    if (!SendAll(fd, &size, sizeof(size))) {
        PLOG(ERROR, DPROF) << "Cannot send data size";
        return false;
    }

    if (size != 0 && !SendAll(fd, message.GetData(), message.GetSize())) {
        PLOG(ERROR, DPROF) << "Cannot send message data, size=" << message.GetSize();
        return false;
    }

    return true;
}

int RecvMessage(int fd, Message &message)
{
    constexpr int DEFAULT_TIMEOUT = 500; /* 0.5 sec */

    Message::Id messageId;
    int ret = RecvTimeout(fd, &messageId, sizeof(messageId), DEFAULT_TIMEOUT);
    if (ret == 0) {
        // socket was closed
        return 0;
    }
    if (ret == -1) {
        LOG(ERROR, DPROF) << "Cannot get messageId";
        return -1;
    }

    uint32_t size;
    if (RecvTimeout(fd, &size, sizeof(size), DEFAULT_TIMEOUT) <= 0) {
        LOG(ERROR, DPROF) << "Cannot get data size";
        return -1;
    }

    if (size > Message::MAX_DATA_SIZE) {
        LOG(ERROR, DPROF) << "Data size is too large, size=" << size;
        return -1;
    }

    std::vector<uint8_t> data(size);
    if (size != 0) {
        if (RecvTimeout(fd, data.data(), data.size(), DEFAULT_TIMEOUT) <= 0) {
            LOG(ERROR, DPROF) << "Cannot get message data";
            return -1;
        }
    }

    message = Message(messageId, std::move(data));
    return 1;
}
}  // namespace panda::dprof::ipc
