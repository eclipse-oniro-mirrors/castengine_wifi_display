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

#ifndef OHOS_SHARING_WFD_SOURCE_SCENE_H
#define OHOS_SHARING_WFD_SOURCE_SCENE_H

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "agent/agent_def.h"
#include "interaction/scene/base_scene.h"
#include "timer.h"
#include "wfd_def.h"
#include "wfd_msg.h"
#include "wifi_p2p.h"
#include "interaction/device_kit/dm_kit.h"
#include "json.hpp"
#include "system_ability_status_change_stub.h"

using nlohmann::json;

namespace OHOS {
namespace Sharing {
class WfdSourceScene final : public BaseScene,
                             public std::enable_shared_from_this<WfdSourceScene> {
public:
    WfdSourceScene();
    ~WfdSourceScene();

public:
    class WfdP2pCallback : public Wifi::IWifiP2pCallback {
    public:
        sptr<IRemoteObject> AsObject() override { return nullptr; }

    public:
        explicit WfdP2pCallback(std::weak_ptr<WfdSourceScene> scene) : scene_(scene) {}

        void SetScene(std::weak_ptr<WfdSourceScene> scene);
        void OnP2pStateChanged(int32_t state) override;
        void OnP2pPersistentGroupsChanged(void) override;
        void OnP2pDiscoveryChanged(bool isChange) override;
        void OnP2pGcJoinGroup(const OHOS::Wifi::GcInfo &info) override;
        void OnP2pGcLeaveGroup(const OHOS::Wifi::GcInfo &info) override;
        void OnP2pThisDeviceChanged(const Wifi::WifiP2pDevice &device) override;
        void OnP2pConnectionChanged(const Wifi::WifiP2pLinkedInfo &info) override;
        void OnConfigChanged(Wifi::CfgType type, char *data, int32_t dataLen) override;
        void OnP2pPeersChanged(const std::vector<Wifi::WifiP2pDevice> &device) override;
        void OnP2pPrivatePeersChanged(const std::string &priWfdInfo) override;
        void OnP2pActionResult(Wifi::P2pActionCallback action, Wifi::ErrCode code) override;
        void OnP2pServicesChanged(const std::vector<Wifi::WifiP2pServiceInfo> &srvInfo) override;
        std::string GetUdidHash(std::string deviceId);
        static bool GetErrorCode(Wifi::ErrCode errorCode, std::pair<SharingErrorCode, std::string>& sharingError);
        void OnP2pChrErrCodeReport(const int errCode) override;
        void OnP2pServiceDiscoveryResult(const Wifi::P2pServiceDiscoveryInfo &info,
            const Wifi::WifiP2pDevice &device) override;
        Wifi::WifiP2pDevice GetGoDevice(const Wifi::WifiP2pGroupInfo &group,
                                         const std::shared_ptr<WfdSourceScene> &scene);
        void NotifyPeerConnected(const Wifi::WifiP2pLinkedInfo &info,
                                 const Wifi::WifiP2pDevice &goDevice);
    private:
        std::weak_ptr<WfdSourceScene> scene_;
        std::mutex macMapMutex_;
        std::mutex macCastPlusMutex_;
        std::unordered_map<std::string, std::string> macUdidMap_;
        std::unordered_map<std::string, std::string> macCastPlusMap_;
        static std::unordered_map<Wifi::ErrCode, std::pair<SharingErrorCode, std::string>> wifiErrorMapping_;
    };

    friend class WfdP2pCallback;

    class WfdSystemAbilityListener : public SystemAbilityStatusChangeStub {
    public:
        explicit WfdSystemAbilityListener(std::weak_ptr<WfdSourceScene> scene) : scene_(scene) {}

        void OnAddSystemAbility(int32_t systemAbilityId, const std::string &deviceId) override;
        void OnRemoveSystemAbility(int32_t systemAbilityId, const std::string &deviceId) override;

    private:
        std::weak_ptr<WfdSourceScene> scene_;
    };

protected:
    void SetIpcAdapter(const std::weak_ptr<IpcMsgAdapter> &adapter) final
    {
        SHARING_LOGD("trace.");
        ipcAdapter_ = adapter;
        auto ipcAdapter = ipcAdapter_.lock();
        if (ipcAdapter) {
            ipcAdapter->SetListener(shared_from_this());
        }
    }

protected:
    void Release();

    // impl BaseScene
    void Initialize() final;
    void OnRemoteDied() final;
    void OnInnerEvent(SharingEvent &event) final;
    void OnInnerError(uint32_t contextId, uint32_t agentId, int errorCode,
                      std::string message = "wfd inner error", bool isFormP2p = false);
    void OnInnerError(uint32_t contextId, uint32_t agentId, SharingErrorCode errorCode,
                      std::string message = "wfd inner error") final;
    void OnInnerDestroy(uint32_t contextId, uint32_t agentId, AgentType agentType) final;

    // interactionMgr -> interaction -> scene directly
    void OnDomainMsg(std::shared_ptr<BaseDomainMsg> &msg) final;

    // impl IpcMsgAdapterListener
    void OnRequest(std::shared_ptr<BaseMsg> msg, std::shared_ptr<BaseMsg> &reply) final;

private:
    void WfdP2pStop();
    void WfdP2pStart();

    static void SetWifiScene(uint32_t scene);

    void ErrorCodeFiltering(int32_t &code);

    void OnP2pPeerDisconnected(bool isNeedNotify);
    void OnP2pPeerDisconnected(const std::string &mac, bool isNeetReportConnection);
    void OnP2pPeerConnected(ConnectionInfo &connectionInfo);
    void OnP2pDeviceDisconnectedChanged();
    void OnCheckWfdConnection();
    void OnConnectionChanged(SharingEvent &event);
    void OnConnectionChanged(ConnectionInfo &connectionInfo);
    void OnEvent(int32_t eventId, const std::string &eventParam);
    void OnDeviceFound(const std::vector<WfdCastDeviceInfo> &deviceInfos);
    void OnDeviceOffline();
    void SetCheckWfdConnectionTimer();
    void ResetCheckWfdConnectionTimer();

    void OnCheckWfdDisconnection();
    void SetCheckWfdDisconnectionTimer();
    void ResetCheckWfdDisconnectionTimer();

    void OnWfdServiceAbilityDied();
    void RegisterWfdP2pCallback();
    void RegisterWfdAbilityListener();
    void UnRegisterWfdAbilityListener();

    int32_t CreateScreenCapture();
    int32_t AppendCast(const std::string &deviceId);
    int32_t HandleDestroyScreenCapture(std::shared_ptr<BaseMsg> &baseMsg);
    int32_t HandleStartDiscovery(std::shared_ptr<BaseMsg> &baseMsg,
                                 std::shared_ptr<WfdCommonRsp> &reply);
    int32_t HandleStopDiscovery(std::shared_ptr<BaseMsg> &baseMsg, std::shared_ptr<WfdCommonRsp> &reply);
    int32_t HandleAddDevice(std::shared_ptr<BaseMsg> &baseMsg, std::shared_ptr<WfdCommonRsp> &reply);
    int32_t HandleRemoveDevice(std::shared_ptr<BaseMsg> &baseMsg, std::shared_ptr<WfdCommonRsp> &reply);
    int32_t HandleSetAudio(std::shared_ptr<BaseMsg> &baseMsg);
    int32_t HandleSetProjectionDisplay(std::shared_ptr<BaseMsg> &baseMsg);
    int32_t HandleEvent(std::shared_ptr<BaseMsg> &baseMsg);
    int32_t HandleAppendSurface(std::shared_ptr<BaseMsg> &baseMsg);
    bool setConnDev(uint32_t contextId, uint32_t agentId);
    int32_t HandleRequestIdr();
    int32_t HandleSetRefreshRate(int32_t interval);
    bool WfdP2pIsDiscovery();
    void WfdP2pReStartDiscovery();
    void SetLocalDeviceName();
    bool IsScreenIdExist(uint64_t screenId);
    std::shared_ptr<Wifi::WifiP2p> GetP2pInstance();
    void HandleScreenParam();
private:
    std::atomic_bool isSourceRunning_ = false;
    std::atomic_bool isSourceDiscovering = false;
    std::atomic_bool isSourceCanceled = false;
    std::atomic_bool isReportError_ = false;
    std::atomic_bool isFirstDiscovery_ = true;
    std::atomic_bool IsWfdP2pCallbackRegistered_ = false;
    std::atomic_bool isP2pConnected_ = false;
    std::atomic_bool isGroupOwner_ = true;

    std::atomic<uint32_t> timerId_ = 0;
    std::atomic<uint32_t> disconnectTimerId_ = 0;

    std::atomic<uint32_t> agentId_ = INVALID_ID;
    std::atomic<uint32_t> contextId_ = INVALID_ID;

    int32_t ctrlPort_ = DEFAULT_WFD_CTRLPORT;

    std::atomic<uint64_t> screenId_ = 0;

    CodecId audioCodecId_ = CodecId::CODEC_AAC;
    CodecId videoCodecId_ = CodecId::CODEC_H264;
    AudioFormat audioFormat_ = AudioFormat::AUDIO_48000_16_2;
    VideoFormat videoFormat_ = VideoFormat::VIDEO_1920X1080_25;
    std::string localIp_ = "";

    std::mutex mutex_;
    std::shared_ptr<Wifi::WifiP2p> p2pInstance_;
    std::unique_ptr<ConnectionInfo> connDev_ = nullptr;
    std::unique_ptr<OHOS::Utils::Timer> timer_ = std::make_unique<OHOS::Utils::Timer>("WfdConnectTimeoutTimer");
    sptr<ISystemAbilityStatusChange> sysAbilityListener_ = nullptr;
    static sptr<WfdP2pCallback> wfdP2pCallback_;
    std::string screenParam_ = "";
};

} // namespace Sharing
} // namespace OHOS
#endif
