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

#include "socket_utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/tcp.h>
#include <unistd.h>
#include "common/const_def.h"
#include "common/media_log.h"

namespace OHOS {
namespace Sharing {
uint16_t SocketUtils::minPort_ = MIN_PORT;
uint16_t SocketUtils::maxPort_ = MAX_PORT;

bool SocketUtils::CreateTcpServer(const char *ip, unsigned port, int32_t &fd)
{
    SHARING_LOGD("trace.");
    return CreateSocket(SOCK_STREAM, fd) && SetReuseAddr(fd, true) && SetNoDelay(fd, true) &&
           BindSocket(fd, ip, port) && ListenSocket(fd);
}

uint16_t SocketUtils::GetAvailableUdpPortPair()
{
    SHARING_LOGD("trace.");
    static uint16_t g_availablePort = minPort_;

    SHARING_LOGD("current udp port: %{public}d.", g_availablePort);
    if (g_availablePort >= maxPort_) {
        g_availablePort = minPort_;
    }

    uint16_t port = GetAvailableUdpPortPair(g_availablePort, maxPort_);
    if (port != 0) {
        g_availablePort = port + 2;
        return port;
    }

    port = GetAvailableUdpPortPair(minPort_, g_availablePort);
    if (port != 0) {
        g_availablePort = port + 2;
    }

    return port;
}

uint16_t SocketUtils::GetAvailableUdpPortPair(uint16_t minPort, uint16_t maxPort)
{
    SHARING_LOGD("trace.");
    if (minPort == maxPort) {
        return 0;
    }

    uint16_t port = minPort;
    while (true) {
        if (port >= maxPort) {
            port = 0;
            break;
        } else if (IsUdpPortAvailable(port) && IsUdpPortAvailable(port + 1)) {
            break;
        } else {
            port += 2;
        }
    }

    return port;
}

bool SocketUtils::IsUdpPortAvailable(uint16_t port)
{
    SHARING_LOGD("trace.");
    int32_t fd = -1;
    auto ret = CreateSocket(SOCK_DGRAM, fd) && BindSocket(fd, "", port);
    if (fd != -1) {
        CloseSocket(fd);
    }

    return ret;
}

bool SocketUtils::CreateTcpClient(const char *ip, unsigned port, int32_t &fd, int32_t &ret)
{
    SHARING_LOGD("trace.");
    return CreateSocket(SOCK_STREAM, fd) && ConnectSocket(fd, true, ip, port, ret);
}

bool SocketUtils::CreateUdpSession(unsigned port, int32_t &fd)
{
    SHARING_LOGD("trace.");
    return CreateSocket(SOCK_DGRAM, fd) && SetRecvBuf(fd) && SetSendBuf(fd) && BindSocket(fd, "", port);
}

bool SocketUtils::CreateSocket(int32_t socketType, int32_t &fd)
{
    SHARING_LOGD("trace.");
    fd = -1;
    if (socketType != SOCK_STREAM && socketType != SOCK_DGRAM) {
        SHARING_LOGE("type error: %{public}d!", socketType);
    }

    fd = socket(AF_INET, socketType, (socketType == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP));
    if (fd < 0) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }
    SHARING_LOGD("success fd: %{public}d.", fd);
    return true;
}

bool SocketUtils::BindSocket(int32_t fd, const std::string &host, uint16_t port)
{
    SHARING_LOGD("trace.");
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (host == "" || host == "::") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            SHARING_LOGE("error: %{public}s!", strerror(errno));
            return false;
        }
    }
    if (::bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::ListenSocket(int32_t fd, uint32_t backlog)
{
    SHARING_LOGD("trace.");
    if (::listen(fd, backlog) == -1) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::ConnectSocket(int32_t fd, bool isAsync, const std::string &ip, uint16_t port, int32_t &ret)
{
    SHARING_LOGD("trace.");
    if (ip == "") {
        SHARING_LOGE("ip null!");
        return false;
    }

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        SHARING_LOGE("inet_pton ip error: %{public}s!", strerror(errno));
        return false;
    }

    int32_t res = ::connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    ret = res;
    if (isAsync) {
        if (res == 0) {
            SHARING_LOGD("connect immediately.");
            return true;
        } else {
            if (errno == EINPROGRESS) {
                SHARING_LOGD("connecting.");
                return true;
            } else {
                return false;
            }
        }
    } else {
        if (res == 0) {
            return true;
        } else {
            return false;
        }
    }
}

void SocketUtils::ShutDownSocket(int32_t fd)
{
    SHARING_LOGD("trace.");
    if (fd >= 0) {
        SHARING_LOGD("shutdown fd: %{public}d.", fd);
        shutdown(fd, SHUT_RDWR);
    }
}

bool SocketUtils::SetReuseAddr(int32_t fd, bool isReuse)
{
    SHARING_LOGD("trace.");
    int32_t on = isReuse ? 1 : 0;
    if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::SetReusePort(int32_t fd, bool isReuse)
{
    SHARING_LOGD("trace.");
    int32_t on = isReuse ? 1 : 0;
    if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on))) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::SetCloseWait(int32_t fd, int32_t second)
{
    SHARING_LOGD("trace.");
    linger sLinger;
    sLinger.l_onoff = (second > 0);
    sLinger.l_linger = second;
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &sLinger, sizeof(linger)) == -1) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::SetCloExec(int32_t fd, bool isOn)
{
    SHARING_LOGD("trace.");

    int32_t flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        SHARING_LOGE("fcntl error: %{public}s!", strerror(errno));
        return false;
    }

    if (isOn) {
        flags |= FD_CLOEXEC;
    } else {
        int32_t cloexec = FD_CLOEXEC;
        flags &= ~cloexec;
    }

    if (fcntl(fd, F_SETFD, flags) == -1) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::SetNoDelay(int32_t fd, bool isOn)
{
    SHARING_LOGD("trace.");
    int32_t on = isOn;
    if (0 != setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on))) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

