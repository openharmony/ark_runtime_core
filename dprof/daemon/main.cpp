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

#include "dprof/ipc/ipc_unix_socket.h"
#include "dprof/ipc/ipc_message.h"
#include "dprof/ipc/ipc_message_protocol.h"
#include "dprof/storage.h"
#include "serializer/serializer.h"
#include "utils/logger.h"
#include "utils/pandargs.h"
#include "utils/span.h"

#include <csignal>
#include <queue>
#include <sys/socket.h>
#include <thread>

#include "generated/daemon_options.h"

namespace panda::dprof {
bool CheckVersion(const os::unique_fd::UniqueFd &sock)
{
    // Get version
    ipc::Message msg;
    if (RecvMessage(sock.Get(), msg) <= 0) {
        LOG(ERROR, DPROF) << "Cannot read message";
        return false;
    }
    if (msg.GetId() != ipc::Message::Id::VERSION) {
        LOG(ERROR, DPROF) << "Incorrect first message id, id=" << static_cast<uint32_t>(msg.GetId());
        return false;
    }
    ipc::protocol::Version tmp;
    if (!serializer::BufferToStruct<ipc::protocol::VERSION_FCOUNT>(msg.GetData(), msg.GetSize(), tmp)) {
        LOG(ERROR, DPROF) << "Cannot convert data to version message";
        return false;
    }
    if (tmp.version != ipc::protocol::VERSION) {
        LOG(ERROR, DPROF) << "Incorrect version:" << tmp.version;
        return false;
    }
    return true;
}

static std::unique_ptr<AppData> ProcessingConnect(const os::unique_fd::UniqueFd &sock)
{
    if (!CheckVersion(sock)) {
        return nullptr;
    }

    ipc::protocol::AppInfo ipcAppInfo;
    {
        // Get app info
        ipc::Message msg;
        if (RecvMessage(sock.Get(), msg) <= 0) {
            LOG(ERROR, DPROF) << "Cannot read message";
            return nullptr;
        }
        if (msg.GetId() != ipc::Message::Id::APP_INFO) {
            LOG(ERROR, DPROF) << "Incorrect second message id, id=" << static_cast<uint32_t>(msg.GetId());
            return nullptr;
        }
        if (!serializer::BufferToStruct<ipc::protocol::APP_INFO_FCOUNT>(msg.GetData(), msg.GetSize(), ipcAppInfo)) {
            LOG(ERROR, DPROF) << "Cannot convert data to an app info message";
            return nullptr;
        }
    }

    // Get features data
    AppData::FeaturesMap featuresMap;
    for (;;) {
        ipc::Message msg;
        int ret = RecvMessage(sock.Get(), msg);
        if (ret == 0) {
            // There are no more messages, the socket is closed
            break;
        }
        if (ret < 0) {
            LOG(ERROR, DPROF) << "Cannot read a feature data message";
            return nullptr;
        }

        ipc::protocol::FeatureData tmp;
        if (!serializer::BufferToStruct<ipc::protocol::FEATURE_DATA_FCOUNT>(msg.GetData(), msg.GetSize(), tmp)) {
            LOG(ERROR, DPROF) << "Cannot convert data to a feature data";
            return nullptr;
        }

        featuresMap.emplace(std::pair(std::move(tmp.name), std::move(tmp.data)));
    }

    return AppData::CreateByParams(ipcAppInfo.app_name, ipcAppInfo.hash, ipcAppInfo.pid, std::move(featuresMap));
}

class Worker {
public:
    void EnqueueClientSocket(os::unique_fd::UniqueFd clientSock)
    {
        os::memory::LockHolder lock(queue_lock_);
        queue_.push(std::move(clientSock));
        cond_.Signal();
    }

    void Start(AppDataStorage *storage)
    {
        done_ = false;
        thread_ = std::thread([this](AppDataStorage *strg) { DoRun(strg); }, storage);
    }

    void Stop()
    {
        os::memory::LockHolder lock(queue_lock_);
        done_ = true;
        cond_.Signal();
        thread_.join();
    }

