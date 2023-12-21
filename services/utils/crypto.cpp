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

#include "crypto.h"
#include <iomanip>
#include <sstream>
// #include "openssl/md5.h"     // TODO: fix this

namespace OHOS {
namespace Sharing {

std::string GetMD5(const std::string &src)
{
    int MD5_DIGEST_LENGTH = 4; // TODO: fix this
    unsigned char MD5Hash[MD5_DIGEST_LENGTH];
    std::string MD5Digest;
    std::string tmp;
    std::stringstream ss;

    // TODO: fix this
    // MD5((const unsigned char *)src.c_str(), src.size(), MD5Hash);

    for (int32_t i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int32_t)MD5Hash[i] << std::endl;
        ss >> tmp;
        MD5Digest += tmp;
    }

    return MD5Digest;
}

} // namespace Sharing
} // namespace OHOS