void SocketUtils::SetKeepAlive(int32_t sockfd)
{
    SHARING_LOGD("trace.");
    int32_t on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));
}

bool SocketUtils::SetNonBlocking(int32_t fd, bool isNonBlock, uint32_t write_timeout)
{
    SHARING_LOGD("trace.");
    int32_t flags = -1;
    if (isNonBlock) {
        flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    } else {
        flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
        if (write_timeout > 0) {
            struct timeval tv = {write_timeout / 1000, (write_timeout % 1000) * 1000};
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof tv);
        }
    }

    return (flags != -1) ? true : false;
}

bool SocketUtils::SetSendBuf(int32_t fd, int32_t size)
{
    SHARING_LOGD("trace.");
    if (0 != setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size))) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

bool SocketUtils::SetRecvBuf(int32_t fd, int32_t size)
{
    SHARING_LOGD("trace.");
    if (0 != setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size))) {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        return false;
    }

    return true;
}

void SocketUtils::CloseSocket(int32_t fd)
{
    SHARING_LOGD("trace.");
    if (fd >= 0) {
        SHARING_LOGD("close fd: %{public}d.", fd);
        close(fd);
    }
}

int32_t SocketUtils::SendSocket(int32_t fd, const char *buf, int32_t len)
{
    SHARING_LOGD("trace.");
    if (fd < 0 || buf == NULL || len == 0) {
        return -1;
    }
    SHARING_LOGD("sendSocket: \r\n%{public}s", buf);
    int32_t bytes = 0;
    while (true) {
        if (bytes >= len || bytes < 0) {
            break;
        }

        int32_t retCode = send(fd, &buf[bytes], len - bytes, 0);
        if ((retCode < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
            SHARING_LOGD("sendSocket: continue.");
            break;
        } else if (retCode > 0) {
            bytes += retCode;
            if (bytes == len) {
                break;
            }
        } else {
            SHARING_LOGE("error: %{public}s!", strerror(errno));
            bytes = 0;
            break;
        }
    }

    SHARING_LOGD("finish! fd: %{public}d size: %{public}d.", fd, bytes);
    return bytes;
}

int32_t SocketUtils::Sendto(int32_t fd, const char *buf, size_t len, const char *ip, int32_t nPort)
{
    SHARING_LOGD("trace: \r\n%{public}s.", buf);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nPort);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        SHARING_LOGE("inet_pton error: %{public}s!", strerror(errno));
        return -1;
    }
    int32_t retCode = sendto(fd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (retCode < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
    } else if (retCode > 0) {
    } else {
        SHARING_LOGE("sendto error: %{public}s!", strerror(errno));
        retCode = 0;
    }

    return retCode;
}

