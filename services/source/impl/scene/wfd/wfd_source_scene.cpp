/*
 * Copyright (c) 2023 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#include "wfd_source_scene.h"
#include <display_manager.h>
#include <unistd.h>
#include "common/common_macro.h"
#include "common/const_def.h"
#include "common/reflect_registration.h"
#include "sharing_hisysevent.h"
#include "common/sharing_log.h"
#include "configuration/include/config.h"
#include "extend/magic_enum/magic_enum.hpp"
#include "network/socket/socket_utils.h"
#include "screen_capture_def.h"
#include "utils/utils.h"
#include "source_session_def.h"
#include "wfd_utils.h"
#include "screen_capture_def.h"
#include "device_manager.h"
#include "dm_device_info.h"
#include "dm_constants.h"
#include "kits/c/wifi_hid2d.h"
#include "interaction/device_kit/dm_kit.h"
#include "json.hpp"
#include "system_ability_definition.h"
#include "iservice_registry.h"
#include "mem_mgr_client.h"
#include "mem_mgr_proxy.h"

using namespace OHOS::DistributedHardware;
using nlohmann::json;
namespace OHOS {
namespace Sharing {

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN SHARING_LOG_DOMAIN
#define LOG_TAG SHARING_LOG_TAG

constexpr int SUB_LEN = 1;
constexpr int SUB_INDEX = 7;
constexpr int ID_LEN = 16;

// The most inclination to be a group owner
constexpr int GROUP_OWNER_INTENT_MAX = 15;
constexpr int TYPE_CAST = 1 << 6;
constexpr int HEX = 16;
constexpr int EVENT_SCREEN_PARAM = 1081;

void WfdSourceScene::WfdP2pCallback::SetScene(std::weak_ptr<WfdSourceScene> scene)
{
    scene_ = scene;
}

void WfdSourceScene::WfdP2pCallback::OnP2pStateChanged(int32_t state)
{
    HILOG_COMM_INFO("state: %{public}d.", state);
    auto scene = scene_.lock();
    if (scene) {
        switch (static_cast<Wifi::P2pState>(state)) {
            case Wifi::P2pState::P2P_STATE_NONE:
                break;
            case Wifi::P2pState::P2P_STATE_IDLE:
                break;
            case Wifi::P2pState::P2P_STATE_STARTING:
                break;
            case Wifi::P2pState::P2P_STATE_STARTED:
                if (scene->isSourceRunning_) {
                    scene->WfdP2pStart();
                }
                break;
            case Wifi::P2pState::P2P_STATE_CLOSING:
                break;
            case Wifi::P2pState::P2P_STATE_CLOSED:
                if (scene->isSourceRunning_) {
                    scene->isSourceRunning_ = false;
                    scene->WfdP2pStop();
                    scene->OnInnerError(0, 0, SharingErrorCode::ERR_P2P_DISCONNECTED_STATE_CLOSED,
                                        "P2P  Disconnected MODULE STOPPED");
                }
                break;
            default:
                HILOG_COMM_ERROR("none process case.");
                break;
        }
    }
}

void WfdSourceScene::WfdP2pCallback::OnP2pPersistentGroupsChanged(void)
{
    SHARING_LOGE("%{public}s.", __FUNCTION__);
}

void WfdSourceScene::WfdP2pCallback::OnP2pThisDeviceChanged(const Wifi::WifiP2pDevice &device)
{
    HILOG_COMM_INFO("OnP2pThisDeviceChanged");
    auto scene = scene_.lock();
    if (scene == nullptr) {
        HILOG_COMM_ERROR("scene is nullptr.");
        return;
    }

    auto p2p = scene->GetP2pInstance();
    if (!p2p) {
        HILOG_COMM_ERROR("p2pInstance is nullptr.");
        return;
    }

    Wifi::P2pChrEvent p2pEvent = device.GetChrErrCode();
    HILOG_COMM_INFO("p2pEvent: %{public}d.", p2pEvent);
    if (p2pEvent == Wifi::P2pChrEvent::GO_NEGOTIATION_PEER_REJECT) {
        scene->OnInnerError(0, 0, SharingErrorCode::ERR_P2P_GO_NEGOTIATION_REJECT,
            "P2P Go Negotiation reject");
    } else if (p2pEvent == Wifi::P2pChrEvent::GO_NEGOTIATION_WAIT_PEER_READY_TIMEOUT) {
        scene->OnInnerError(0, 0, SharingErrorCode::ERR_P2P_GO_NEGOTIATION_TIMEOUT,
            "P2P Go Negotiation timeout");
    }

    Wifi::WifiP2pLinkedInfo info;
    if (p2p->QueryP2pLinkedInfo(info)) {
        HILOG_COMM_ERROR("failed to query p2p link info.");
        return;
    }

    Wifi::P2pConnectedState state = info.GetConnectState();
    HILOG_COMM_INFO("ConnectState: %{public}d.", state);
    if (state != Wifi::P2pConnectedState::P2P_DISCONNECTED) {
        return;
    }
    scene->OnP2pDeviceDisconnectedChanged();
}

void WfdSourceScene::WfdP2pCallback::OnP2pPeersChanged(const std::vector<Wifi::WifiP2pDevice> &device)
{
    HILOG_COMM_INFO("OnP2pPeersChanged");
    auto scene = scene_.lock();
    RETURN_IF_NULL(scene);

    if (!scene->isSourceDiscovering) {
        HILOG_COMM_ERROR("p2p source is not discovering.");
        return;
    }

    HILOG_COMM_INFO("device size: %{public}zu.", device.size());
    std::vector<WfdCastDeviceInfo> foundedDevices;
    for (auto itDev : device) {
        auto status = itDev.GetP2pDeviceStatus();
        HILOG_COMM_INFO("device name: %{public}s, mac: %{public}s, status: %{public}d.",
                        GetAnonyString(itDev.GetDeviceName()).c_str(),
                        GetAnonyString(itDev.GetDeviceAddress()).c_str(), status);
        if (status == Wifi::P2pDeviceStatus::PDS_AVAILABLE) {
            std::string subelement;
            Wifi::WifiP2pWfdInfo wfdInfo(itDev.GetWfdInfo());
            wfdInfo.GetDeviceInfoElement(subelement);
            if (subelement.empty() || subelement.length() < ID_LEN) {
                HILOG_COMM_ERROR("device id is empty or len not right.");
                return;
            }
            HILOG_COMM_INFO("session available: %{public}d, device info: %{public}s, subelement substr: %{public}s",
                            wfdInfo.isSessionAvailable(), GetAnonyString(subelement).c_str(),
                            subelement.substr(SUB_INDEX, SUB_LEN).c_str());
            if (wfdInfo.isSessionAvailable() && subelement.substr(SUB_INDEX, SUB_LEN) != "0") {
                WfdCastDeviceInfo deviceInfo;
                deviceInfo.deviceId = itDev.GetDeviceAddress();
                deviceInfo.deviceName = itDev.GetDeviceName();
                deviceInfo.primaryDeviceType = itDev.GetPrimaryDeviceType();
                {
                    std::lock_guard<std::mutex> lock(macCastPlusMutex_);
                    deviceInfo.secondaryDeviceType =
                        macCastPlusMap_.count(deviceInfo.deviceId) ? "CastPlus" : "Other";
                }
                {
                    std::lock_guard<std::mutex> lock(macMapMutex_);
                    deviceInfo.udidHash =
                        macUdidMap_.count(deviceInfo.deviceId) ? macUdidMap_[deviceInfo.deviceId] : "";
                }
                HILOG_COMM_INFO("udidHash: %{public}s.", GetAnonyString(deviceInfo.udidHash).c_str());
                foundedDevices.emplace_back(deviceInfo);
            }
        }
    }

    scene->OnDeviceFound(foundedDevices);
}

void WfdSourceScene::WfdP2pCallback::OnP2pPrivatePeersChanged(const std::string &priWfdInfo)
{
    HILOG_COMM_INFO("OnP2pPrivatePeersChanged");
    WfdCastDeviceInfo deviceInfo;
    if (!ParseHwDeviceInfo(priWfdInfo, deviceInfo)) {
        if (!deviceInfo.primaryDeviceType.empty()) {
            errno = 0;
            char *endPtr = nullptr;
            int capability = std::strtol(deviceInfo.primaryDeviceType.c_str(), &endPtr, HEX);
            if (errno == ERANGE || endPtr == deviceInfo.primaryDeviceType.c_str() || *endPtr != '\0') {
                HILOG_COMM_ERROR("%{public}s.errno:%{public}d", __FUNCTION__, errno);
                return;
            }
            HILOG_COMM_INFO("ParseHwDeviceInfo mac is %{public}s, udidHash is %{public}s.",
                            GetAnonyString(deviceInfo.deviceId).c_str(), GetAnonyString(deviceInfo.udidHash).c_str());
            {
                std::lock_guard<std::mutex> lock(macMapMutex_);
                macUdidMap_[deviceInfo.deviceId] = deviceInfo.udidHash;
            }
            uint32_t capability_uint = (uint32_t)capability;
            uint32_t type_cast_uint = (uint32_t)TYPE_CAST;
            HILOG_COMM_INFO("capability = %{public}d", capability);
            if (capability_uint & type_cast_uint) {
                std::lock_guard<std::mutex> lock(macCastPlusMutex_);
                macCastPlusMap_[deviceInfo.deviceId] = "CastPlus";
            }
        }
    } else {
        HILOG_COMM_ERROR("ParseHwDeviceInfo fail");
    }
}

void WfdSourceScene::WfdP2pCallback::OnP2pServicesChanged(const std::vector<Wifi::WifiP2pServiceInfo> &srvInfo)
{
    SHARING_LOGE("%{public}s.", __FUNCTION__);
}

Wifi::WifiP2pDevice WfdSourceScene::WfdP2pCallback::GetGoDevice(
    const Wifi::WifiP2pGroupInfo &group, const std::shared_ptr<WfdSourceScene> &scene)
{
    Wifi::WifiP2pDevice goDevice;
    if (group.IsGroupOwner()) {
        scene->isGroupOwner_ = true;
        if (group.IsClientDevicesEmpty()) {
            HILOG_COMM_ERROR("GetClientDevices failed");
            return goDevice;
        }
        std::string connMac;
        {
            std::lock_guard<std::mutex> lock(scene->mutex_);
            if (scene->connDev_ == nullptr) {
                SHARING_LOGE("connDev_ is nullptr.");
                return goDevice;
            }
            connMac = scene->connDev_->mac;
        }
        for (int i = 0; i < (int) group.GetClientDevices().size(); i++) {
            if (connMac == group.GetClientDevices()[i].GetDeviceAddress()) {
                goDevice = group.GetClientDevices()[i];
                break;
            }
        }
        if (connMac != goDevice.GetDeviceAddress()) {
            HILOG_COMM_ERROR("can't find device in group");
            goDevice = Wifi::WifiP2pDevice();
        }
    } else {
        scene->isGroupOwner_ = false;
        goDevice = group.GetOwner();
    }
    return goDevice;
}

void WfdSourceScene::WfdP2pCallback::NotifyPeerConnected(
    const Wifi::WifiP2pLinkedInfo &info, const Wifi::WifiP2pDevice &goDevice)
{
    ConnectionInfo connectionInfo;
    connectionInfo.ip = info.GetGroupOwnerAddress();
    connectionInfo.mac = goDevice.GetDeviceAddress();
    connectionInfo.deviceName = goDevice.GetDeviceName();
    connectionInfo.primaryDeviceType = goDevice.GetPrimaryDeviceType();
    connectionInfo.ctrlPort = DEFAULT_WFD_CTRLPORT;
    connectionInfo.state = ConnectionState::CONNECTING;
    {
        std::lock_guard<std::mutex> lock(macCastPlusMutex_);
        connectionInfo.secondaryDeviceType =
            macCastPlusMap_.count(connectionInfo.mac) ? "CastPlus" : "Other";
    }
    {
        std::lock_guard<std::mutex> lock(macMapMutex_);
        connectionInfo.udidHash = macUdidMap_.count(connectionInfo.mac) ? macUdidMap_[connectionInfo.mac] : "";
    }
    auto scene = scene_.lock();
    if (scene) {
        scene->OnP2pPeerConnected(connectionInfo);
    }
    SharingHisysevent::HisyseventInfo hiSysEventInfo;
    {
        std::lock_guard<std::mutex> lock(macMapMutex_);
        hiSysEventInfo = {
            __func__,  "", ConnectionStage::S1_P2P_CONNECTION, StageResType::STAGE_RES_SUCCESS,
            connectionInfo.deviceName, macUdidMap_.count(connectionInfo.mac) ? "09C" : "000"};
    }
    SharingHisysevent::GetInstance().ReportFrequency(hiSysEventInfo);
}

void WfdSourceScene::WfdP2pCallback::OnP2pConnectionChanged(const Wifi::WifiP2pLinkedInfo &info)
{
    Wifi::P2pConnectedState state = info.GetConnectState();
    HILOG_COMM_INFO("OnP2pConnectionChanged ConnectState: %{public}d.", state);
    auto scene = scene_.lock();
    if (scene == nullptr) {
        HILOG_COMM_ERROR("scene is nullptr.");
        return;
    }
    auto p2p = scene->GetP2pInstance();
    if (!p2p) {
        SHARING_LOGE("p2pInstance is nullptr.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(scene->mutex_);
        if (scene->connDev_ == nullptr) {
            SHARING_LOGE("connDev_ is nullptr.");
            return;
        }
    }
    if (state == Wifi::P2pConnectedState::P2P_DISCONNECTED) {
        scene->OnInnerError(0, 0, SharingErrorCode::ERR_P2P_DISCONNECTED_CONNECTION_CHANGE,
                            "P2P Disconnected connection change");
        return;
    }
    if (state != Wifi::P2pConnectedState::P2P_CONNECTED) {
        return;
    }
    scene->isP2pConnected_ = true;
    if (info.GetGroupOwnerAddress().empty() || info.GetGroupOwnerAddress() == "0.0.0.0") {
        return;
    }
    Wifi::WifiP2pGroupInfo group;
    if (Wifi::ErrCode::WIFI_OPT_SUCCESS != p2p->GetCurrentGroup(group)) {
        HILOG_COMM_ERROR("GetCurrentGroup failed");
        return;
    }
    HILOG_COMM_INFO("IsGroupOwner: %{public}d, Frequency: %{public}d, IsPersistent: %{public}d",
                    group.IsGroupOwner(), group.GetFrequency(), group.IsPersistent());
    {
        std::lock_guard<std::mutex> lock(scene->mutex_);
        scene->localIp_ = GetLocalP2pAddress(group.GetInterface());
        SHARING_LOGI("OnP2pConnectionChanged, set localIp: %{public}s", GetAnonyString(scene->localIp_).c_str());
    }
    Wifi::WifiP2pDevice goDevice = GetGoDevice(group, scene);
    if (!goDevice.GetDeviceAddress().empty()) {
        NotifyPeerConnected(info, goDevice);
    }
}

void WfdSourceScene::WfdP2pCallback::OnP2pGcJoinGroup(const OHOS::Wifi::GcInfo &info) {}

void WfdSourceScene::WfdP2pCallback::OnP2pGcLeaveGroup(const OHOS::Wifi::GcInfo &info) {}

void WfdSourceScene::WfdP2pCallback::OnP2pDiscoveryChanged(bool isChange)
{
    HILOG_COMM_INFO("isChange: %{public}d.", isChange);
    if (!isChange) {
        auto scene = scene_.lock();
        RETURN_IF_NULL(scene);
        scene->OnDeviceOffline();
        scene->WfdP2pReStartDiscovery();
    }
}

std::unordered_map<Wifi::ErrCode, std::pair<SharingErrorCode, std::string>>
   WfdSourceScene::WfdP2pCallback::wifiErrorMapping_ = {
       {
           Wifi::WIFI_OPT_FAILED,
           { SharingErrorCode::ERR_P2P_OPT_FAILED, STRING_P2P_OPT_FAILED }
       },
       {
           Wifi::WIFI_OPT_NOT_SUPPORTED,
           { SharingErrorCode::ERR_P2P_OPT_NOT_SUPPORTED, STRING_P2P_OPT_NOT_SUPPORTED }
       },
       {
           Wifi::WIFI_OPT_INVALID_PARAM,
           { SharingErrorCode::ERR_P2P_OPT_INVALID_PARAM, STRING_P2P_OPT_INVALID_PARAM }
       },
       {
           Wifi::WIFI_OPT_FORBID_AIRPLANE,
           { SharingErrorCode::ERR_P2P_OPT_FORBID_AIRPLANE, STRING_P2P_OPT_FORBID_AIRPLANE }
       },
       {
           Wifi::WIFI_OPT_FORBID_POWSAVING,
           { SharingErrorCode::ERR_P2P_OPT_FORBID_POWSAVING, STRING_P2P_OPT_FORBID_POWSAVING }
       },
       {
           Wifi::WIFI_OPT_PERMISSION_DENIED,
           { SharingErrorCode::ERR_P2P_OPT_PERMISSION_DENIED, STRING_P2P_OPT_PERMISSION_DENIED }
       },
       {
           Wifi::WIFI_OPT_OPEN_FAIL_WHEN_CLOSING,
           { SharingErrorCode::ERR_P2P_OPT_OPEN_FAIL_WHEN_CLOSING, STRING_P2P_OPT_OPEN_FAIL_WHEN_CLOSING }
       },
       {
           Wifi::WIFI_OPT_OPEN_SUCC_WHEN_OPENED,
           { SharingErrorCode::ERR_P2P_OPT_OPEN_SUCC_WHEN_OPENED, STRING_P2P_OPT_OPEN_SUCC_WHEN_OPENED }
       },
       {
           Wifi::WIFI_OPT_CLOSE_FAIL_WHEN_OPENING,
           { SharingErrorCode::ERR_P2P_OPT_CLOSE_FAIL_WHEN_OPENING, STRING_P2P_OPT_CLOSE_FAIL_WHEN_OPENING }
       },
       {
           Wifi::WIFI_OPT_CLOSE_SUCC_WHEN_CLOSED,
           { SharingErrorCode::ERR_P2P_OPT_CLOSE_SUCC_WHEN_CLOSED, STRING_P2P_OPT_CLOSE_SUCC_WHEN_CLOSED }
       },
       {
           Wifi::WIFI_OPT_STA_NOT_OPENED,
           { SharingErrorCode::ERR_P2P_OPT_STA_NOT_OPENED, STRING_P2P_OPT_STA_NOT_OPENED }
       },
       {
           Wifi::WIFI_OPT_SCAN_NOT_OPENED,
           { SharingErrorCode::ERR_P2P_OPT_SCAN_NOT_OPENED, STRING_P2P_OPT_SCAN_NOT_OPENED }
       },
       {
           Wifi::WIFI_OPT_AP_NOT_OPENED,
           { SharingErrorCode::ERR_P2P_OPT_AP_NOT_OPENED, STRING_P2P_OPT_AP_NOT_OPENED }
       },
       {
           Wifi::WIFI_OPT_INVALID_CONFIG,
           { SharingErrorCode::ERR_P2P_OPT_INVALID_CONFIG, STRING_P2P_OPT_INVALID_CONFIG }
       },
       {
           Wifi::WIFI_OPT_P2P_NOT_OPENED,
           { SharingErrorCode::ERR_P2P_OPT_P2P_NOT_OPENED, STRING_P2P_OPT_P2P_NOT_OPENED }
       },
       {
           Wifi::WIFI_OPT_P2P_MAC_NOT_FOUND,
           { SharingErrorCode::ERR_P2P_OPT_P2P_MAC_NOT_FOUND, STRING_P2P_OPT_P2P_MAC_NOT_FOUND }
       },
       {
           Wifi::WIFI_OPT_P2P_ERR_MAC_FORMAT,
           { SharingErrorCode::ERR_P2P_OPT_P2P_ERR_MAC_FORMAT, STRING_P2P_OPT_P2P_ERR_MAC_FORMAT }
       },
       {
           Wifi::WIFI_OPT_P2P_ERR_INTENT,
           { SharingErrorCode::ERR_P2P_OPT_P2P_ERR_INTENT, STRING_P2P_OPT_P2P_ERR_INTENT }
       },
       {
           Wifi::WIFI_OPT_P2P_ERR_SIZE_NW_NAME,
           { SharingErrorCode::ERR_P2P_OPT_P2P_ERR_SIZE_NW_NAME, STRING_P2P_OPT_P2P_ERR_SIZE_NW_NAME }
       },
       {
           Wifi::WIFI_OPT_MOVING_FREEZE_CTRL,
           { SharingErrorCode::ERR_P2P_OPT_MOVING_FREEZE_CTRL, STRING_P2P_OPT_MOVING_FREEZE_CTRL }
       },
       {
           Wifi::WIFI_OPT_NON_SYSTEMAPP,
           { SharingErrorCode::ERR_P2P_OPT_NON_SYSTEMAPP, STRING_P2P_OPT_NON_SYSTEMAPP }
       },
       {
           Wifi::WIFI_OPT_ENTERPRISE_DENIED,
           { SharingErrorCode::ERR_P2P_OPT_ENTERPRISE_DENIED, STRING_P2P_OPT_ENTERPRISE_DENIED }
       }
};

bool WfdSourceScene::WfdP2pCallback::GetErrorCode(Wifi::ErrCode errorCode,
                                                  std::pair<SharingErrorCode, std::string> &sharingError)
{
    auto it = wifiErrorMapping_.find(errorCode);
    if (it != wifiErrorMapping_.end()) {
        sharingError = it->second;
        HILOG_COMM_INFO("wifi errorCode: %{public}d,  sharingError: %{public}d, %{public}s", errorCode,
                        sharingError.first, sharingError.second.c_str());
        return true;
    } else {
        HILOG_COMM_ERROR("Unkonw wifi errorCode: %{public}d.", errorCode);
        return false;
    }
}

void WfdSourceScene::WfdP2pCallback::OnP2pActionResult(Wifi::P2pActionCallback action, Wifi::ErrCode code)
{
    HILOG_COMM_INFO("action: %{public}hhu, code: %{public}d.", action, code);
    auto scene = scene_.lock();
    if (scene) {
        std::pair<SharingErrorCode, std::string> sharingErrorCode;
        if (action == Wifi::P2pActionCallback::P2pConnect && code != Wifi::WIFI_OPT_SUCCESS) {
            if (GetErrorCode(code, sharingErrorCode)) {
                HILOG_COMM_INFO("wifi errorCode: %{public}d,  sharingError: %{public}d, %{public}s", code,
                                sharingErrorCode.first, sharingErrorCode.second.c_str());
                scene->OnInnerError(0, 0, sharingErrorCode.first, sharingErrorCode.second);
            } else {
                HILOG_COMM_INFO("default return action: %{public}hhu, code: %{public}d.", action, code);
            }
        }
    }
}

void WfdSourceScene::WfdP2pCallback::OnConfigChanged(Wifi::CfgType type, char *data, int32_t dataLen)
{
    (void)type;
    (void)data;
    (void)dataLen;
    SHARING_LOGE("%{public}s.", __FUNCTION__);
}

std::string WfdSourceScene::WfdP2pCallback::GetUdidHash(std::string deviceId)
{
    std::lock_guard<std::mutex> lock(macMapMutex_);
    return macUdidMap_.count(deviceId) ?  macUdidMap_[deviceId] : "";
}

void WfdSourceScene::WfdP2pCallback::OnP2pChrErrCodeReport(const int errCode)
{
    HILOG_COMM_INFO("OnP2pChrErrCodeReport");
    auto scene = scene_.lock();
    if (scene == nullptr) {
        HILOG_COMM_ERROR("scene is nullptr.");
        return;
    }
    scene->OnInnerError(0, 0, errCode, "on P2P errCode report", true);
}

void WfdSourceScene::WfdSystemAbilityListener::OnAddSystemAbility(int32_t systemAbilityId, const std::string& deviceId)
{
    HILOG_COMM_INFO("%{public}s, id is %{public}d.", __FUNCTION__, systemAbilityId);
}

void WfdSourceScene::WfdSystemAbilityListener::OnRemoveSystemAbility(int32_t systemAbilityId,
                                                                     const std::string& deviceId)
{
    HILOG_COMM_INFO("%{public}s, id is %{public}d.", __FUNCTION__, systemAbilityId);
    if (systemAbilityId != WIFI_DEVICE_ABILITY_ID) {
        HILOG_COMM_ERROR("not wifi device SA info.");
        return;
    }
    auto scene = scene_.lock();
    RETURN_IF_NULL(scene);
    scene->OnWfdServiceAbilityDied();
}

WfdSourceScene::WfdSourceScene()
{
    HILOG_COMM_INFO("id: %{public}u.", GetId());
}

WfdSourceScene::~WfdSourceScene()
{
    HILOG_COMM_INFO("id: %{public}u.", GetId());
    Release();
}

void WfdSourceScene::OnCheckWfdConnection()
{
    ResetCheckWfdConnectionTimer();
    OnInnerError(0, 0, SharingErrorCode::ERR_CONNECTION_TIMEOUT, "wfd connection timeout");
}

void WfdSourceScene::SetCheckWfdConnectionTimer()
{
    int32_t interval = P2P_CONNECT_TIMEOUT * WFD_SEC_TO_MSEC;
    if (timer_) {
        timerId_ = timer_->Register([this] { this->OnCheckWfdConnection(); }, interval);
        timer_->Setup();
    }
}

void WfdSourceScene::ResetCheckWfdConnectionTimer()
{
    uint32_t id = timerId_.exchange(0);
    if (timer_ && id != 0) {
        timer_->Unregister(id);
    }
}

void WfdSourceScene::OnCheckWfdDisconnection()
{
    HILOG_COMM_INFO("OnCheckWfdDisconnection");

    ResetCheckWfdDisconnectionTimer();
    OnP2pPeerDisconnected(true);
}

void WfdSourceScene::SetCheckWfdDisconnectionTimer()
{
    int32_t interval = P2P_DISCONNECT_TIMEOUT * WFD_SEC_TO_MSEC;
    if (timer_) {
        disconnectTimerId_ = timer_->Register([this] { this->OnCheckWfdDisconnection(); }, interval);
        timer_->Setup();
    }
}

void WfdSourceScene::ResetCheckWfdDisconnectionTimer()
{
    HILOG_COMM_INFO("ResetCheckWfdDisconnectionTimer");

    uint32_t id = disconnectTimerId_.exchange(0);
    if (timer_ && id != 0) {
        timer_->Unregister(id);
    }
}

sptr<WfdSourceScene::WfdP2pCallback> WfdSourceScene::wfdP2pCallback_ = nullptr;

void WfdSourceScene::SetWifiScene(uint32_t scene)
{
    HILOG_COMM_INFO("SetWifiScene %{public}u start", scene);
    std::string ifName = "p2p0";
    Hid2dUpperScene upperScene;
    upperScene.scene = scene;
    upperScene.fps = -1;
    upperScene.bw = 0;
    if (Hid2dSetUpperScene(ifName.c_str(), &upperScene) != 0) {
        HILOG_COMM_ERROR("SetWifiScene scene:%{public}u error", scene);
    }
    HILOG_COMM_INFO("SetWifiScene %{public}u end", scene);
}

void WfdSourceScene::Initialize()
{
    HILOG_COMM_INFO("Initialize");
    SharingValue::Ptr values = nullptr;

    auto ret = Config::GetInstance().GetConfig("khSharingWfd", "ctrlport", "defaultWfdCtrlport", values);
    if (ret == CONFIGURE_ERROR_NONE) {
        values->GetValue<int32_t>(ctrlPort_);
    }

    ret = Config::GetInstance().GetConfig("khSharingWfd", "mediaFormat", "videoCodec", values);
    if (ret == CONFIGURE_ERROR_NONE) {
        int32_t videoCodec;
        values->GetValue<int32_t>(videoCodec);
        videoCodecId_ = static_cast<CodecId>(videoCodec);
        videoCodecId_ = CodecId::CODEC_H264;
    }

    ret = Config::GetInstance().GetConfig("khSharingWfd", "mediaFormat", "videoFormat", values);
    if (ret == CONFIGURE_ERROR_NONE) {
        int32_t videoFormat;
        values->GetValue<int32_t>(videoFormat);
        videoFormat_ = static_cast<VideoFormat>(videoFormat);
        videoFormat_ = VideoFormat::VIDEO_1920X1080_25;
    }

    ret = Config::GetInstance().GetConfig("khSharingWfd", "mediaFormat", "audioCodec", values);
    if (ret == CONFIGURE_ERROR_NONE) {
        int32_t audioCodec;
        values->GetValue<int32_t>(audioCodec);
        audioCodecId_ = static_cast<CodecId>(audioCodec);
        audioCodecId_ = CodecId::CODEC_AAC;
    }

    ret = Config::GetInstance().GetConfig("khSharingWfd", "mediaFormat", "audioFormat", values);
    if (ret == CONFIGURE_ERROR_NONE) {
        int32_t audioFormat;
        values->GetValue<int32_t>(audioFormat);
        audioFormat_ = static_cast<AudioFormat>(audioFormat);
        audioFormat_ = AudioFormat::AUDIO_48000_16_2;
    }

    RegisterWfdP2pCallback();

    RegisterWfdAbilityListener();
}

void WfdSourceScene::Release()
{
    HILOG_COMM_INFO("Release");
    UnRegisterWfdAbilityListener();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (timer_ != nullptr) {
            timer_->Shutdown();
            timer_.reset();
        }
    }
    uint32_t contextId = INVALID_ID;
    uint32_t agentId = INVALID_ID;
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto sessionMsg = std::make_shared<WfdSourceSessionEventMsg>();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if ((connDev_ != nullptr) && (connDev_->contextId != INVALID_ID) && (connDev_->agentId != INVALID_ID)) {
                sessionMsg->type = EVENT_SESSION_TEARDOWN;
                sessionMsg->toMgr = MODULE_CONTEXT;
                sessionMsg->dstId = connDev_->contextId;
                sessionMsg->agentId = connDev_->agentId;
                contextId = connDev_->contextId;
                agentId = connDev_->agentId;
            }
            connDev_.reset();
        }

        SharingEvent event;
        event.eventMsg = std::move(sessionMsg);
        if ((contextId != INVALID_ID) && (agentId != INVALID_ID)) {
            sharingAdapter->ForwardEvent(contextId, agentId, event, true);
            sharingAdapter->DestroyAgent(contextId, agentId);
        }

        if ((contextId_ != INVALID_ID) && (agentId_ != INVALID_ID)) {
            sharingAdapter->DestroyAgent(contextId_, agentId_);
        }
        std::shared_ptr<Wifi::WifiP2p> p2p;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            p2p = std::move(p2pInstance_);
        }
        if (p2p) {
            p2p->RemoveGroup();
            p2p.reset();
        }
        SetWifiScene(0);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        p2pInstance_.reset();
    }
}

void WfdSourceScene::OnDomainMsg(std::shared_ptr<BaseDomainMsg> &msg)
{
    SHARING_LOGE("%{public}s.", __FUNCTION__);
}

void WfdSourceScene::OnRequest(std::shared_ptr<BaseMsg> msg, std::shared_ptr<BaseMsg> &reply)
{
    RETURN_IF_NULL(msg);
    auto rsp = std::make_shared<WfdCommonRsp>();
    switch (msg->GetMsgId()) {
        case WfdSourceStartDiscoveryReq::MSG_ID: {
            rsp->ret = HandleStartDiscovery(msg, rsp);
            break;
        }
        case WfdSourceStopDiscoveryReq::MSG_ID: {
            rsp->ret = HandleStopDiscovery(msg, rsp);
            break;
        }
        case WfdSourceAddDeviceReq::MSG_ID: {
            ResetCheckWfdConnectionTimer();
            rsp->ret = WfdSourceScene::HandleAddDevice(msg, rsp);
            SetCheckWfdConnectionTimer();
            break;
        }
        case WfdSourceRemoveDeviceReq::MSG_ID: {
            rsp->ret = HandleRemoveDevice(msg, rsp);
            break;
        }
        case DestroyScreenCaptureReq::MSG_ID: {
            rsp->ret = HandleDestroyScreenCapture(msg);
            break;
        }
        case WfdSourceSetAudioReq::MSG_ID: {
            rsp->ret = HandleSetAudio(msg);
            break;
        }
        case WfdSourceSetDisplayReq::MSG_ID: {
            rsp->ret = HandleSetProjectionDisplay(msg);
            break;
        }
        case WfdSourceEventReq::MSG_ID: {
            rsp->ret = HandleEvent(msg);
            break;
        }
        case WfdAppCastSurfaceReq::MSG_ID: {
            rsp->ret = HandleAppendSurface(msg);
            break;
        }
        default:
            HILOG_COMM_ERROR("unknown msg request.");
            return;
    }
    reply = std::static_pointer_cast<BaseMsg>(rsp);
}

int32_t WfdSourceScene::HandleStartDiscovery(std::shared_ptr<BaseMsg> &baseMsg,
                                             std::shared_ptr<WfdCommonRsp> &reply)
{
    HILOG_COMM_INFO("HandleStartDiscovery");
    if (!IsWfdP2pCallbackRegistered_) {
        RegisterWfdP2pCallback();
    }
    (void)baseMsg;
    (void)reply;
    auto p2p = GetP2pInstance();
    if (!p2p) {
        HILOG_COMM_ERROR("p2pInstance is nullptr.");
        return -1;
    }
    if (!isSourceRunning_) {
        int32_t status = 0;
        p2p->GetP2pEnableStatus(status);
        HILOG_COMM_INFO("HandleStartDiscovery, currP2P status is %{public}d.", status);
        switch (static_cast<Wifi::P2pState>(status)) {
            case Wifi::P2pState::P2P_STATE_NONE:     // fall-through
            case Wifi::P2pState::P2P_STATE_IDLE:     // fall-through
            case Wifi::P2pState::P2P_STATE_STARTING: // fall-through
            case Wifi::P2pState::P2P_STATE_CLOSING: {
                OnInnerError(0, 0, SharingErrorCode::ERR_STATE_EXCEPTION, "HandleStart current p2p state: CLOSING");
                return -1;
            }
            case Wifi::P2pState::P2P_STATE_STARTED: {
                isSourceRunning_ = true;
                break;
            }
            case Wifi::P2pState::P2P_STATE_CLOSED: {
                isSourceRunning_ = true;
                p2p->EnableP2p();
                return 0;
            }
            default: {
                HILOG_COMM_ERROR("none process case.");
                return -1;
            }
        }
    }
    WfdP2pStart();
    return 0;
}

bool WfdSourceScene::WfdP2pIsDiscovery()
{
    HILOG_COMM_INFO("WfdP2pIsDiscovery");

    auto p2p = GetP2pInstance();
    if (p2p) {
        int status = 0;
        if (p2p->GetP2pDiscoverStatus(status) == Wifi::WIFI_OPT_SUCCESS) {
            HILOG_COMM_INFO("GetP2pDiscoverStatus status:%{public}d", status);
            if (status == static_cast<int>(Wifi::P2pDiscoverState::P2P_DISCOVER_STARTING)) {
                return true;
            }
        }
    }
    return false;
}

void WfdSourceScene::WfdP2pReStartDiscovery()
{
    HILOG_COMM_INFO("WfdP2pReStartDiscovery");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connDev_ != nullptr &&
            (connDev_->state == ConnectionState::CONNECTING ||
             connDev_->state == ConnectionState::CONNECTED)) {
            HILOG_COMM_ERROR("p2pInstance is CONNECTING.");
            return ;
        }
    }

    auto p2p = GetP2pInstance();
    if (p2p && isSourceDiscovering) {
        HILOG_COMM_INFO("WfdP2pStartDiscovery DiscoverDevices.");
        SetLocalDeviceName();
        p2p->DiscoverPeers(DISCOVERY_PEERS_CHANNEL);
        isSourceDiscovering = true;
    }
}

void WfdSourceScene::SetLocalDeviceName()
{
    HILOG_COMM_INFO("SetLocalDeviceName");
    auto p2p = GetP2pInstance();
    if (p2p) {
        DmKit::InitDeviceManager();
        DmDeviceInfo local;
        if (DeviceManager::GetInstance().GetLocalDeviceInfo(DM_PKG_NAME, local) != DM_OK) {
            HILOG_COMM_ERROR("Cannot get the local device info from DM");
            p2p->SetP2pDeviceName("HarmonyOS");
        } else {
            HILOG_COMM_INFO("WfdP2pStart local deviceName = %{public}s.", GetAnonyString(local.deviceName).c_str());
            p2p->SetP2pDeviceName(local.deviceName);
        }
    }
}

int32_t WfdSourceScene::HandleStopDiscovery(std::shared_ptr<BaseMsg> &baseMsg,
                                            std::shared_ptr<WfdCommonRsp> &reply)
{
    HILOG_COMM_INFO("HandleStopDiscovery");
    (void)baseMsg;
    (void)reply;
    if (!isSourceRunning_) {
        HILOG_COMM_ERROR("p2p source is not running.");
        return -1;
    }

    auto p2p = GetP2pInstance();
    if (!p2p) {
        HILOG_COMM_ERROR("p2pInstance is nullptr.");
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connDev_ != nullptr && connDev_->state == ConnectionState::CONNECTING) {
            HILOG_COMM_ERROR("p2pInstance is CONNECTING.");
            return -1;
        }
    }

    isSourceDiscovering = false;
    int32_t ret = p2p->StopDiscoverDevices();

    return ret;
}

int32_t WfdSourceScene::HandleAddDevice(std::shared_ptr<BaseMsg> &baseMsg,
                                        std::shared_ptr<WfdCommonRsp> &reply)
{
    HILOG_COMM_INFO("HandleAddDevice");
    (void)reply;
    auto p2p = GetP2pInstance();
    if (!isSourceRunning_ || !p2p) {
        HILOG_COMM_ERROR("p2p source is not running.");
        return -1;
    }
    isSourceCanceled = false;
    isP2pConnected_ = false;

    std::shared_ptr<WfdSourceAddDeviceReq> msg = std::static_pointer_cast<WfdSourceAddDeviceReq>(baseMsg);
    if (!IsScreenIdExist(msg->screenId)) {
        HILOG_COMM_ERROR("can't find screenId %{public}" PRIu64 ".", msg->screenId);
        return ERR_BAD_PARAMETER;
    }

    ConnectionInfo connectionInfo;
    connectionInfo.mac = msg->deviceId;
    connectionInfo.state = ConnectionState::CONNECTING;

    if (wfdP2pCallback_ != nullptr) {
        connectionInfo.udidHash = wfdP2pCallback_->GetUdidHash(connectionInfo.mac);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connDev_ = nullptr;
        connDev_ = std::make_unique<ConnectionInfo>(connectionInfo);
    }

    p2p->DisableRandomMac(1);

    Wifi::WifiP2pConfig config;
    config.SetDeviceAddress(msg->deviceId);
    config.SetDeviceAddressType(OHOS::Wifi::RANDOM_DEVICE_ADDRESS);
    config.SetGroupOwnerIntent(GROUP_OWNER_INTENT_MAX - 1);
    screenId_ = msg->screenId;
    SetWifiScene(1);
    int32_t ret = p2p->P2pConnect(config);
    if (ret != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        connDev_ = nullptr;
        OnInnerError(0, 0, SharingErrorCode::ERR_CONNECTION_FAILURE, "P2P CONNECT ERROR");
        return ERR_CONNECTION_FAILURE;
    }

    HILOG_COMM_INFO("connect device: %{public}s, ret = %{public}d", GetAnonyString(msg->deviceId).c_str(), ret);
    OnConnectionChanged(connectionInfo);
    isReportError_ = false;
    Memory::MemMgrClient::GetInstance().SetCritical(getpid(), true, SHARING_SERVICE_TEMP_SA_ID);
    return ret;
}

bool WfdSourceScene::IsScreenIdExist(uint64_t screenId)
{
    HILOG_COMM_INFO("IsScreenIdExist");
    auto displayIds = OHOS::Rosen::DisplayManager::GetInstance().GetAllDisplayIds();
    bool findDisplayId = false;
    for (auto displayId : displayIds) {
        HILOG_COMM_INFO("displayId = %{public}" PRIu64 ".", displayId);
        if (screenId == displayId) {
            findDisplayId = true;
            break;
        }
    }
    return findDisplayId;
}

int32_t WfdSourceScene::HandleRemoveDevice(std::shared_ptr<BaseMsg> &baseMsg,
                                           std::shared_ptr<WfdCommonRsp> &reply)
{
    HILOG_COMM_INFO("HandleRemoveDevice");
    (void)reply;
    auto sharingAdapter = sharingAdapter_.lock();
    RETURN_INVALID_IF_NULL(sharingAdapter);

    auto p2p = GetP2pInstance();
    uint32_t contextId = INVALID_ID;
    uint32_t agentId = INVALID_ID;
    std::shared_ptr<WfdSourceRemoveDeviceReq> msg = std::static_pointer_cast<WfdSourceRemoveDeviceReq>(baseMsg);

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connDev_ == nullptr || connDev_->mac != msg->deviceId || connDev_->state != ConnectionState::CONNECTED) {
            connDev_ = nullptr;
            lock.unlock();

            HILOG_COMM_ERROR("can not find dev, mac: %{public}s.", GetAnonyString(msg->deviceId).c_str());

            if (p2p) p2p->P2pCancelConnect();

            if (!isReportError_) {
                ConnectionInfo conn{};
                conn.state = ConnectionState::DISCONNECTED;
                conn.mac = msg->deviceId;
                OnConnectionChanged(conn);
            }
            isSourceCanceled = true;

            if ((contextId_ != INVALID_ID) && (agentId_ != INVALID_ID)) {
                sharingAdapter->DestroyAgent(contextId_, agentId_);
            }
            if (p2p) p2p->RemoveGroup();

            SetWifiScene(0);
            return 0;
        }

        if (connDev_ != nullptr) {
            contextId = connDev_->contextId;
            agentId = connDev_->agentId;
            connDev_->state = ConnectionState::DISCONNECTED;
        }
    }

    if ((contextId != INVALID_ID) && (agentId != INVALID_ID)) {
        auto sessionMsg = std::make_shared<WfdSourceSessionEventMsg>();
        sessionMsg->type = EVENT_SESSION_TEARDOWN;
        sessionMsg->toMgr = MODULE_CONTEXT;
        sessionMsg->dstId = contextId;
        sessionMsg->agentId = agentId;

        SharingEvent event;
        event.eventMsg = std::move(sessionMsg);
        sharingAdapter->ForwardEvent(contextId, agentId, event, true);
    }

    SetCheckWfdDisconnectionTimer();
    Memory::MemMgrClient::GetInstance().SetCritical(getpid(), false, SHARING_SERVICE_TEMP_SA_ID);
    return 0;
}

int32_t WfdSourceScene::CreateScreenCapture()
{
    HILOG_COMM_INFO("CreateScreenCapture");
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Destroy existing screen-capture agent before creating a new one
        if (contextId_ != INVALID_ID && agentId_ != INVALID_ID) {
            uint32_t ctxId = contextId_;
            uint32_t agtId = agentId_;
            SHARING_LOGI("Destroy existing screen-capture agent, contextId: %{public}u, agentId: %{public}u",
                ctxId, agtId);
            sharingAdapter->DestroyAgent(ctxId, agtId);
            contextId_ = INVALID_ID;
            agentId_ = INVALID_ID;
        }

        uint32_t ctxId = contextId_;
        uint32_t agtId = agentId_;
        sharingAdapter->CreateAgent(ctxId, agtId, AgentType::SINK_AGENT, "ScreenCaptureSession", localIp_);
        contextId_ = ctxId;
        agentId_ = agtId;
        if (contextId_ == INVALID_ID || agentId_ == INVALID_ID) {
            lock.unlock();
            HILOG_COMM_ERROR("Create ScreenCapture sink agent failed");
            return ERR_AGENT_CREATE;
        } else {
            HILOG_COMM_INFO("Create ScreenCapture sink agent, contextId: %{public}u, agentId: %{public}u",
                            contextId_.load(), agentId_.load());
        }
        lock.unlock();

        auto startSessionMsg = std::make_shared<ScreenCaptureSessionEventMsg>();
        startSessionMsg->mediaType = MEDIA_TYPE_AV;
        startSessionMsg->type = EVENT_SESSION_INIT;
        startSessionMsg->toMgr = MODULE_CONTEXT;
        startSessionMsg->dstId = contextId_;
        startSessionMsg->agentId = agentId_;
        startSessionMsg->screenId = screenId_;
        videoFormat_ = VideoFormat::VIDEO_1920X1080_25;

        startSessionMsg->videoFormat = videoFormat_;
        startSessionMsg->audioFormat = audioFormat_;

        SharingEvent event;
        event.eventMsg = std::move(startSessionMsg);

        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
        sharingAdapter->Start(contextId_, agentId_);
    }

    return ERR_OK;
}

int32_t WfdSourceScene::HandleDestroyScreenCapture(std::shared_ptr<BaseMsg> &baseMsg)
{
    HILOG_COMM_INFO("HandleDestroyScreenCapture");
    (void)baseMsg;
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        if ((contextId_ != INVALID_ID) && (agentId_ != INVALID_ID)) {
            sharingAdapter->DestroyAgent(contextId_, agentId_);
        }
    }
    return ERR_OK;
}

int32_t WfdSourceScene::HandleSetAudio(std::shared_ptr<BaseMsg> &baseMsg)
{
    HILOG_COMM_INFO("HandleSetAudio");

    std::shared_ptr<WfdSourceSetAudioReq> msg = std::static_pointer_cast<WfdSourceSetAudioReq>(baseMsg);

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto setAudioSessionMsg = std::make_shared<SetAudioSessionEventMsg>();
        setAudioSessionMsg->type = EVENT_SCREEN_CAPTURE_SET_AUDIO;
        setAudioSessionMsg->toMgr = MODULE_CONTEXT;
        setAudioSessionMsg->dstId = contextId_;
        setAudioSessionMsg->agentId = agentId_;
        setAudioSessionMsg->isAudioOn = msg->isAudioOn;
        SharingEvent event;
        event.eventMsg = std::move(setAudioSessionMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return ERR_OK;
}

int32_t WfdSourceScene::HandleSetProjectionDisplay(std::shared_ptr<BaseMsg> &baseMsg)
{
    HILOG_COMM_INFO("HandleSetProjectionDisplay");

    std::shared_ptr<WfdSourceSetDisplayReq> msg = std::static_pointer_cast<WfdSourceSetDisplayReq>(baseMsg);

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto displayParam = msg->displayParam;
        if (!json::accept(displayParam)) {
            HILOG_COMM_ERROR("accept error.");
            return -1;
        }
        json jsonObject = json::parse(displayParam);
        if (!jsonObject.contains(KEY_PROJECT_MODE) || !jsonObject.contains(KEY_PROJECT_ROTATION) ||
            !jsonObject.contains(KEY_PROJECT_RECT)) {
            HILOG_COMM_ERROR("project face json param error");
            return -1;
        }

        if (!jsonObject[KEY_PROJECT_MODE].is_number() || !jsonObject[KEY_PROJECT_ROTATION].is_number() ||
            !jsonObject[KEY_PROJECT_RECT].is_structured()) {
            HILOG_COMM_ERROR("select screen json param error");
            return -1;
        }
        uint32_t projectMode = jsonObject[KEY_PROJECT_MODE];
        uint32_t projectRotation = jsonObject[KEY_PROJECT_ROTATION];
        Rect mainScreenRegion;
        mainScreenRegion.x = jsonObject[KEY_PROJECT_RECT][KEY_PROJECT_RECT_LEFT];
        mainScreenRegion.y = jsonObject[KEY_PROJECT_RECT][KEY_PROJECT_RECT_TOP];
        mainScreenRegion.w = jsonObject[KEY_PROJECT_RECT][KEY_PROJECT_RECT_WIDTH];
        mainScreenRegion.h = jsonObject[KEY_PROJECT_RECT][KEY_PROJECT_RECT_HEIGHT];

        auto setProjectionDisplayMsg = std::make_shared<SetDisplaySessionEventMsg>();
        setProjectionDisplayMsg->type = EVENT_SCREEN_CAPTURE_SET_DISPLAY;
        setProjectionDisplayMsg->toMgr = MODULE_CONTEXT;
        setProjectionDisplayMsg->dstId = contextId_;
        setProjectionDisplayMsg->agentId = agentId_;
        setProjectionDisplayMsg->projectMode = projectMode;
        setProjectionDisplayMsg->projectRotation = projectRotation;
        setProjectionDisplayMsg->mainScreenRegion = mainScreenRegion;
        SharingEvent event;
        event.eventMsg = std::move(setProjectionDisplayMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return 0;
}

int32_t WfdSourceScene::HandleEvent(std::shared_ptr<BaseMsg> &baseMsg)
{
    HILOG_COMM_INFO("HandleEvent");

    std::shared_ptr<WfdSourceEventReq> msg = std::static_pointer_cast<WfdSourceEventReq>(baseMsg);

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto eventId = msg->eventId;
        auto eventParam = msg->eventParam;
        if (eventId == EVENT_SCREEN_PARAM) {
            screenParam_ = eventParam;
            SHARING_LOGI("HandleEvent cache screen param.");
            return 0;
        }

        auto commonMsg = std::make_shared<CommonSessionEventMsg>();
        commonMsg->type = EVENT_SCREEN_CAPTURE_COMMON;
        commonMsg->toMgr = MODULE_CONTEXT;
        commonMsg->dstId = contextId_;
        commonMsg->agentId = agentId_;
        commonMsg->eventId = eventId;
        commonMsg->eventParam = eventParam;
        SharingEvent event;
        event.eventMsg = std::move(commonMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return 0;
}

int32_t WfdSourceScene::HandleAppendSurface(std::shared_ptr<BaseMsg> &baseMsg)
{
    HILOG_COMM_INFO("HandleAppendSurface");

    std::shared_ptr<WfdAppCastSurfaceReq> msg = std::static_pointer_cast<WfdAppCastSurfaceReq>(baseMsg);

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto surface = msg->surface;

        auto surfaceMsg = std::make_shared<AppendSurfaceSessionEventMsg>();
        surfaceMsg->type = EVENT_SCREEN_CAPTURE_APPEND_SURFACE;
        surfaceMsg->toMgr = MODULE_CONTEXT;
        surfaceMsg->dstId = contextId_;
        surfaceMsg->agentId = agentId_;
        surfaceMsg->surface = surface;

        SharingEvent event;
        event.eventMsg = std::move(surfaceMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return 0;
}

bool WfdSourceScene::setConnDev(uint32_t contextId, uint32_t agentId)
{
    connDev_->contextId = contextId;
    connDev_->agentId = agentId;

    if (connDev_->isRunning == true) {
        return true;
    }
    connDev_->isRunning = true;
    return false;
}

int32_t WfdSourceScene::AppendCast(const std::string &deviceId)
{
    HILOG_COMM_INFO("AppendCast");
    auto sharingAdapter = sharingAdapter_.lock();
    RETURN_INVALID_IF_NULL(sharingAdapter);

    std::unique_lock<std::mutex> lock(mutex_);
    if ((connDev_ == nullptr) || (connDev_->mac != deviceId)) {
        HILOG_COMM_ERROR("can not find dev, mac: %{public}s.", GetAnonyString(deviceId).c_str());
        lock.unlock();
        OnInnerError(0, 0, SharingErrorCode::ERR_BAD_PARAMETER, "AppendCast can't find the dev");
        return -1;
    }
    auto port = SocketUtils::GetAvailableUdpPortPair();
    if (port == 0) {
        HILOG_COMM_ERROR("get udp port failed.");
        return -1;
    }

    uint32_t contextId = contextId_;
    uint32_t agentId = agentId_;
    sharingAdapter->CreateAgent(contextId, agentId, AgentType::SRC_AGENT, "WfdSourceSession", localIp_);
    if (contextId == INVALID_ID || agentId == INVALID_ID) {
        lock.unlock();
        HILOG_COMM_ERROR("create source agent failed.");
        return -1;
    }
    HILOG_COMM_INFO("create source agent, contextId: %{public}u, agentId: %{public}u", contextId, agentId);
    if (setConnDev(contextId, agentId)) {
        return 0;
    }

    auto startSessionMsg = std::make_shared<WfdSourceSessionEventMsg>();
    startSessionMsg->type = EVENT_SESSION_INIT;
    startSessionMsg->sinkAgentId = agentId_;
    startSessionMsg->toMgr = MODULE_CONTEXT;
    startSessionMsg->dstId = contextId;
    startSessionMsg->agentId = agentId;
    startSessionMsg->ip = connDev_->ip;
    startSessionMsg->mac = connDev_->mac;
    startSessionMsg->remotePort = connDev_->ctrlPort;
    startSessionMsg->videoFormat = connDev_->videoFormatId;
    startSessionMsg->audioFormat = connDev_->audioFormatId;
    startSessionMsg->localPort = port;
    lock.unlock();

    SharingEvent event;
    event.eventMsg = std::move(startSessionMsg);
    sharingAdapter->ForwardEvent(contextId, agentId, event, true);
    sharingAdapter->Start(contextId, agentId);

    HandleScreenParam();

    return 0;
}


void WfdSourceScene::HandleScreenParam()
{
    SHARING_LOGI("HandleScreenParam");
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr && !screenParam_.empty()) {
        auto commonMsg = std::make_shared<CommonSessionEventMsg>();
        commonMsg->type = EVENT_SCREEN_CAPTURE_COMMON;
        commonMsg->toMgr = MODULE_CONTEXT;
        commonMsg->dstId = contextId_;
        commonMsg->agentId = agentId_;
        commonMsg->eventId = EVENT_SCREEN_PARAM;
        commonMsg->eventParam = screenParam_;
        SharingEvent event;
        event.eventMsg = std::move(commonMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
        screenParam_ = "";
    }
}

int32_t WfdSourceScene::HandleSetRefreshRate(int32_t interval)
{
    HILOG_COMM_INFO("HandleSetRefreshRate");

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto setRefreshRateSessionMsg = std::make_shared<SetRefreshRateSessionEventMsg>();
        setRefreshRateSessionMsg->type = EVENT_SCREEN_CAPTURE_SET_REFRESHRATE;
        setRefreshRateSessionMsg->toMgr = MODULE_CONTEXT;
        setRefreshRateSessionMsg->dstId = contextId_;
        setRefreshRateSessionMsg->agentId = agentId_;
        setRefreshRateSessionMsg->interval = interval;

        SharingEvent event;
        event.eventMsg = std::move(setRefreshRateSessionMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return ERR_OK;
}

int32_t WfdSourceScene::HandleRequestIdr()
{
    HILOG_COMM_INFO("HandleRequestIdr");

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        auto sessionMsg = std::make_shared<SessionEventMsg>();
        sessionMsg->type = EVENT_SCREEN_REQUEST_IDR;
        sessionMsg->toMgr = MODULE_CONTEXT;
        sessionMsg->dstId = contextId_;
        sessionMsg->agentId = agentId_;

        SharingEvent event;
        event.eventMsg = std::move(sessionMsg);
        sharingAdapter->ForwardEvent(contextId_, agentId_, event, true);
    }
    return ERR_OK;
}

void WfdSourceScene::OnWfdServiceAbilityDied()
{
    HILOG_COMM_INFO("OnWfdServiceAbilityDied");
    IsWfdP2pCallbackRegistered_ = false;
    wfdP2pCallback_ = nullptr;
    OnDeviceOffline();
    OnInnerError(0, 0, SharingErrorCode::ERR_WIFI_DEVICE_ABILITY_DIED, "wifi device ability died");
}

void WfdSourceScene::RegisterWfdP2pCallback()
{
    HILOG_COMM_INFO("RegisterWfdP2pCallback");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        p2pInstance_ = Wifi::WifiP2p::GetInstance(WIFI_P2P_ABILITY_ID);
    }
    auto p2p = GetP2pInstance();
    RETURN_IF_NULL(p2p);

    if (wfdP2pCallback_ != nullptr) {
        wfdP2pCallback_->SetScene(shared_from_this());
        return;
    }
    sptr<WfdP2pCallback> wfdP2pCallback(new WfdP2pCallback(shared_from_this()));
    std::vector<std::string> event = {
        EVENT_P2P_STATE_CHANGE,
        EVENT_P2P_DEVICE_STATE_CHANGE,
        EVENT_P2P_PEER_DEVICE_CHANGE,
        EVENT_P2P_CONN_STATE_CHANGE,
        EVENT_P2P_DISCOVERY_CHANGE,
        EVENT_P2P_ACTION_RESULT,
        EVENT_P2P_CHR_ERRCODE_REPORT,
        EVENT_P2P_PRIVATE_PEER_DEVICE_CHANGE
    };

    p2p->RegisterCallBack(wfdP2pCallback, event);
    wfdP2pCallback_ = wfdP2pCallback;

    IsWfdP2pCallbackRegistered_ = true;
}

void WfdSourceScene::RegisterWfdAbilityListener()
{
    HILOG_COMM_INFO("RegisterWfdAbilityListener");
    std::unique_lock<std::mutex> lock(mutex_);
    auto samgrProxy = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgrProxy == nullptr) {
        HILOG_COMM_ERROR("get system ability manager fail");
        return;
    }
    sysAbilityListener_ = new (std::nothrow) WfdSystemAbilityListener(shared_from_this());
    if (sysAbilityListener_ == nullptr) {
        HILOG_COMM_ERROR("sysAbilityListener create failed.");
        return;
    }
    int32_t ret = samgrProxy->SubscribeSystemAbility(WIFI_DEVICE_ABILITY_ID, sysAbilityListener_);
    HILOG_COMM_INFO("result is %{public}d.", ret);
}

void WfdSourceScene::UnRegisterWfdAbilityListener()
{
    HILOG_COMM_INFO("UnRegisterWfdAbilityListener");
    std::unique_lock<std::mutex> lock(mutex_);
    if (sysAbilityListener_ == nullptr) {
        HILOG_COMM_ERROR("listener is null");
        return;
    }
    auto samgrProxy = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgrProxy == nullptr) {
        HILOG_COMM_ERROR("get system ability manager fail");
        return;
    }
    int32_t ret = samgrProxy->UnSubscribeSystemAbility(WIFI_DEVICE_ABILITY_ID, sysAbilityListener_);
    HILOG_COMM_INFO("result is %{public}d.", ret);
    sysAbilityListener_ = nullptr;
}

std::shared_ptr<Wifi::WifiP2p> WfdSourceScene::GetP2pInstance()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return p2pInstance_;
}

void WfdSourceScene::WfdP2pStart()
{
    HILOG_COMM_INFO("WfdP2pStart");
    auto p2p = GetP2pInstance();
    if (!p2p) {
        SHARING_LOGE("p2pInstance is nullptr.");
        return;
    }

    Wifi::WifiP2pWfdInfo wfdInfo;
    wfdInfo.SetWfdEnabled(true);
    wfdInfo.SetDeviceInfo(0x10);
    wfdInfo.SetCtrlPort(ctrlPort_);
    wfdInfo.SetMaxThroughput(0x00c8);

    auto ret = p2p->SetP2pWfdInfo(wfdInfo);
    if (ret != Wifi::WIFI_OPT_SUCCESS) {
        SHARING_LOGE("SetP2pWfdInfo failed: %{public}d", ret);
    }

    HILOG_COMM_INFO("WfdSourceScene DiscoverDevices.");
    if (isFirstDiscovery_ && WfdP2pIsDiscovery()) {
        HILOG_COMM_INFO("StopDiscoverDevices");
        ret = p2p->StopDiscoverDevices();
        if (ret != Wifi::WIFI_OPT_SUCCESS) {
            SHARING_LOGE("StopDiscoverDevices failed: %{public}d", ret);
        }
        isFirstDiscovery_ = false;
    }
    SetLocalDeviceName();
    p2p->DiscoverPeers(DISCOVERY_PEERS_CHANNEL);
    isSourceDiscovering = true;
}

void WfdSourceScene::WfdP2pStop()
{
    HILOG_COMM_INFO("WfdP2pStop");
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if ((connDev_ != nullptr) && (connDev_->contextId != INVALID_ID) && (connDev_->agentId != INVALID_ID)) {
                auto sessionMsg = std::make_shared<WfdSourceSessionEventMsg>();
                sessionMsg->type = EVENT_SESSION_TEARDOWN;
                sessionMsg->toMgr = MODULE_CONTEXT;
                sessionMsg->dstId = connDev_->contextId;
                sessionMsg->agentId = connDev_->agentId;
                lock.unlock();
                SharingEvent event;
                event.eventMsg = std::move(sessionMsg);
                sharingAdapter->ForwardEvent(connDev_->contextId, connDev_->agentId, event, true);
            }
        }
        if ((contextId_ != INVALID_ID) && (agentId_ != INVALID_ID)) {
            sharingAdapter->DestroyAgent(contextId_, agentId_);
        }
    }
}

void WfdSourceScene::OnDeviceFound(const std::vector<WfdCastDeviceInfo> &deviceInfos)
{
    HILOG_COMM_INFO("OnDeviceFound");
    auto ipcAdapter = ipcAdapter_.lock();
    RETURN_IF_NULL(ipcAdapter);

    auto msg = std::make_shared<WfdSourceDeviceFoundMsg>();
    msg->deviceInfos = deviceInfos;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connDev_ != nullptr && connDev_->state != ConnectionState::DISCONNECTED) {
            bool hasDevice = false;
            for (auto device : deviceInfos) {
                if (device.deviceId == connDev_->mac) {
                    HILOG_COMM_ERROR("has Current Device, device name: %{public}s, mac: %{public}s",
                                     GetAnonyString(device.deviceName).c_str(),
                                     GetAnonyString(device.deviceId).c_str());
                    hasDevice = true;
                    break;
                }
            }
            if (!hasDevice) {
                WfdCastDeviceInfo deviceInfo;
                deviceInfo.deviceId = connDev_->mac;
                deviceInfo.deviceName = connDev_->deviceName;
                deviceInfo.primaryDeviceType = connDev_->primaryDeviceType;
                deviceInfo.secondaryDeviceType = connDev_->secondaryDeviceType;
                deviceInfo.udidHash = connDev_->udidHash;
                HILOG_COMM_INFO("add Current Device, device name: %{public}s, mac: %{public}s",
                                GetAnonyString(deviceInfo.deviceName).c_str(),
                                GetAnonyString(deviceInfo.deviceId).c_str());
                msg->deviceInfos.emplace_back(deviceInfo);
            }
        }
    }
    auto reply = std::static_pointer_cast<BaseMsg>(std::make_shared<WfdCommonRsp>());
    ipcAdapter->SendRequest(msg, reply);
}

void WfdSourceScene::OnDeviceOffline()
{
    HILOG_COMM_INFO("OnDeviceOffline");
    auto ipcAdapter = ipcAdapter_.lock();
    RETURN_IF_NULL(ipcAdapter);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connDev_ != nullptr) {
            HILOG_COMM_ERROR("p2pInstance is CONNECTING.");
            return;
        }
    }
    auto msg = std::make_shared<WfdSourceDeviceFoundMsg>();

    auto reply = std::static_pointer_cast<BaseMsg>(std::make_shared<WfdCommonRsp>());
    ipcAdapter->SendRequest(msg, reply);
}

void WfdSourceScene::OnP2pDeviceDisconnectedChanged()
{
    HILOG_COMM_INFO("OnP2pDeviceDisconnectedChanged");

    if (isSourceDiscovering) {
        HILOG_COMM_ERROR("is P2p Discovering");
        return;
    }

    OnInnerError(0, 0, SharingErrorCode::ERR_P2P_DISCONNECTED_DEVICE_CHANGE,
        "P2P Disconnected device change");
}

void WfdSourceScene::OnP2pPeerConnected(ConnectionInfo &connectionInfo)
{
    HILOG_COMM_INFO("OnP2pPeerConnected, deviceName %{public}s, mac: %{public}s, ip: %{public}s, port: %{public}d.",
                    GetAnonyString(connectionInfo.deviceName).c_str(), GetAnonyString(connectionInfo.mac).c_str(),
                    GetAnonyString(connectionInfo.ip).c_str(), connectionInfo.ctrlPort);
    auto p2p = GetP2pInstance();
    if (!p2p) {
        HILOG_COMM_ERROR("p2pInstance is nullptr.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connDev_) {
            HILOG_COMM_ERROR("OnP2pPeerConnected ignore it, P2P already connected");
            return;
        }
        if (connDev_->ctrlPort != 0 || connDev_->mac != connectionInfo.mac) {
            HILOG_COMM_ERROR("OnP2pPeerConnected ignore it, connDev_ mac: %{public}s, connectionInfo mac: %{public}s",
                             GetAnonyString(connDev_->mac).c_str(), GetAnonyString(connectionInfo.mac).c_str());
            return;
        }
    }

    if (!isSourceRunning_ || connectionInfo.ctrlPort <= 0) {
        HILOG_COMM_ERROR("source service is not running.");
        return;
    }

    if (isSourceCanceled) {
        HILOG_COMM_ERROR("service is canceled.");
        return;
    }

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        connectionInfo.videoCodecId = videoCodecId_;
        connectionInfo.videoFormatId = videoFormat_;
        connectionInfo.audioCodecId = audioCodecId_;
        connectionInfo.audioFormatId = audioFormat_;
        connectionInfo.isRunning = connDev_->isRunning;
        connectionInfo.agentId = connDev_->agentId;
        connectionInfo.contextId = connDev_->contextId;
        connDev_ = nullptr;
        connDev_ = std::make_unique<ConnectionInfo>(connectionInfo);
        HILOG_COMM_INFO("connected, devMac: %{public}s, devIp: %{public}s.",
                        GetAnonyString(connectionInfo.mac).c_str(),
                        GetAnonyString(connectionInfo.ip).c_str());
    }
    if (p2p) {
        p2p->StopDiscoverDevices();
    }
    isSourceDiscovering = false;
    ResetCheckWfdConnectionTimer();
    if (CreateScreenCapture() == ERR_OK) {
        if (AppendCast(connectionInfo.mac) != 0) {
            SHARING_LOGE("append cast error.");
            // Rollback: destroy the screen-capture agent just created
            auto sharingAdapter = sharingAdapter_.lock();
            if (sharingAdapter != nullptr) {
                std::unique_lock<std::mutex> lock(mutex_);
                sharingAdapter->DestroyAgent(contextId_, agentId_);
                contextId_ = INVALID_ID;
                agentId_ = INVALID_ID;
            }
            OnInnerError(0, 0, SharingErrorCode::ERR_APPEND_CAST_ERROR, "append cast error");
        }
    } else {
        SHARING_LOGE("create screen capture error.");
        OnInnerError(0, 0, SharingErrorCode::ERR_CREATE_SCREEN_CAPTURE_ERROR, "create screen capture error");
    }
}

void WfdSourceScene::OnP2pPeerDisconnected(bool isNeedNotify)
{
    HILOG_COMM_INFO("OnP2pPeerDisconnected");
    std::string mac;
    bool isConnecting = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connDev_ != nullptr) {
            mac = connDev_->mac;
            isConnecting = true;
        }
    }

    if (isNeedNotify && isConnecting) {
        HILOG_COMM_ERROR("%{public}s.isConnecting = %{public}d", __FUNCTION__, isConnecting);
        OnInnerError(0, 0, SharingErrorCode::ERR_NETWORK_ERROR, "wfd p2p disconnect");
    }

    if (!mac.empty()) {
        HILOG_COMM_INFO("%{public}s. mac:%{public}s", __FUNCTION__, GetAnonyString(mac).c_str());
        OnP2pPeerDisconnected(mac, !isNeedNotify);
    }
}

void WfdSourceScene::OnP2pPeerDisconnected(const std::string &mac, bool isNeetReportConnection)
{
    HILOG_COMM_INFO("OnP2pPeerDisconnected");

    auto p2p = GetP2pInstance();
    if (!p2p) {
        SHARING_LOGE("p2pInstance is nullptr.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connDev_) {
            HILOG_COMM_ERROR("p2pInstance is nullptr.");
            return;
        }
    }

    ResetCheckWfdDisconnectionTimer();
    ConnectionInfo connectionInfo;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((connDev_ == nullptr) || (connDev_->mac != mac)) {
            HILOG_COMM_ERROR("can not find dev, mac: %{public}s.", GetAnonyString(mac).c_str());
            return;
        }
        connDev_->state = ConnectionState::DISCONNECTED;
        connectionInfo.mac = connDev_->mac;
        connectionInfo.deviceName = connDev_->deviceName;
        connectionInfo.state = connDev_->state;
        connectionInfo.udidHash = connDev_->udidHash;
        connDev_.reset();
    }

    if (isNeetReportConnection) {
        OnConnectionChanged(connectionInfo);
    }

    if (p2p) {
        p2p->RemoveGroup();
    }

    SetWifiScene(0);

    if ((contextId_ == INVALID_ID) || (agentId_ == INVALID_ID)) {
        HILOG_COMM_ERROR("OnP2pPeerDisconnected contextId_%{public}d, agentId_%{public}d",
                            contextId_.load(), agentId_.load());
        return;
    }

    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter != nullptr) {
        sharingAdapter->DestroyAgent(contextId_, agentId_);
    }
}

void WfdSourceScene::ErrorCodeFiltering(int32_t &code)
{
    HILOG_COMM_INFO("the error code is %{public}d.", code);
    switch (ABSTRACT_ERR_BASE(code)) {
        case SharingErrorCode::ERR_CONTEXT_AGENT_BASE: // fall-through
        case SharingErrorCode::ERR_SESSION_BASE:
            code = SharingErrorCode::ERR_GENERAL_ERROR;
            HILOG_COMM_INFO("the error change to %{public}d.", code);
            break;
        case SharingErrorCode::ERR_PROSUMER_BASE: {
            switch (code) {
                case ERR_PROSUMER_TIMEOUT:
                    code = SharingErrorCode::ERR_CONNECTION_TIMEOUT;
                    break;
                case ERR_PROSUMER_DESTROY:
                    code = SharingErrorCode::ERR_STATE_EXCEPTION;
                    break;
                case ERR_PROSUMER_RTP_CONNECT:
                case ERR_VTP_CONNECT_FAIL:
                case ERR_UDP_CONNECT_FAIL:
                    break;
                default:
                    code = SharingErrorCode::ERR_GENERAL_ERROR;
                    HILOG_COMM_INFO("the error change to %{public}d.", code);
                    break;
            }
            break;
        }
        default:
            HILOG_COMM_ERROR("none process case.");
            break;
    }
}

void WfdSourceScene::OnInnerError(uint32_t contextId, uint32_t agentId, SharingErrorCode errorCode, std::string message)
{
    if (isP2pConnected_ && (errorCode == SharingErrorCode::ERR_P2P_DISCONNECTED_DEVICE_CHANGE ||
                            errorCode == SharingErrorCode::ERR_P2P_DISCONNECTED_CONNECTION_CHANGE)) {
        isP2pConnected_ = false;
        errorCode = SharingErrorCode::ERR_P2P_DISCONNECTED_AFTER_CONNECTED;
    }
    if (!isGroupOwner_ && errorCode == SharingErrorCode::ERR_PROTOCOL_CONNECTION_TIMEOUT) {
        errorCode = SharingErrorCode::ERR_PROTOCOL_CONNECTION_TIMEOUT_GC;
    }
    if (!isGroupOwner_ && errorCode == SharingErrorCode::ERR_SOCKET_ERROR) {
        errorCode = SharingErrorCode::ERR_SOCKET_ERROR_GC;
    }
    OnInnerError(contextId, agentId, errorCode, message, false);
}

void WfdSourceScene::OnInnerError(uint32_t contextId, uint32_t agentId, int errorCode, std::string message,
                                  bool isFormP2p)
{
    HILOG_COMM_INFO("OnInnerError");
    if (!isFormP2p && isReportError_) {
        HILOG_COMM_ERROR("Already report error!.");
        return;
    }
    isReportError_ = true;

    auto ipcAdapter = ipcAdapter_.lock();
    RETURN_IF_NULL(ipcAdapter);

    auto msg = std::make_shared<WfdErrorMsg>();
    msg->contextId = contextId;
    msg->agentId = agentId;
    msg->errorCode = errorCode;

    if (errorCode == SharingErrorCode::ERR_PROSUMER_TIMEOUT) {
        msg->message =
            "contextId: " + std::to_string(contextId) + ", agentId: " + std::to_string(agentId) + ", producer timeout";
    } else {
        msg->message = std::move(message);
    }

    ErrorCodeFiltering(msg->errorCode);
    auto reply = std::static_pointer_cast<BaseMsg>(std::make_shared<WfdCommonRsp>());

    HILOG_COMM_INFO("receive errorCode: %{public}d.", errorCode);
    ipcAdapter->SendRequest(msg, reply);

    std::string mac;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connDev_ != nullptr) {
            mac = connDev_->mac;
        }
    }

    if (!mac.empty()) {
        HILOG_COMM_INFO("%{public}s. mac:%{public}s", __FUNCTION__, GetAnonyString(mac).c_str());
        OnP2pPeerDisconnected(mac, false);
    }
}

void WfdSourceScene::OnInnerDestroy(uint32_t contextId, uint32_t agentId, AgentType agentType)
{
    HILOG_COMM_INFO("HandleInnerDestroy, contextId: %{public}u, agentId: %{public}u, agentType: %{public}s.",
                    contextId, agentId, std::string(magic_enum::enum_name(agentType)).c_str());
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connDev_ == nullptr) {
            HILOG_COMM_ERROR("connDev_ is nullptr.");
            return;
        }

        if ((contextId == connDev_->contextId) && ((agentId == connDev_->agentId) || agentId == agentId_)) {
            ConnectionInfo connectionInfo;
            connectionInfo.mac = connDev_->mac;
            connectionInfo.deviceName = connDev_->deviceName;
            connectionInfo.state = ConnectionState::DISCONNECTED;
            connectionInfo.udidHash = connDev_->udidHash;
            connDev_.reset();
            lock.unlock();
            OnConnectionChanged(connectionInfo);
        } else {
            connDev_.reset();
        }
    }
}

void WfdSourceScene::OnInnerEvent(SharingEvent &event)
{
    RETURN_IF_NULL(event.eventMsg);

    HILOG_COMM_INFO("OnInnerEvent Type: %{public}s.",
                    std::string(magic_enum::enum_name(event.eventMsg->type)).c_str());
    switch (event.eventMsg->type) {
        case EventType::EVENT_WFD_NOTIFY_RTSP_PLAYED: {
            OnConnectionChanged(event);
            break;
        }
        case EventType::EVENT_WFD_NOTIFY_RTSP_TEARDOWN: {
            auto msg = ConvertEventMsg<WfdSceneEventMsg>(event);
            if (msg) {
                OnP2pPeerDisconnected(msg->mac, true);
            }
            break;
        }
        case EventType::EVENT_WFD_NOTIFY_SET_REFRESHRATE: {
            auto msg = ConvertEventMsg<WfdSceneEventMsg>(event);
            if (msg) {
                HandleSetRefreshRate(msg->interval);
            }
            break;
        }
        case EventType::EVENT_WFD_REQUEST_IDR: {
            auto msg = ConvertEventMsg<WfdSceneEventMsg>(event);
            if (msg) {
                HandleRequestIdr();
            }
            break;
        }
        case EventType::EVENT_WFD_NOTIFY_EVENT: {
            auto msg = ConvertEventMsg<WfdSceneCommonEventMsg>(event);
            if (msg) {
                OnEvent(msg->eventId, msg->eventParam);
            }
            break;
        }
        default:
            HILOG_COMM_ERROR("none process case.");
            break;
    }
}

void WfdSourceScene::OnConnectionChanged(SharingEvent &event)
{
    auto msg = ConvertEventMsg<WfdSceneEventMsg>(event);
    if (msg) {
        ConnectionInfo connectionInfo;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (connDev_ == nullptr) {
                HILOG_COMM_ERROR("connDev_ is null, mac:%{public}s", GetAnonyString(msg->mac).c_str());
                return;
            }
            connDev_->state = ConnectionState::CONNECTED;
            connectionInfo.mac = connDev_->mac;
            connectionInfo.deviceName = connDev_->deviceName;
            connectionInfo.state = connDev_->state;
            connectionInfo.udidHash = connDev_->udidHash;
        }
        OnConnectionChanged(connectionInfo);
    }
}

void WfdSourceScene::OnConnectionChanged(ConnectionInfo &connectionInfo)
{
    HILOG_COMM_INFO("OnConnectionChanged");

    if (isSourceCanceled) {
        HILOG_COMM_ERROR("service is canceled.");
        return;
    }

    auto ipcAdapter = ipcAdapter_.lock();
    RETURN_IF_NULL(ipcAdapter);

    auto msg = std::make_shared<WfdConnectionChangedMsg>();
    msg->ip = connectionInfo.ip;
    msg->mac = connectionInfo.mac;
    msg->state = connectionInfo.state;
    msg->surfaceId = connectionInfo.surfaceId;
    msg->deviceName = connectionInfo.deviceName;
    msg->primaryDeviceType = connectionInfo.primaryDeviceType;
    msg->secondaryDeviceType = connectionInfo.secondaryDeviceType;

    auto reply = std::static_pointer_cast<BaseMsg>(std::make_shared<WfdCommonRsp>());
    ipcAdapter->SendRequest(msg, reply);
}

void WfdSourceScene::OnEvent(int32_t eventId, const std::string &eventParam)
{
    HILOG_COMM_INFO("OnEvent");

    if (isSourceCanceled) {
        HILOG_COMM_ERROR("service is canceled.");
        return;
    }

    auto ipcAdapter = ipcAdapter_.lock();
    RETURN_IF_NULL(ipcAdapter);

    auto msg = std::make_shared<WfdEventMsg>();
    msg->eventId = eventId;
    msg->eventParam = eventParam;

    auto reply = std::static_pointer_cast<BaseMsg>(std::make_shared<WfdCommonRsp>());
    ipcAdapter->SendRequest(msg, reply);
}

void WfdSourceScene::OnRemoteDied()
{
    HILOG_COMM_INFO("OnRemoteDied");
    auto sharingAdapter = sharingAdapter_.lock();
    if (sharingAdapter) {
        sharingAdapter->ReleaseScene(GetId());
    }
}

REGISTER_CLASS_REFLECTOR(WfdSourceScene);
} // namespace Sharing
} // namespace OHOS
