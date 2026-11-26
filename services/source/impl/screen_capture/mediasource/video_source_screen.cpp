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

#include <cstddef>
#include "video_source_screen.h"
#include "screen.h"
#include "sharing_log.h"
#include "parameters.h"
#include "transaction/rs_interfaces.h"
#include "screen_info.h"
#include "json.hpp"

namespace OHOS {
namespace Sharing {
using namespace std;
using OHOS::Rosen::DMError;
using nlohmann::json;
const std::string PARAM_DEVICE_TYPE = "const.product.devicetype";
const std::string PARAM_DEVICE_TYPE_PC = "2in1";

enum class ProjectionDisplay {
    DISPLAY_B = 1,
    DISPLAY_C,
    DISPLAY_BC
};

class ScreenListener : public Rosen::ScreenManager::IScreenListener {
public:
    explicit ScreenListener(uint64_t screenId)
    {
        screenId_ = screenId;
    }

    void OnConnect(Rosen::ScreenId screenId) override {}

    void OnDisconnect(Rosen::ScreenId screenId) override {}

    void OnChange(Rosen::ScreenId screenId) override
    {
        SHARING_LOGE("OnChange screenId = %{public}" PRIu64"", screenId);
        if (screenId == screenId_) {
            DisplayOnSourceModeChange(screenId);
        }
    }

private:
    void DisplayOnSourceModeChange(Rosen::ScreenId screenId)
    {
        auto screen = Rosen::ScreenManager::GetInstance().GetScreenById(screenId);
        if (screen == nullptr) {
            SHARING_LOGE("OnChange screen is nullptr");
            return;
        }
        sptr<Rosen::ScreenInfo> screenInfo = screen->GetScreenInfo();
        Rosen::ScreenSourceMode screenMode = screenInfo->GetSourceMode();
        SHARING_LOGE("OnChange GetSourceMode = %{public}d", screenMode);
        if (screenMode_ != screenMode) {
            screenMode_ = screenMode;
            if (screenMode == Rosen::ScreenSourceMode::SCREEN_MIRROR) {
                isScreenUniqueMode_ = false;
            } else if (screenMode == Rosen::ScreenSourceMode::SCREEN_UNIQUE) {
                isScreenUniqueMode_ = true;
                DMError ret = Rosen::ScreenManager::GetInstance().ResizeVirtualScreen(screenId,
                    screen->GetWidth(), screen->GetHeight());
                if (ret != DMError::DM_OK) {
                    SHARING_LOGE("ResizeVirtualScreen failed.");
                }
            }
        }
    }

    static constexpr uint64_t SCREEN_ID_INVALID = -1ULL;
    static constexpr uint64_t SCREEN_ID_MAIN = 0;

    uint64_t screenId_ = SCREEN_ID_INVALID;
    Rosen::ScreenSourceMode screenMode_ = Rosen::ScreenSourceMode::SCREEN_MIRROR;
    bool isScreenUniqueMode_ = false;
    std::mutex dataMutex_;
};

void VideoSourceScreen::ScreenGroupListener::OnChange(const std::vector<uint64_t> &screenIds,
                                                      Rosen::ScreenGroupChangeEvent event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    switch (event) {
        case Rosen::ScreenGroupChangeEvent::ADD_TO_GROUP:
            SHARING_LOGI("ADD_TO_GROUP done!");
            break;
        case Rosen::ScreenGroupChangeEvent::REMOVE_FROM_GROUP:
            SHARING_LOGI("REMOVE_FROM_GROUP done!");
            break;
        case Rosen::ScreenGroupChangeEvent::CHANGE_GROUP:
            SHARING_LOGI("CHANGE_GROUP done!");
            break;
        default:
            break;
    }
}

VideoSourceScreen::~VideoSourceScreen()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    StopScreenSourceCapture();
    encoderSurface_ = nullptr;
}

int32_t VideoSourceScreen::InitScreenSource(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (encoderSurface_ == nullptr) {
        SHARING_LOGE("encoderSurface_ is null!");
        return ERR_NULL_ENCODERSURFACE;
    }
    srcScreenId_ = configure.srcScreenId_;
    int32_t ret = RegisterScreenGroupListener();
    if (ret != ERR_OK) {
        SHARING_LOGE("RegisterScreenGroupListener failed, ret: %{public}d.", ret);
        return ret;
    }
    uint64_t screenId = CreateVirtualScreen(configure);
    if (screenId == SCREEN_ID_INVALID) {
        SHARING_LOGE("CreateVirtualScreen failed.");
        return ERR_INVALID_SCREENID;
    }
    return ERR_OK;
}

int32_t VideoSourceScreen::ReleaseScreenBuffer() const
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    return ERR_OK;
}

