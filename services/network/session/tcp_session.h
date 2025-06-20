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

#ifndef OHOS_SHARING_TCP_SESSION_H
#define OHOS_SHARING_TCP_SESSION_H

#include "base_network_session.h"
#include <mutex>

namespace OHOS {
namespace Sharing {

class TcpSessionEventListener : public BaseSessionEventListener {};

class TcpSessionEventHandler : public BaseSessionEventHandler {};

class TcpSession final : public BaseNetworkSession {
public:
    ~TcpSession() override;
    explicit TcpSession(SocketInfo::Ptr socket);

    bool Start() override;
    void Shutdown() override;
    void OnSessionReadble(int32_t fd) override;
    bool Send(const char *buf, int32_t nSize) override;
    bool Send(const DataBuffer::Ptr &buf, int32_t nSize) override;
private:
    std::mutex mutex_;
};
} // namespace Sharing
} // namespace OHOS
#endif