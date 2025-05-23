/*
 * Copyright (c) 2023 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tcp_client.h"
#include "common/common_macro.h"
#include "common/media_log.h"
#include "common/sharing_log.h"
#include "network/socket/socket_utils.h"
#include "network/socket/tcp_socket.h"
#include "utils/utils.h"
namespace OHOS {
namespace Sharing {
TcpClient::~TcpClient()
{
    SHARING_LOGD("trace.");
    Disconnect();
}

TcpClient::TcpClient()
{
    SHARING_LOGD("trace.");
}

bool TcpClient::Connect(const std::string &peerIp, uint16_t peerPort, const std::string &localIp, uint16_t localPort)
{
    SHARING_LOGI("peerIp:%{public}s, peerPort:%{public}d, thread_id: %{public}llu.", GetAnonymousIp(peerIp).c_str(),
                 peerPort, GetThreadId());

    int32_t retCode = 0;
    socket_ = std::make_unique<TcpSocket>();
    if (socket_) {
        if (socket_->Connect(peerIp, peerPort, retCode, true, true, localIp, localPort)) {
            SHARING_LOGI("connect success.");
            auto eventRunner = OHOS::AppExecFwk::EventRunner::Create(true);
            eventHandler_ = std::make_shared<TcpClientEventHandler>();
            eventHandler_->SetClient(shared_from_this());
            eventHandler_->SetEventRunner(eventRunner);
            eventRunner->Run();

            bool ret = false;
            eventListener_ = std::make_shared<TcpClientEventListener>();
            if (eventListener_) {
                eventListener_->SetClient(shared_from_this());
                ret = eventListener_->AddFdListener(socket_->GetLocalFd(), eventListener_, eventHandler_);
            }

            auto callback = callback_.lock();
            if (callback) {
                callback->OnClientConnect(true);
                callback->OnClientWriteable(socket_->GetLocalFd());
            }

            return ret;
        } else {
            std::unique_lock<std::shared_mutex> lk(mutex_);
            if (eventListener_) {
                eventListener_->RemoveFdListener(socket_->GetLocalFd());
            } else {
                SHARING_LOGE("eventListener is nullptr.");
            }
            SocketUtils::ShutDownSocket(socket_->GetLocalFd());
            SocketUtils::CloseSocket(socket_->GetLocalFd());
            socket_.reset();
        }
    }
    SHARING_LOGE("[TcpClient] Connect failed!");
    auto callback = callback_.lock();
    if (callback) {
        callback->OnClientConnect(false);
    }

    return false;
}

void TcpClient::Disconnect()
{
    SHARING_LOGD("trace.");
    std::unique_lock<std::shared_mutex> lk(mutex_);
    if (socket_ != nullptr) {
        eventListener_->RemoveFdListener(socket_->GetLocalFd());
        SocketUtils::ShutDownSocket(socket_->GetLocalFd());
        SocketUtils::CloseSocket(socket_->GetLocalFd());
        eventListener_->OnShutdown(socket_->GetLocalFd());
        socket_.reset();
    }
}

bool TcpClient::Send(const DataBuffer::Ptr &buf, int32_t nSize)
{
    SHARING_LOGD("trace.");
    RETURN_FALSE_IF_NULL(buf);
    return Send(buf->Peek(), nSize);
}

bool TcpClient::Send(const char *buf, int32_t nSize)
{
    SHARING_LOGD("trace.");
    std::unique_lock<std::shared_mutex> lk(mutex_);
    if (socket_ != nullptr) {
        if (SocketUtils::SendSocket(socket_->GetLocalFd(), buf, nSize) != 0) {
            return true;
        } else {
            lk.unlock();
            SHARING_LOGE("send Failed, Disconnect!");
            Disconnect();
            return false;
        }
    } else {
        return false;
    }
}

bool TcpClient::Send(const std::string &msg)
{
    SHARING_LOGD("trace.");
    return Send(msg.c_str(), msg.size());
}

SocketInfo::Ptr TcpClient::GetSocketInfo()
{
    SHARING_LOGD("trace.");
    return socket_;
}

void TcpClient::OnClientReadable(int32_t fd)
{
    SHARING_LOGI("trace fd: %{public}d, thread_id: %{public}llu.", fd, GetThreadId());
    int32_t error = 0;
    int32_t retCode = 0;
    do {
        DataBuffer::Ptr buf = std::make_shared<DataBuffer>(DEFAULT_READ_BUFFER_SIZE);
        retCode = SocketUtils::RecvSocket(fd, (char *)buf->Data(), DEFAULT_READ_BUFFER_SIZE, flags_, error);
        SHARING_LOGD("recvSocket len: %{public}d.", retCode);
        if (retCode > 0) {
            buf->UpdateSize(retCode);
            auto callback = callback_.lock();
            if (callback) {
                callback->OnClientReadData(fd, std::move(buf));
            }
        } else if (retCode < 0) {
            if (error == ECONNREFUSED) {
                auto callback = callback_.lock();
                if (callback) {
                    callback->OnClientConnect(false);
                }
            }
        } else {
            SHARING_LOGE("recvSocket failed!");
            Disconnect();
        }
    } while (retCode > 0);
}

} // namespace Sharing
} // namespace OHOS