int32_t VideoSourceScreen::RegisterScreenGroupListener()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (screenGroupListener_ == nullptr) {
        screenGroupListener_ = new ScreenGroupListener();
    }
    auto ret = Rosen::ScreenManager::GetInstance().RegisterScreenGroupListener(screenGroupListener_);
    if (ret != OHOS::Rosen::DMError::DM_OK) {
        SHARING_LOGE("RegisterScreenGroupListener Failed!");
        screenGroupListener_ = nullptr;
        return ERR_REGISTER_FAILURE;
    }
    SHARING_LOGI("Register successed!");
    return ERR_OK;
}

int32_t VideoSourceScreen::UnregisterScreenGroupListener() const
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (screenGroupListener_ == nullptr) {
        SHARING_LOGE("screenGroupListener_ is null!");
        return ERR_NULL_LISTENER;
    }

    auto ret = Rosen::ScreenManager::GetInstance().UnregisterScreenGroupListener(screenGroupListener_);
    if (ret != OHOS::Rosen::DMError::DM_OK) {
        SHARING_LOGE("UnregisterScreenGroupListener Failed!");
        return ERR_UNREGISTER_FAILURE;
    }

    return ERR_OK;
}

void VideoSourceScreen::SetMultiScreenMode(Rosen::MultiScreenMode mode)
{
    SHARING_LOGI("In, mode:%{public}d", static_cast<int>(mode));
    if (screenId_ == 0) {
        SHARING_LOGI("screenId_ is invalid.");
        return;
    }
    sptr<Rosen::Display> defaultDisplay = Rosen::DisplayManager::GetInstance().GetDefaultDisplay();
    if (defaultDisplay == nullptr) {
        SHARING_LOGI("defaultDisplay is nullptr.");
        return;
    }
    uint64_t mainRsId;
    uint64_t secondRsId;
    if (!Rosen::DisplayManager::GetInstance().ConvertScreenIdToRsScreenId(defaultDisplay->GetScreenId(), mainRsId) ||
        !Rosen::DisplayManager::GetInstance().ConvertScreenIdToRsScreenId(screenId_, secondRsId)) {
        SHARING_LOGI("ConvertScreenIdToRsScreenId failed.");
        return;
    }

    DMError ret = Rosen::ScreenManager::GetInstance().SetMultiScreenMode(mainRsId, secondRsId, mode);
    if (ret != DMError::DM_OK) {
        SHARING_LOGI("SetMultiScreenModer failed, ret: %{public}d", ret);
    }
}

void VideoSourceScreen::SetProjectionDisplay(uint32_t mode, uint32_t rotation, Rect mainScreenRegion)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (mode == 5) { // 5:mirror
        SetMultiScreenMode(Rosen::MultiScreenMode::SCREEN_MIRROR);
        return;
    }

    if (mode == 4) { // 4:extend
        SetMultiScreenMode(Rosen::MultiScreenMode::SCREEN_EXTEND);
        return;
    }

    if (static_cast<Rosen::Rotation>(rotation) != Rosen::Rotation::ROTATION_0) {
        SHARING_LOGI("MakeMirrorByScreenIds");
        MakeMirrorByScreenIds();
        return;
    }

    if (mode == static_cast<uint32_t>(ProjectionDisplay::DISPLAY_BC)) {
        MakeMirrorByScreenIds();
        return;
    }

    Rosen::DMRect mainScreenDMRegion;
    mainScreenDMRegion.posX_ = mainScreenRegion.x;
    mainScreenDMRegion.posY_ = mainScreenRegion.y;
    uint32_t widthUint = (uint32_t)mainScreenRegion.w;
    uint32_t heightUint = (uint32_t)mainScreenRegion.h;
    mainScreenDMRegion.width_ = widthUint;
    mainScreenDMRegion.height_ = heightUint;
    MakeMirrorByRect(mainScreenDMRegion);
}

