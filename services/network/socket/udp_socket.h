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

#ifndef OHOS_SHARING_UDP_SOCKET_H
#define OHOS_SHARING_UDP_SOCKET_H

#include "base_socket.h"

namespace OHOS {
namespace Sharing {
class UdpSocket : public BaseSocket {
public:
    using Ptr = std::shared_ptr<UdpSocket>;

    UdpSocket();
    ~UdpSocket() override;

    bool Bind(const uint16_t port, const std::string &host, bool enableReuse = true,
              uint32_t backlog = MAX_BACKLOG_NUM) override;
    bool Connect(const std::string &peerIp, uint16_t peerPort, int32_t &retCode, bool isAsync = true,
                 bool enableReuse = true, const std::string &localIp = "", uint16_t localPort = 0) override;
};
} // namespace Sharing
} // namespace OHOS
#endif