    void DoRun(AppDataStorage *storage)
    {
        while (!done_) {
            os::unique_fd::UniqueFd clientSock;
            {
                os::memory::LockHolder lock(queue_lock_);
                while (queue_.empty() && !done_) {
                    cond_.Wait(&queue_lock_);
                }
                if (done_) {
                    break;
                }

                clientSock = std::move(queue_.front());
                queue_.pop();
            }

            auto appData = ProcessingConnect(clientSock);
            if (!appData) {
                LOG(ERROR, DPROF) << "Cannot process connection";
                continue;
            }

            storage->SaveAppData(*appData);
        }
    }

private:
    std::thread thread_;
    os::memory::Mutex queue_lock_;
    std::queue<os::unique_fd::UniqueFd> queue_;
    os::memory::ConditionVariable cond_ GUARDED_BY(queue_lock_);
    bool done_ = false;
};

class ArgsParser {
public:
    bool Parse(panda::Span<const char *> args)
    {
        options_.AddOptions(&parser_);
        if (!parser_.Parse(args.Size(), args.Data())) {
            std::cerr << parser_.GetErrorString();
            return false;
        }
        auto err = options_.Validate();
        if (err) {
            std::cerr << err.value().GetMessage() << std::endl;
            return false;
        }
        if (options_.GetStorageDir().empty()) {
            std::cerr << "Option \"storage-dir\" is not set" << std::endl;
            return false;
        }
        return true;
    }

    const Options &GetOptionos() const
    {
        return options_;
    }

    void Help() const
    {
        std::cerr << "Usage: " << app_name_ << " [OPTIONS]" << std::endl;
        std::cerr << "optional arguments:" << std::endl;
        std::cerr << parser_.GetHelpString() << std::endl;
    }

private:
    std::string app_name_;
    PandArgParser parser_;
    Options options_ {""};
};

static bool g_done = false;

static void SignalHandler(int sig)
{
    if (sig == SIGINT || sig == SIGHUP || sig == SIGTERM) {
        g_done = true;
    }
}

static void SetupSignals()
{
    struct sigaction sa {
    };
    PLOG_IF(::memset_s(&sa, sizeof(sa), 0, sizeof(sa)) != EOK, FATAL, DPROF) << "memset_s failed";
    sa.sa_handler = SignalHandler;  // NOLINT(cppcoreguidelines-pro-type-union-access)
    PLOG_IF(::sigemptyset(&sa.sa_mask) == -1, FATAL, DPROF) << "sigemptyset() failed";

    PLOG_IF(::sigaction(SIGINT, &sa, nullptr) == -1, FATAL, DPROF) << "sigaction(SIGINT) failed";
    PLOG_IF(::sigaction(SIGHUP, &sa, nullptr) == -1, FATAL, DPROF) << "sigaction(SIGHUP) failed";
    PLOG_IF(::sigaction(SIGTERM, &sa, nullptr) == -1, FATAL, DPROF) << "sigaction(SIGTERM) failed";
}

static int Main(panda::Span<const char *> args)
{
    const int MAX_PENDING_CONNECTIONS_QUEUE = 32;

    ArgsParser parser;
    if (!parser.Parse(args)) {
        parser.Help();
        return -1;
    }
    const Options &options = parser.GetOptionos();

    Logger::InitializeStdLogging(Logger::LevelFromString(options.GetLogLevel()), panda::LoggerComponentMaskAll);

    SetupSignals();

    auto storage = AppDataStorage::Create(options.GetStorageDir(), true);
    if (!storage) {
        LOG(FATAL, DPROF) << "Cannot init storage";
        return -1;
    }

    // Create server socket
    os::unique_fd::UniqueFd sock(ipc::CreateUnixServerSocket(MAX_PENDING_CONNECTIONS_QUEUE));
    if (!sock.IsValid()) {
        LOG(FATAL, DPROF) << "Cannot create socket";
        return -1;
    }

    Worker worker;
    worker.Start(storage.get());

    LOG(INFO, DPROF) << "Daemon is ready for connections";
    // Main loop
    while (!g_done) {
        os::unique_fd::UniqueFd clientSock(::accept4(sock.Get(), nullptr, nullptr, SOCK_CLOEXEC));
        if (!clientSock.IsValid()) {
            if (errno == EINTR) {
                continue;
            }
            PLOG(FATAL, DPROF) << "accept() failed";
            return -1;
        }
        worker.EnqueueClientSocket(std::move(clientSock));
    }
    LOG(INFO, DPROF) << "Daemon has received an end signal and stops";
    worker.Stop();
    LOG(INFO, DPROF) << "Daemon is stopped";
    return 0;
}
}  // namespace panda::dprof

int main(int argc, const char *argv[])
{
    panda::Span<const char *> args(argv, argc);
    return panda::dprof::Main(args);
}