void VideoSourceScreen::MakeMirrorByScreenIds()
{
    SHARING_LOGI("In");
    sptr<Rosen::Display> defaultDisplay = OHOS::Rosen::DisplayManager::GetInstance().GetDefaultDisplay();
    if (defaultDisplay == nullptr) {
        SHARING_LOGE("defaultDisplay is nullptr.");
        return;
    }
    std::vector<uint64_t> mirrorIds{ screenId_ };
    Rosen::ScreenId groupId;
    OHOS::Rosen::DMError ret =
        Rosen::ScreenManager::GetInstance().MakeMirror(defaultDisplay->GetScreenId(), mirrorIds, groupId);
    if (ret != OHOS::Rosen::DMError::DM_OK || groupId == SCREEN_ID_INVALID) {
        SHARING_LOGE("Make mirror failed.");
    }
}

void VideoSourceScreen::MakeMirrorByRect(Rosen::DMRect mainScreenRegion)
{
    // 规格中限制，使用DMRect做Mirror，会出现虚拟屏不旋转的情况，旋转场景不建议使用
    sptr<Rosen::Display> defaultDisplay = OHOS::Rosen::DisplayManager::GetInstance().GetDefaultDisplay();
    if (defaultDisplay == nullptr) {
        SHARING_LOGE("defaultDisplay is nullptr.");
        return;
    }
    SHARING_LOGI("(x,y,w,h)=(%{public}d,%{public}d,%{public}d,%{public}d,)",
                 mainScreenRegion.posX_, mainScreenRegion.posY_,
                 mainScreenRegion.width_, mainScreenRegion.height_);
    Rosen::ScreenId groupId;
    std::vector<uint64_t> mirrorIds{ screenId_ };
    OHOS::Rosen::DMError ret = Rosen::ScreenManager::GetInstance().MakeMirror(defaultDisplay->GetScreenId(),
        mirrorIds, mainScreenRegion, groupId);
    if (ret != OHOS::Rosen::DMError::DM_OK || groupId == SCREEN_ID_INVALID) {
        SHARING_LOGE("Make mirror failed.");
        return;
    }
}

void VideoSourceScreen::SetScreenPixelFormat(uint64_t screenId)
{
    bool isYuvSupport = OHOS::system::GetBoolParameter(PARAM_YUV_SUPPORT, false);
    if (!isYuvSupport) {
        return;
    }
    SHARING_LOGI("support yuv format");
    sptr<Rosen::Screen> screen = Rosen::ScreenManager::GetInstance().GetScreenById(screenId);
    if (screen == nullptr) {
        SHARING_LOGE("screen is nullptr");
        return;
    }
    DMError error = screen->SetPixelFormat(OHOS::GRAPHIC_PIXEL_FMT_YCBCR_420_SP);
    if (error != DMError::DM_OK) {
        SHARING_LOGE("SetPixelFormat error:%{public}d", error);
    }
}

bool VideoSourceScreen::IsPcMode()
{
    SHARING_LOGI("IsPcMode in");
    if (system::GetParameter("const.window.support_window_pcmode_switch", "false") == "true") {
        if (system::GetParameter("persist.sceneboard.ispcmode", "false") == "true") {
            SHARING_LOGI("Current mode is PC mode.");
            return true;
        }
    }
    return false;
}

bool VideoSourceScreen::SetVirtualScreenId(int32_t &virtualScreenId)
{
    SHARING_LOGI("Start Find and Set Available ScreenId.");
    auto displayIds = Rosen::DisplayManager::GetInstance().GetAllDisplayIds();
    for (int32_t id = 500; id <= 900; ++id) {
        if (std::find(displayIds.begin(), displayIds.end(), id) != displayIds.end()) {
            continue;
        }
        virtualScreenId = id;
        SHARING_LOGI("VirtualDisplay ScreenId_: %{public}d", virtualScreenId);
        return true;
    }
    SHARING_LOGI("No available virtual screen ID.");
    return false;
}

