/*
 * Copyright (c) 2024 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "wfd_utils.h"
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "sharing_log.h"
#include "utils/crypto.h"
#include "utils/utils.h"

namespace OHOS {
namespace Sharing {

constexpr int32_t HEX_TO_UINT8 = 2;
constexpr int WIDTH = 4;
constexpr unsigned char MASK = 0x0F;
const std::string TYPE_DEVICE_ID = "0x02";
const std::string TYPE_DEVICE_ID_CASE = "0x03";
const std::string TYPE_DEVICE_CAPABILITY = "0x42";
const std::string OUI = "0x00e0fc";
const std::string ADDRESS_KEY = "address";
const int BYTE_HEX_LEN = 2;
const int OUI_LEN = 3;
const int MAC_STRING_LENGTH = 17;
const int WFD_DEVICE_INFO_LENGTH = 6;
const int HW_DEVICE_MIN_IE_LENGTH = 6;
const int HEX_TO_BYTE = 4;
const int PARSE_FAIL = -1;
const int PARSE_OK = 0;
const int TYPE_BYTE_LEN = 1;
const int BYTE_MASK_FF = 0xFF;
const int SOFTBUS_UDID_HASH_STRLEN = 16;
const uint8_t FEATURE_ID = 0x81;
const uint8_t HWMARK = 0xdd;
const std::vector<uint8_t> ID_CASE_MASK = {0x08, 0x04, 0x02, 0x01};

std::string ByteToHexStr(const std::vector<uint8_t> &data, uint32_t pos, uint32_t len, bool isNeed0x)
{
    std::stringstream ss;
    if (isNeed0x) {
        ss << "0x" << std::hex << std::setfill('0');
    } else {
        ss << std::hex << std::setfill('0');
    }

    for (uint32_t i = pos; (i < (pos + len)) && (i < data.size()); i++) {
        ss << std::setw(BYTE_HEX_LEN) << static_cast<int>(data[i]);
    }
    return ss.str();
}

uint8_t CharToHex(char c)
{
    int32_t ret;
    std::stringstream ss;
    ss << c;
    ss >> std::hex >> ret;
    return static_cast<uint8_t>(ret);
}

std::string GetUdidHash(std::string &udid)
{
    if (udid.empty()) {
        return "";
    }
    uint32_t udidLen = udid.size();
    std::vector<uint8_t> hashIdResult = GenerateSha256HashId(reinterpret_cast<const uint8_t *>(udid.c_str()), udidLen);
    if (hashIdResult.empty()) {
        return "";
    }
    std::string ret = ByteToHexStr(hashIdResult, 0, hashIdResult.size(), false);
    return ret;
}

std::string UpdateDeviceIdCase(std::map<std::string, std::string> &infoMap)
{
    if (infoMap.empty() || !infoMap.count(TYPE_DEVICE_ID) || !infoMap.count(TYPE_DEVICE_ID_CASE)) {
        SHARING_LOGE("invalid infoMap");
        return "";
    }
    std::string udid = infoMap[TYPE_DEVICE_ID];
    std::string udidCaseIdx = infoMap[TYPE_DEVICE_ID_CASE];
    if (udid.size() != udidCaseIdx.size() * HEX_TO_BYTE) {
        SHARING_LOGE("invalid data.");
        return "";
    }
    std::string updatedId = "";
    int count = 0;

    for (uint32_t j = 0; j < udidCaseIdx.size(); j++) {
        for (uint32_t i = 0; i < ID_CASE_MASK.size(); i++) {
            uint8_t index = CharToHex(udidCaseIdx[j]);
            if ((index & ID_CASE_MASK[i]) != 0) {
                updatedId += std::toupper(udid[count]);
            } else {
                updatedId += udid[count];
            }
            count++;
        }
    }

    return updatedId;
}

std::string GetSoftbusUdidHash(std::string &udid)
{
    if (udid.empty()) {
        return "";
    }
    if (udid.size() <= SOFTBUS_UDID_HASH_STRLEN) {
        return udid;
    }
    std::string udidHash = GetUdidHash(udid);
    if (udidHash.empty() || udidHash.size() < SOFTBUS_UDID_HASH_STRLEN) {
        return "";
    }
    return udidHash.substr(0, SOFTBUS_UDID_HASH_STRLEN);
}

// The whole extra data includes fix-len parts(12 bytes) and tlv parts
bool IsExtraDataInvalid(std::vector<uint8_t> &extra, uint32_t &position, uint32_t &actulTotalLen)
{
    if (extra.size() < WFD_DEVICE_INFO_LENGTH + HW_DEVICE_MIN_IE_LENGTH) {
        SHARING_LOGE("parse hw device tlv length wrong");
        return false;
    }

    position = WFD_DEVICE_INFO_LENGTH;
    if (extra[position++] != HWMARK) {
        SHARING_LOGE("HWMARK is not equal, receive HWMARK is:%{public}x ", extra[position - 1]);
        return false;
    }

    uint32_t len = extra[position++];
    actulTotalLen = len + WFD_DEVICE_INFO_LENGTH + BYTE_HEX_LEN;
    if (actulTotalLen > extra.size()) {
        SHARING_LOGE("array index will out of bounds");
        return false;
    }

    std::string strOui = ByteToHexStr(extra, position, OUI_LEN, true);
    if (strOui.compare(OUI) != 0) {
        SHARING_LOGE("OUI is not equal, receive OUI is %s", strOui.c_str());
        return false;
    }
    position += OUI_LEN;

    if (extra[position++] != FEATURE_ID) {
        SHARING_LOGE("FEATURE_ID is not equal, receive FEATURE_ID is :%x\n", extra[position - 1]);
        return false;
    }

    return true;
}

std::map<std::string, std::string> ParseHwDeviceTLV(std::vector<uint8_t> &extra)
{
    std::map<std::string, std::string> infoMap;
    uint32_t position = 0;
    uint32_t totalLength = 0;
    if (!IsExtraDataInvalid(extra, position, totalLength)) {
        SHARING_LOGE("extra data has invalid element");
        return infoMap;
    }

    while (position < totalLength - 1) {
        std::string type = ByteToHexStr(extra, position++, TYPE_BYTE_LEN, true);
        uint32_t length = extra[position++] & BYTE_MASK_FF;
        if (position + length > totalLength) {
            SHARING_LOGE("extra array length is less than what we need, parse IE tlv info error");
            std::map<std::string, std::string> tmp;
            return tmp;
        }

        std::string value = ByteToHexStr(extra, position, length, false);
        infoMap.emplace(type, value);
        position += length; // now position represents the position of next type
    }

    std::string typeDeviceId = UpdateDeviceIdCase(infoMap);
    if (!typeDeviceId.empty()) {
        infoMap[TYPE_DEVICE_ID] = typeDeviceId;
    }

    return infoMap;
}

int32_t ParseHwDeviceInfo(const std::string &wfdInfo, WfdCastDeviceInfo &castDevInfo)
{
    SHARING_LOGI("parseHwDeviceInfo in, info size is: %{public}ld", wfdInfo.size());
    if (wfdInfo.size() < MAC_STRING_LENGTH + WFD_DEVICE_INFO_LENGTH + HW_DEVICE_MIN_IE_LENGTH) {
        SHARING_LOGE("wfdInfo info length should be more than 29");
        return PARSE_FAIL;
    }

    std::string devMac = wfdInfo.substr(0, MAC_STRING_LENGTH);
    std::vector<uint8_t> extraInfo(wfdInfo.begin() + MAC_STRING_LENGTH, wfdInfo.end());
    std::map<std::string, std::string> extraInfoMap = ParseHwDeviceTLV(extraInfo);
    if (extraInfoMap.empty()) {
        SHARING_LOGE("IE info is empty");
        return PARSE_FAIL;
    }
    if (extraInfoMap.find(TYPE_DEVICE_ID) == extraInfoMap.end()) {
        SHARING_LOGE("udid is empty");
        return PARSE_FAIL;
    }

    if (extraInfoMap.find(TYPE_DEVICE_CAPABILITY) == extraInfoMap.end()) {
        SHARING_LOGE("capability is empty");
        return PARSE_FAIL;
    }

    castDevInfo.primaryDeviceType = extraInfoMap[TYPE_DEVICE_CAPABILITY];
    castDevInfo.deviceId = devMac;
    castDevInfo.udidHash = GetSoftbusUdidHash(extraInfoMap[TYPE_DEVICE_ID]);
    SHARING_LOGI("parseHwDeviceInfo succ, hash ret is %{public}s", GetAnonymousDeviceId(castDevInfo.udidHash).c_str());
    return PARSE_OK;
}

std::string GetAddressHash(const std::string &address)
{
    std::string addressStr = std::string(address);
    return GetUdidHash(addressStr);
}

std::string Sha256(const std::string &text, bool isUpper)
{
    return Sha256(text.data(), text.size(), isUpper);
}

std::string Sha256(const void *data, size_t size, bool isUpper)
{
    if (data == nullptr || size == 0) {
        SHARING_LOGI("data or size is invalid");
        return "";
    }
    unsigned char hash[SHA256_DIGEST_LENGTH * HEX_TO_UINT8 + 1] = "";
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, size);
    SHA256_Final(&hash[SHA256_DIGEST_LENGTH], &ctx);
    // here we translate sha256 hash to hexadecimal. each 8-bit char will be presented by two characters([0-9a-f])
    const char* hexCode = isUpper ? "0123456789ABCDEF" : "0123456789abcdef";
    for (int32_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        unsigned char value = hash[SHA256_DIGEST_LENGTH + i];
        // uint8_t is 2 digits in hexadecimal.
        hash[i * HEX_TO_UINT8] = hexCode[(value >> WIDTH) & MASK];
        hash[i * HEX_TO_UINT8 + 1] = hexCode[value & MASK];
    }
    hash[SHA256_DIGEST_LENGTH * HEX_TO_UINT8] = 0;
    std::stringstream ss;
    ss << hash;
    return ss.str();
}
} // namespace Sharing
} // namespace OHOS
