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

#include "udp_server.h"
#include <algorithm>
#include <arpa/inet.h>
#include <iostream>
#include "common/media_log.h"
#include "network/session/udp_session.h"
#include "network/socket/socket_utils.h"
#include "network/socket/udp_socket.h"
#include "utils/utils.h"
namespace OHOS {
namespace Sharing {
bool UdpServer::Start(uint16_t port, const std::string &host, bool enableReuse, uint32_t backlog)
{
    SHARING_LOGD("server ip:%{public}s, Port:%{public}d, thread_id: %{public}llu.", host.c_str(), port, GetThreadId());
    std::unique_lock<std::shared_mutex> lk(mutex_);
    socket_ = std::make_unique<UdpSocket>();
    if (socket_) {
        if (socket_->Bind(port, host, enableReuse)) {
            SHARING_LOGD("start success, fd: %{public}d.", socket_->GetLocalFd());

            auto eventRunner = OHOS::AppExecFwk::EventRunner::Create(true);
            eventHandler_ = std::make_shared<UdpServerEventHandler>();
            eventHandler_->SetServer(shared_from_this());
            eventHandler_->SetEventRunner(eventRunner);
            eventRunner->Run();

            eventListener_ = std::make_shared<UdpServerEventListener>();
            eventListener_->SetServer(shared_from_this());

            return eventListener_->AddFdListener(socket_->GetLocalFd(), eventListener_, eventHandler_);
        }
    }

    SHARING_LOGE("start failed!");
    return false;
}

UdpServer::~UdpServer()
{
    SHARING_LOGD("trace.");
    Stop();
}

UdpServer::UdpServer()
{
    SHARING_LOGD("trace.");
}

void UdpServer::Stop()
{
    SHARING_LOGD("stop.");
    std::unique_lock<std::shared_mutex> lk(mutex_);

    for (auto kv : sessionMap_) {
        kv.second->Shutdown();
        kv.second.reset();
        SocketUtils::CloseSocket(kv.first);
    }

    if (socket_ != nullptr) {
        eventListener_->RemoveFdListener(socket_->GetLocalFd());
        SocketUtils::ShutDownSocket(socket_->GetLocalFd());
        SocketUtils::CloseSocket(socket_->GetLocalFd());
        socket_.reset();
    }
}

SocketInfo::Ptr UdpServer::GetSocketInfo()
{
    SHARING_LOGD("trace.");
    return socket_;
}

void UdpServer::CloseClientSocket(int32_t fd)
{
    SHARING_LOGD("fd: %{public}d.", fd);
    std::unique_lock<std::shared_mutex> lk(mutex_);
    if (fd > 0) {
        auto itemItr = sessionMap_.find(fd);
        if (itemItr != sessionMap_.end()) {
            if (itemItr->second) {
                itemItr->second->Shutdown();
                itemItr->second.reset();
            }
            SocketUtils::CloseSocket(fd);
            sessionMap_.erase(itemItr);
            SHARING_LOGD("erase fd: %{public}d.", fd);
        }
    }
}

void UdpServer::OnServerReadable(int32_t fd)
{
    MEDIA_LOGD("fd: %{public}d, thread_id: %{public}llu.", fd, GetThreadId());

    std::unique_lock<std::shared_mutex> lk(mutex_);
    if (socket_ == nullptr) {
        SHARING_LOGE("onReadable socket null!");
        return;
    }

    if (fd == socket_->GetLocalFd()) {
        int32_t retCode = 0;
        do {
            DataBuffer::Ptr buf = std::make_shared<DataBuffer>(DEAFULT_READ_BUFFER_SIZE);
            struct sockaddr_in clientAddr;
            socklen_t len = sizeof(struct sockaddr_in);
            retCode = ::recvfrom(fd, buf->Data(), DEAFULT_READ_BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &len);
            MEDIA_LOGD("recvSocket len: %{public}d,address: %{public}s,port: %{public}d,socklen: %{public}d.", retCode,
                       inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port, len);
            if (retCode > 0) {
                buf->UpdateSize(retCode);
                BaseNetworkSession::Ptr session = FindOrCreateSession(clientAddr);
                if (session) {
                    auto callback = callback_.lock();
                    if (callback) {
                        callback->OnServerReadData(fd, std::move(buf), session);
                    }
                }
            } else if (retCode == 0) {
                MEDIA_LOGE("onReadable error: %{public}s!", strerror(errno));
                break;
            }
        } while (retCode > 0);
    } else {
        MEDIA_LOGD("onReadable receive msg!");
    }
}

std::shared_ptr<BaseNetworkSession> UdpServer::FindOrCreateSession(const struct sockaddr_in &addr)
{
    MEDIA_LOGD("trace.");

    auto it = std::find_if(addrToFdMap_.begin(), addrToFdMap_.end(),
                           [&addr](std::pair<std::shared_ptr<struct sockaddr_in>, int32_t> value) {
                               return value.first->sin_addr.s_addr == addr.sin_addr.s_addr &&
                                      value.first->sin_port == addr.sin_port;
                           });

    if (it != addrToFdMap_.end()) {
        MEDIA_LOGD("find session.");
        return sessionMap_[it->second];
    } else if (socket_ != nullptr) {
        MEDIA_LOGD("not find, create session!");
        int32_t peerFd = 0;
        SocketUtils::CreateSocket(SOCK_DGRAM, peerFd);
        if (peerFd == 0 || !BindAndConnectClinetFd(peerFd, addr)) {
            SHARING_LOGE("create socket failed!");
            return nullptr;
        }

        SocketInfo::Ptr socketInfo =
            std::make_shared<SocketInfo>(socket_->GetLocalIp(), inet_ntoa(addr.sin_addr), socket_->GetLocalFd(), peerFd,
                                         socket_->GetLocalPort(), addr.sin_port);
        memcpy(&socketInfo->udpClientAddr_, &addr, sizeof(struct sockaddr_in));
        socketInfo->SetSocketType(SOCKET_TYPE_UDP);

        if (socketInfo) {
            BaseNetworkSession::Ptr session = std::make_shared<UdpSession>(std::move(socketInfo));
            if (session) {
                auto peerAddr = std::make_shared<struct sockaddr_in>();
                memcpy(peerAddr.get(), &addr, sizeof(struct sockaddr_in));
                addrToFdMap_.insert(make_pair(peerAddr, peerFd));
                sessionMap_.insert(make_pair(peerFd, std::move(session)));
                auto callback = callback_.lock();
                if (callback) {
                    callback->OnAccept(sessionMap_[peerFd]);
                }

                return sessionMap_[peerFd];
            }
        }
    }

    return nullptr;
}

bool UdpServer::BindAndConnectClinetFd(int32_t fd, const struct sockaddr_in &addr)
{
    SHARING_LOGD("trace.");

    int32_t ret = 0;
    SocketUtils::SetNonBlocking(fd);
    SocketUtils::SetReusePort(fd, true);
    SocketUtils::SetReuseAddr(fd, true);
    SocketUtils::SetSendBuf(fd);
    SocketUtils::SetRecvBuf(fd);

    if (!SocketUtils::BindSocket(fd, "", socket_->GetLocalPort())) {
        SocketUtils::ShutDownSocket(fd);
        SHARING_LOGE("bind BindSocket Failed!");
        return false;
    }

    SocketUtils::ConnectSocket(fd, true, inet_ntoa(addr.sin_addr), addr.sin_port, ret);
    if (ret < 0) {
        SHARING_LOGE("connectSocket error: %{public}s!", strerror(errno));
        SocketUtils::CloseSocket(fd);
        return false;
    }

    return true;
}

} // namespace Sharing
} // namespace OHOS