Rosen::VirtualScreenOption VideoSourceScreen::CreateVirtualScreenOption(const VideoSourceConfigure &configure)
{
    std::string screenName = "CastEngine";
    float density = DEFAULT_SCREEN_DENSITY;
    Rosen::VirtualScreenOption option = {screenName,
                                         configure.screenWidth_,
                                         configure.screenHeight_,
                                         density,
                                         nullptr,
                                         DEFAULT_SCREEN_FLAGS,
                                         false,
                                         {},
                                         Rosen::VirtualScreenType::UNKNOWN,
                                         true,
                                         Rosen::VirtualScreenFlag::DEFAULT,
                                         true,
                                         true,
                                         configure.serialNumber_};
    option.bundleName_ = "cast";
    if (configure.isPc_) {
        SHARING_LOGI("set pc mode flag");
        option.virtualScreenFlag_ = Rosen::VirtualScreenFlag::DEFAULT;
        dpi_ =  GetExtendScreenDpi(configure.screenWidth_, configure.screenHeight_);
        density = static_cast<float>(dpi_) / DEFAULT_DPI_TIMES;
        option.density_ = density;
    } else {
        option.virtualScreenFlag_ = Rosen::VirtualScreenFlag::CAST;
    }
    return option;
}

uint32_t VideoSourceScreen::GetExtendScreenDpi(uint32_t castWidth, uint32_t castHeight)
{
    auto defaultDisplay = Rosen::DisplayManager::GetInstance().GetDefaultDisplay();
    if (defaultDisplay == nullptr) {
        SHARING_LOGE("default display is null");
        return DEFAULT_SCREEN_DENSITY;
    }

    std::vector<OHOS::Rosen::DisplayPhysicalResolution> physicalSizeArray =
            Rosen::DisplayManager::GetInstance().GetAllDisplayPhysicalResolution();
    uint32_t maxDeviceSize = 0;
    for (const auto &dpr: physicalSizeArray) {
        uint64_t widthSq = static_cast<uint64_t>(dpr.physicalWidth_) * dpr.physicalWidth_;
        uint64_t heightSq = static_cast<uint64_t>(dpr.physicalHeight_) * dpr.physicalHeight_;
        uint64_t sum = widthSq + heightSq;
        uint32_t diagonal = static_cast<uint32_t>(std::sqrt(static_cast<double>(sum)));
        maxDeviceSize = std::max(diagonal, maxDeviceSize);
    }

    if (maxDeviceSize == 0) {
        SHARING_LOGE("get device width error.");
        return DEFAULT_SCREEN_DENSITY;
    }

    uint64_t castWidthSq = static_cast<uint64_t>(castWidth) * castWidth;
    uint64_t castHeightSq = static_cast<uint64_t>(castHeight) * castHeight;
    double tvSize = std::sqrt(static_cast<double>(castWidthSq + castHeightSq));
    uint32_t dpi = static_cast<uint32_t>(std::round(defaultDisplay->GetDpi() * tvSize / maxDeviceSize));
    dpi = std::max(80u, std::min(640u, dpi)); // [80, 640]
    SHARING_LOGI("get extend screen dpi %{public}u", dpi);
    return dpi;
}

void VideoSourceScreen::SetupVirtualScreenMirror(const VideoSourceConfigure &configure)
{
    std::vector<uint64_t> mirrorIds;
    mirrorIds.push_back(screenId_);
    if (configure.isPc_) {
        SetMultiScreenMode(configure.mode_ == 2 ? Rosen::MultiScreenMode::SCREEN_EXTEND
                                                : Rosen::MultiScreenMode::SCREEN_MIRROR);
    } else {
        Rosen::ScreenId groupId = 0;
        Rosen::DMError err = Rosen::ScreenManager::GetInstance().MakeMirror(srcScreenId_, mirrorIds, groupId);
        if (err != Rosen::DMError::DM_OK) {
            SHARING_LOGE("MakeMirror failed, screenId:%{public}" PRIu64 "!", screenId_);
        }
    }
}

void VideoSourceScreen::SetupVirtualScreenDpi(const VideoSourceConfigure &configure)
{
    sptr<Rosen::Screen> virtualScreen = Rosen::ScreenManager::GetInstance().GetScreenById(screenId_);
    if (virtualScreen != nullptr && dpi_ > 0) {
        virtualScreen->SetDefaultDensityDpi(dpi_);
    }
}

void VideoSourceScreen::SetupVirtualScreenRefreshRate(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("frameRate_:%{public}f.", configure.frameRate_);
    uint32_t actualRefreshRate;
    DMError ret = DMError::DM_ERROR_UNKNOWN;
    if (configure.frameRate_ == HIVISION_FRAME_RATE) {
        ret = Rosen::ScreenManager::GetInstance().SetVirtualScreenMaxRefreshRate(screenId_, REFRESH_60FPS,
                                                                                 actualRefreshRate);
    } else {
        ret = Rosen::ScreenManager::GetInstance().SetVirtualScreenMaxRefreshRate(screenId_, REFRESH_30FPS,
                                                                                 actualRefreshRate);
    }
    if (ret != DMError::DM_OK) {
        SHARING_LOGE("Set screen refresh rate failed, ret: %{public}d.", ret);
    }
}