int32_t SocketUtils::ReadSocket(int32_t fd, char *buf, uint32_t len, int32_t &error)
{
    SHARING_LOGD("trace.");
    if (fd < 0 || buf == NULL || len == 0) {
        SHARING_LOGE("invalid param!");
        return -1;
    }

    int32_t retCode = read(fd, buf, len);
    error = errno;
    if (retCode < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
    } else if (retCode > 0) {
    } else {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        retCode = 0;
    }

    return retCode;
}

int32_t SocketUtils::ReadSocket(int32_t fd, DataBuffer::Ptr buf, int32_t &error)
{
    SHARING_LOGD("trace.");
    if (fd < 0 || !buf) {
        SHARING_LOGE("readSocket:invalid param!");
        return -1;
    }

    auto size = buf->Capacity() - buf->Size();
    if (size < READ_BUF_SIZE) {
        uint32_t bufferReaderSize = buf->Size();
        if (bufferReaderSize > MAX_READ_BUF_SIZE) {
            SHARING_LOGE("error data size!");
            return -1;
        }
        buf->Resize(bufferReaderSize + READ_BUF_SIZE);
    }

    int32_t bytesRead = read(fd, buf->Data() + buf->Size(), READ_BUF_SIZE);
    error = errno;
    if (bytesRead > 0) {
        buf->UpdateSize(buf->Size() + bytesRead);
    } else if (bytesRead < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
    } else {
        bytesRead = 0;
        SHARING_LOGE("error: %{public}s!", strerror(errno));
    }

    return bytesRead;
}

int32_t SocketUtils::RecvSocket(int32_t fd, char *buf, uint32_t len, int32_t flags, int32_t &error)
{
    SHARING_LOGD("trace.");
    if (fd < 0 || buf == NULL || len == 0) {
        SHARING_LOGE("invalid param.");
        return -1;
    }

    int32_t retCode = recv(fd, buf, len, flags);
    error = errno;
    if (retCode < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
    } else if (retCode > 0) {
    } else {
        SHARING_LOGE("error: %{public}s!", strerror(errno));
        retCode = 0;
    }

    return retCode;
}

int32_t SocketUtils::AcceptSocket(int32_t fd, struct sockaddr_in *clientAddr, socklen_t *addrLen)
{
    SHARING_LOGD("trace.");
    int32_t clientFd = accept(fd, (struct sockaddr *)clientAddr, addrLen);
    if (clientFd < 0) {
        SHARING_LOGE("accept error: %{public}s!", strerror(errno));
    }

    return clientFd;
}

bool SocketUtils::GetIpPortInfo(int32_t fd, std::string &strLocalAddr, std::string &strRemoteAddr, uint16_t &localPort,
                                uint16_t &remotePort)
{
    SHARING_LOGD("trace.");
    struct sockaddr_in localAddr;
    socklen_t localAddrLen = sizeof(localAddr);
    if (-1 == getsockname(fd, (struct sockaddr *)&localAddr, &localAddrLen)) {
        SHARING_LOGE("getsockname error: %{public}s!", strerror(errno));
        return false;
    }
    strLocalAddr = inet_ntoa(localAddr.sin_addr);
    localPort = ntohs(localAddr.sin_port);
    SHARING_LOGD("localAddr: %{public}s localPort: %{public}d.", strLocalAddr.c_str(), localPort);
    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen = sizeof(remoteAddr);
    if (-1 == getpeername(fd, (struct sockaddr *)&remoteAddr, &remoteAddrLen)) {
        SHARING_LOGE("getpeername error: %{public}s!", strerror(errno));
        return false;
    }

    strRemoteAddr = inet_ntoa(remoteAddr.sin_addr);
    remotePort = ntohs(remoteAddr.sin_port);
    SHARING_LOGD("remoteAddr: %{public}s remotePort: %{public}d.", strRemoteAddr.c_str(), remotePort);
    return true;
}
} // namespace Sharing
} // namespace OHOS