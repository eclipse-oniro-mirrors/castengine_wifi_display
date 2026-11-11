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

#include "sharing_log.h"
#include "sharing_hisysevent.h"
#include "securec.h"

namespace OHOS {
namespace Sharing {

bool SharingHisysevent::GetWifiLinkedInfo(Wifi::WifiLinkedInfo& wifiInfo)
{
    std::shared_ptr<Wifi::WifiDevice> wifiDevicePtr = Wifi::WifiDevice::GetInstance(WIFI_DEVICE_ABILITY_ID);
    if (wifiDevicePtr == nullptr) {
        SHARING_LOGE("get wifidevice failed.");
        return false;
    }
    bool isActive = false;
    wifiDevicePtr->IsWifiActive(isActive);
    if (!isActive) {
        SHARING_LOGE("wifi is not active.");
        return false;
    }
    ErrCode ret = wifiDevicePtr->GetLinkedInfo(wifiInfo);
    if (ret != Wifi::WIFI_OPT_SUCCESS) {
        SHARING_LOGE("get linked info failed ret = %{public}d.", ret);
        return false;
    }
    return true;
}

bool SharingHisysevent::GetP2pGroupInfo(Wifi::WifiP2pGroupInfo& groupInfo)
{
    std::shared_ptr<Wifi::WifiP2p> wifiP2pPtr = Wifi::WifiP2p::GetInstance(WIFI_P2P_ABILITY_ID);
    if (wifiP2pPtr == nullptr) {
        SHARING_LOGE("get wifip2p failed.");
        return false;
    }
    ErrCode ret = wifiP2pPtr->GetCurrentGroup(groupInfo);
    if (ret != Wifi::WIFI_OPT_SUCCESS) {
        SHARING_LOGE("get p2p info failed ret = %{public}d.", ret);
        return false;
    }
    return true;
}

void SharingHisysevent::ReportFrequency(const HisyseventInfo& hisyseventInfo)
{
    int32_t groupOwner = -1;
    int32_t wifiFrequency = 0;
    int32_t p2pFrequency = 0;
    Wifi::WifiLinkedInfo wifiInfo;
    if (GetWifiLinkedInfo(wifiInfo)) {
        wifiFrequency = wifiInfo.frequency;
    }
    Wifi::WifiP2pGroupInfo groupInfo;
    if (GetP2pGroupInfo(groupInfo)) {
        groupOwner = groupInfo.IsGroupOwner() ? 0 : 1;
        p2pFrequency = groupInfo.GetFrequency();
    }
    SHARING_LOGI("groupOwner: %{public}d, wifiFrequency: %{public}d, p2pFrequency: %{public}d", groupOwner,
                 wifiFrequency, p2pFrequency);
    HiSysEventWrite(
        DOMAIN_WIFI_DISPLAY,
        EVENT_NAME,
        EVENT_TYPE,
        "ORG_PKG", PKG_SHARING,
        "HOST_PKG", HOST_PKG_SHARING,
        "TO_CALL_PKG", hisyseventInfo.toCallpkg,
        "FUNC", hisyseventInfo.func,
        "BIZ_SCENE", static_cast<int32_t>(BizScene::CONNECTION),
        "BIZ_STAGE", static_cast<int32_t>(hisyseventInfo.stage),
        "STAGE_RES", static_cast<int32_t>(hisyseventInfo.stageRes),
        "BIZ_STATE", static_cast<int32_t>(BIZStateType::BIZ_STATE_BEGIN),
        "PEER_DEV_VER", hisyseventInfo.deviceName,
        "PEER_DEV_TYPE", hisyseventInfo.deviceType,
        "GROUP_OWNER", groupOwner,
        "WIFI_FREQ_START", wifiFrequency,
        "P2P_FREQ_START", p2pFrequency);
}

} // namespace Sharing
} // namespace OHOS