uint64_t VideoSourceScreen::CreateVirtualScreen(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("CreateVirtualScreen, width: %{public}u, height: %{public}u.", configure.screenWidth_,
                 configure.screenHeight_);
    
    Rosen::VirtualScreenOption option = CreateVirtualScreenOption(configure);
    SetVirtualScreenId(option.screenId_);
    screenId_ = Rosen::ScreenManager::GetInstance().CreateVirtualScreen(option);
    SHARING_LOGI("virtualScreen id is: %{public}" PRIu64 ". dpi: %{public}f", screenId_, option.density_);
    if (screenId_ == SCREEN_ID_INVALID) {
        SHARING_LOGE("CreateVirtualScreen failed.");
        return screenId_;
    }

    Rosen::ScreenManager::GetInstance().SetVirtualScreenSurface(screenId_, encoderSurface_);
    OHOS::Rosen::DisplayManager::GetInstance().DisablePowerOffRenderControl(screenId_);
    SetScreenPixelFormat(screenId_);
    
    SetupVirtualScreenMirror(configure);
    SetupVirtualScreenDpi(configure);
    SetupVirtualScreenRefreshRate(configure);

    return screenId_;
}

int32_t VideoSourceScreen::DestroyVirtualScreen() const
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (screenId_ == SCREEN_ID_INVALID) {
        SHARING_LOGE("Failed, invalid screenId!");
        return ERR_INVALID_SCREENID;
    }

    SHARING_LOGI("Destroy virtual screen, screenId: %{public}" PRIu64 ".", screenId_);

    std::vector<uint64_t> mirrorIds{ screenId_ };
    DMError ret = Rosen::ScreenManager::GetInstance().StopMirror(mirrorIds);
    if (ret != DMError::DM_OK) {
        SHARING_LOGE("Stop mirror failed.");
    }
    Rosen::ScreenManager::GetInstance().RemoveVirtualScreenFromGroup(mirrorIds);
    Rosen::DMError err = Rosen::ScreenManager::GetInstance().DestroyVirtualScreen(screenId_);
    if (err != Rosen::DMError::DM_OK) {
        SHARING_LOGE("Destroy virtual screen failed, screenId:%{public}" PRIu64 "!", screenId_);
    }

    auto &rsInterfaces = Rosen::RSInterfaces::GetInstance();
    Rosen::EventInfo event = {VOTER_NAME, false, REFRESH_120FPS, REFRESH_120FPS, VIRTUAL_DISPLAY_NAME};
    rsInterfaces.NotifyRefreshRateEvent(event);
    return ERR_OK;
}

void VideoSourceScreen::StartScreenSourceCapture()
{
    SHARING_LOGI("StartScreenSourceCapture.");
    Rosen::ScreenManager::GetInstance().SetVirtualMirrorScreenCanvasRotation(screenId_, true);
    screenListener_ = sptr<ScreenListener>::MakeSptr(screenId_);
    if (screenListener_) {
        DMError ret = Rosen::ScreenManager::GetInstance().RegisterScreenListener(screenListener_);
        if (ret != DMError::DM_OK) {
            SHARING_LOGE("RegisterScreenListener failed, ret: %{public}d.", ret);
        }
    }
    RequestFrame();
    SHARING_LOGI("StartScreenSourceCapture out.");
}

void VideoSourceScreen::RequestFrame()
{
    SHARING_LOGI("RequestFrame");
    auto self = shared_from_this();
    std::thread requestThread = std::thread([self] {
        for (int i = 0; i <= REQUEST_FRAME_COUNT; ++i) {
            SHARING_LOGI("Request frame count %{public}d.", i);
            Rosen::ScreenManager::GetInstance().SetVirtualScreenSurface(self->screenId_, self->encoderSurface_);
            std::this_thread::sleep_for(std::chrono::milliseconds(REQUEST_FRAME_INTERVAL));
        }
    });
    std::string name = "requestFrame";
    pthread_setname_np(requestThread.native_handle(), name.c_str());
    requestThread.detach();
}

bool VideoSourceScreen::ChangeScreenRefreshRate(int32_t interval)
{
    SHARING_LOGI("ChangeScreenRefreshRate");
    return true;
}

