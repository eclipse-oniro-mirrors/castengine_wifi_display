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

#ifndef OHOS_SHARING_HISYSEVENT_H
#define OHOS_SHARING_HISYSEVENT_H

#include <string>
#include "common/event_comm.h"
#include "wifi_device.h"
#include "wifi_p2p.h"
#include "hisysevent.h"

namespace OHOS {
namespace Sharing {

constexpr char DOMAIN_WIFI_DISPLAY[] = "WIFI_DISPLAY";
inline const std::string EVENT_NAME = "MIRACAST_PROJECTION_BEHAVIOR";
inline const std::string PKG_SHARING = "sharing_service";
inline const std::string HOST_PKG_SHARING = "cast_engine_service";
const HiviewDFX::HiSysEvent::EventType EVENT_TYPE = HiviewDFX::HiSysEvent::EventType::BEHAVIOR;

enum class BizScene : int32_t {
    DISCOVERY = 1,
    CONNECTION = 2,
    DISCONNECTION = 3,
};

enum class DiscoveryStage : int32_t {
    S1_START_DISCOVERY = 1,
    S2_FIRST_DEVICE = 2,
    S3_STOP_DISCOVERY = 3,
};

enum class ConnectionStage : int32_t {
    S1_P2P_CONNECTION = 1,
    S2_TCP_SERVER = 2,
    S3_M1_STAGE = 3,
    S4_M2_STAGE = 4,
    S5_M3_STAGE = 5,
    S6_M4_STAGE = 6,
    S7_M5_STAGE = 7,
    S8_M6_STAGE = 8,
    S9_M7_STAGE = 9,
    S10_CREATE_ENCODER = 10,
    S11_VTP_OR_UDP_CONNECTION = 11,
    S12_SEND_STREAM = 12,
};

enum class DisconnectionStage : int32_t {
    S1_START_DISCONNECTION = 1,
    S2_DISCONNECTION_COMPLETED = 2,
};

enum class StageResType : int32_t {
    STAGE_RES_IDLE = 0,
    STAGE_RES_SUCCESS,
    STAGE_RES_FAILED,
    STAGE_RES_CANCEL,
    STAGE_RES_UNKNOWN
};

enum class BIZStateType : int32_t {
    BIZ_STATE_BEGIN = 1,
    BIZ_STATE_END = 2,
    BIZ_STATE_CANCEL = 3,
};

class SharingHisysevent {
public:
    SharingHisysevent(const SharingHisysevent &) = delete;
    SharingHisysevent &operator=(const SharingHisysevent &) = delete;

    static SharingHisysevent &GetInstance()
    {
        static SharingHisysevent instance{};
        return instance;
    }

    struct HisyseventInfo {
        std::string func = "";
        std::string toCallpkg = "";
        ConnectionStage stage = ConnectionStage::S1_P2P_CONNECTION;
        StageResType stageRes = StageResType::STAGE_RES_UNKNOWN;
        std::string deviceName = "";
        std::string deviceType = "";
    };

    bool GetWifiLinkedInfo(Wifi::WifiLinkedInfo& wifiInfo);
    bool GetP2pGroupInfo(Wifi::WifiP2pGroupInfo& groupInfo);
    void ReportFrequency(const HisyseventInfo& hisyseventInfo);

private:
    SharingHisysevent() = default;
    ~SharingHisysevent() = default;
};

} // namespace Sharing
} // namespace OHOS
#endif