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

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include "common/sharing_log.h"
#include "network/interfaces/iclient_callback.h"
#include "network/interfaces/iserver_callback.h"
#include "network/interfaces/isession_callback.h"
#include "network/network_factory.h"

using namespace std;
using namespace OHOS::Sharing;
using namespace testing::ext;

namespace OHOS {
namespace Sharing {

class NetworkTcpUnitTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

class TcpTestAgent final : public IServerCallback,
                           public IClientCallback,
                           public ISessionCallback,
                           public std::enable_shared_from_this<TcpTestAgent> {
public:
    TcpTestAgent() {}

    virtual ~TcpTestAgent()
    {
        if (serverPtr_) {
            serverPtr_->Stop();
        }
    }

    bool StartTcpServer(uint16_t port)
    {
        bool ret = NetworkFactory::CreateTcpServer(port, shared_from_this(), serverPtr_);
        if (!ret) {
            SHARING_LOGE("===[TcpTestAgent] StartTcpServer failed");
        } else {
            SHARING_LOGD("===[TcpTestAgent] server agent started");
        }
        return ret;
    }

    bool StartTcpClient(const std::string &peerIp, uint16_t port)
    {
        bool ret = NetworkFactory::CreateTcpClient(peerIp, port, shared_from_this(), clientPtr_);
        if (!ret) {
            SHARING_LOGE("===[TcpTestAgent] StartTcpClient failed");
        } else {
            SHARING_LOGD("===[TcpTestAgent] client agent started");
        }
        return ret;
    }

    virtual void OnAccept(INetworkSession::Ptr session) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnServerAccept");
        session->RegisterCallback(shared_from_this());
        session->Start();
        static int index = 0;
        DataBuffer::Ptr buf = std::make_shared<DataBuffer>();
        const DataBuffer::Ptr &bufRefs = buf;
        string msg = "tcp server accept send message=" + std::to_string(index++);
        buf->PushData(msg.c_str(), msg.size());
        session->Send(bufRefs, bufRefs->Size());
        sessionPtrVec_.push_back(session);
    }

    virtual void OnServerReadData(int32_t fd, const DataBuffer::Ptr &buf, const INetworkSession::Ptr session) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnServerReadData");
        static int index = 0;
        buf->PrintBuf();
        DataBuffer::Ptr bufSend = std::make_shared<DataBuffer>();
        const DataBuffer::Ptr &bufRefs = bufSend;
        string msg = "tcp server message.index=" + std::to_string(index++);
        bufSend->PushData(msg.c_str(), msg.size());
        size_t nSize = sessionPtrVec_.size();
        for (size_t i = 0; i < nSize; i++) {
            sessionPtrVec_[i]->Send(bufRefs, bufRefs->Size());
            sleep(2);
        }
    }

    virtual void OnServerWriteable(int32_t fd) override
    {
        SHARING_LOGD("onServerWriteable");
    }

    virtual void OnServerClose(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnServerClose");
        serverPtr_ = nullptr;
    }

    virtual void OnServerException(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnServerException");
        serverPtr_ = nullptr;
    }

    virtual void OnSessionReadData(int32_t fd, const DataBuffer::Ptr &buf) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnSessionReadData");
        buf->PrintBuf();
        static int index = 0;
        DataBuffer::Ptr bufSend = std::make_shared<DataBuffer>();
        string msg = "tcp session send message=" + std::to_string(index++);
        bufSend->PushData(msg.c_str(), msg.size());
        size_t nSize = sessionPtrVec_.size();
        for (size_t i = 0; i < nSize; i++) {
            if (sessionPtrVec_[i]->GetSocketInfo()->GetPeerFd() == fd) {
                sessionPtrVec_[i]->Send(bufSend, bufSend->Size());
                break;
            }
        }
        sleep(3);
    }

    virtual void OnSessionWriteable(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnSessionWriteable");
    }

    virtual void OnSessionClose(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnSessionClose");
    }

    virtual void OnSessionException(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnSessionException");
    }

    virtual void OnClientConnectResult(bool isSuccess) override
    {
        if (isSuccess) {
            SHARING_LOGD("===[TcpTestAgent] OnClientConnectResult success");
            static int index = 0;
            string msg = "tcp client OnClientConnectResult message.index=" + std::to_string(index++);
            DataBuffer::Ptr bufSend = std::make_shared<DataBuffer>();
            bufSend->PushData(msg.c_str(), msg.size());
            if (clientPtr_ != nullptr) {
                SHARING_LOGD("===[TcpTestAgent] OnClientConnectResult send");
                clientPtr_->Send(bufSend, bufSend->Size());
            } else {
                SHARING_LOGD("===[TcpTestAgent] OnClientConnectResult nullptr");
            }
        } else {
            if (clientPtr_ != nullptr) {
                SHARING_LOGE("===[TcpTestAgent] OnClientConnectResult failed");
                clientPtr_->Disconnect();
            }
        }
    }

    virtual void OnClientReadData(int32_t fd, const DataBuffer::Ptr &buf) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnClientReadData");
        buf->PrintBuf();
        static int index = 0;
        string msg = "tcp client message.index=" + std::to_string(index++);
        DataBuffer::Ptr bufSend = std::make_shared<DataBuffer>();
        bufSend->PushData(msg.c_str(), msg.size());
        clientPtr_->Send(bufSend, bufSend->Size());
        sleep(3);
    }

    virtual void OnClientWriteable(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnClientWriteable");
    }

    virtual void OnClientClose(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnClientClose");
        clientPtr_ = nullptr;
    }

    virtual void OnClientException(int32_t fd) override
    {
        SHARING_LOGD("===[TcpTestAgent] OnClientException");
        clientPtr_ = nullptr;
    }

private:
    NetworkFactory::ServerPtr serverPtr_{nullptr};
    NetworkFactory::ClientPtr clientPtr_{nullptr};
    std::vector<INetworkSession::Ptr> sessionPtrVec_;
};

namespace {
HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_001, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
}

HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_002, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
    auto ret = tcpAgent->StartTcpServer(8888);
    ASSERT_TRUE(ret);
}

HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_003, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
    auto ret = tcpAgent->StartTcpServer(8889);
    ASSERT_TRUE(ret);
    ret = tcpAgent->StartTcpClient("127.0.0.1", 8889);
    ASSERT_TRUE(ret);
}

HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_004, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
    auto ret = tcpAgent->StartTcpServer(8890);
    ASSERT_TRUE(ret);
    ret = tcpAgent->StartTcpClient("127.0.0.1", 1888);
    ASSERT_TRUE(ret);
}

HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_005, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
    auto ret = tcpAgent->StartTcpServer(8889);
    ASSERT_FALSE(ret);
    ret = tcpAgent->StartTcpClient("1212121", 8889);
    ASSERT_FALSE(ret);
}

HWTEST_F(NetworkTcpUnitTest, NetworkTcpUnitTest_006, TestSize.Level0)
{
    auto tcpAgent = std::make_shared<TcpTestAgent>();
    ASSERT_TRUE(tcpAgent != nullptr);
    auto ret = tcpAgent->StartTcpServer(8891);
    ASSERT_TRUE(ret);
    ret = tcpAgent->StartTcpClient("1212121", 8891);
    ASSERT_FALSE(ret);
}
} // namespace
} // namespace Sharing
} // namespace OHOS