void VideoSourceScreen::StopScreenSourceCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);

    int32_t ret = UnregisterScreenGroupListener();
    if (ret == ERR_OK) {
        SHARING_LOGI("UnregisterScreenGroupListener success!");
        screenGroupListener_ = nullptr;
    }

    if (screenListener_ != nullptr) {
        DMError res = Rosen::ScreenManager::GetInstance().UnregisterScreenListener(screenListener_);
        if (res != DMError::DM_OK) {
            SHARING_LOGE("UnregisterScreenListener failed, res: %{public}d.", res);
        } else {
            screenListener_ = nullptr;
        }
    }

    ret = DestroyVirtualScreen();
    if (ret != ERR_OK) {
        SHARING_LOGE("Destroy virtual screen failed!");
    } else {
        screenId_ = SCREEN_ID_INVALID;
        SHARING_LOGI("Destroy virtual screen success!");
    }
}

int32_t VideoSourceScreen::SetEncoderSurface(sptr<OHOS::Surface> surface)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (screenId_ == SCREEN_ID_INVALID) {
        SHARING_LOGE("Failed, invalid screenId!");
        return ERR_INVALID_SCREENID;
    }
    if (surface == nullptr) {
        SHARING_LOGE("Surface is nullptr!");
        return ERR_NULL_SURFACE;
    }
    SHARING_LOGI("Set surface for virtual screen, screenId: %{public}" PRIu64 ".", screenId_);
    Rosen::DMError err = Rosen::ScreenManager::GetInstance().SetVirtualScreenSurface(screenId_, surface);
    if (err != Rosen::DMError::DM_OK) {
        SHARING_LOGE("Set surface for virtual screen failed, screenId:%{public}" PRIu64 "!", screenId_);
        return ERR_SET_SURFACE_FAILURE;
    }
    return ERR_OK;
}

void VideoSourceScreen::RemoveScreenFromGroup() const
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (screenId_ == SCREEN_ID_INVALID) {
        SHARING_LOGE("Failed, invalid screenId!");
        return;
    }
    SHARING_LOGI("Remove screen from group, screenId: %{publid}" PRIu64 ".", screenId_);
    std::vector<uint64_t> screenIds;
    screenIds.push_back(screenId_);
    Rosen::DMError err = Rosen::ScreenManager::GetInstance().RemoveVirtualScreenFromGroup(screenIds);
    if (err != Rosen::DMError::DM_OK) {
        SHARING_LOGE("Remove screen from group failed, screenId:%{public}" PRIu64 "!", screenId_);
    }
}

void VideoSourceScreen::SetRefreshRateVote(const std::string &jsonParam)
{
    SHARING_LOGI("param: %{public}s.", jsonParam.c_str());
    if (!json::accept(jsonParam)) {
        return;
    }
    json jsonObject = json::parse(jsonParam);
    if (!jsonObject.contains(KEY_VOTING) || !jsonObject.contains(KEY_MIN_REFRESH_RATE) ||
        !jsonObject.contains(KEY_MXA_REFRESH_RATE)) {
        SHARING_LOGE("set refresh rate vote json param error");
        return;
    }
    if (!jsonObject[KEY_VOTING].is_number() || !jsonObject[KEY_MIN_REFRESH_RATE].is_number()||
        !jsonObject[KEY_MXA_REFRESH_RATE].is_number()) {
        SHARING_LOGE("set refresh rate vote json param error");
        return;
    }
    uint32_t voting = jsonObject[KEY_VOTING];
    uint32_t minRefreshRate = jsonObject[KEY_MIN_REFRESH_RATE];
    uint32_t maxRefreshRate = jsonObject[KEY_MXA_REFRESH_RATE];
    if (minRefreshRate < VIDEO_FPS_30 || maxRefreshRate < minRefreshRate) {
        SHARING_LOGE("SetRefreshRateVote params is invalid.");
        return;
    }
    auto &rsInterfaces = Rosen::RSInterfaces::GetInstance();
    Rosen::EventInfo event = {VOTER_NAME, voting == 1, minRefreshRate, maxRefreshRate, VIRTUAL_DISPLAY_NAME};
    rsInterfaces.NotifyRefreshRateEvent(event);
}

uint64_t VideoSourceScreen::GetScreenId()
{
    return screenId_;
}
} // namespace Sharing
} // namespace OHOS
