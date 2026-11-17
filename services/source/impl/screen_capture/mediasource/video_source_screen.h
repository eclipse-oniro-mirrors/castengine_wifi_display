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

#ifndef OHOS_SHARING_VIDEO_SOURCE_SCREEN_H
#define OHOS_SHARING_VIDEO_SOURCE_SCREEN_H

#include <surface.h>
#include <thread>
#include "dm_common.h"
#include "screen_manager.h"
#include "sync_fence.h"
#include "timer.h"
#include "video_source_encoder.h"
#include "dm/display_manager.h"
#include "dm/display.h"
namespace OHOS {
namespace Sharing {

class VideoSourceScreen : public std::enable_shared_from_this<VideoSourceScreen> {
public:
    class ScreenGroupListener : public Rosen::ScreenManager::IScreenGroupListener {
    public:
        void OnChange(const std::vector<uint64_t> &screenIds, Rosen::ScreenGroupChangeEvent event) override;
    };

    explicit VideoSourceScreen(sptr<OHOS::Surface> encoderSurface) : encoderSurface_(encoderSurface) {}
    ~VideoSourceScreen();

    int32_t ReleaseScreenBuffer() const;
    int32_t InitScreenSource(const VideoSourceConfigure &configure);

    void StopScreenSourceCapture();
    bool ChangeScreenRefreshRate(int32_t interval);
    void StartScreenSourceCapture();
    void SetProjectionDisplay(uint32_t mode, uint32_t rotation, Rect mainScreenRegion);
    void MakeMirrorByRect(Rosen::DMRect mainScreenRegion);
    void MakeMirrorByScreenIds();
    void RequestFrame();
    void SetRefreshRateVote(const std::string &jsonParam);
    bool IsPcMode();
    uint64_t GetScreenId();

private:
#ifdef WFD_SINK
    void RemoveScreenFromGroup() const;
#endif
    void FrameRateControlTimerWorker();

    int32_t DestroyVirtualScreen() const;
    int32_t RegisterScreenGroupListener();
    int32_t UnregisterScreenGroupListener() const;
#ifdef WFD_SINK
    int32_t SetEncoderSurface(sptr<OHOS::Surface> surface);
#endif
    void SetScreenPixelFormat(uint64_t screenId);
    uint64_t CreateVirtualScreen(const VideoSourceConfigure &configure);
    void SetMultiScreenMode(Rosen::MultiScreenMode mode);
    bool SetVirtualScreenId(int32_t &virtualScreenId);
    Rosen::VirtualScreenOption CreateVirtualScreenOption(const VideoSourceConfigure &configure);
    void SetupVirtualScreenMirror(const VideoSourceConfigure &configure);
    void SetupVirtualScreenDpi(const VideoSourceConfigure &configure);
    void SetupVirtualScreenRefreshRate(const VideoSourceConfigure &configure);
    uint32_t GetExtendScreenDpi(uint32_t castWidth, uint32_t castHeight);

private:
    uint64_t screenId_ = SCREEN_ID_INVALID;
    uint64_t srcScreenId_ = SCREEN_ID_INVALID;
    static constexpr uint32_t REFRESH_30FPS = 30;
    static constexpr uint32_t REFRESH_60FPS = 60;
    static constexpr uint32_t REFRESH_120FPS = 120;
    static constexpr int32_t REQUEST_FRAME_COUNT = 5;
    static constexpr int32_t REQUEST_FRAME_INTERVAL = 55;
    static constexpr uint32_t DEFAULT_DPI_TIMES = 160;
    OHOS::sptr<OHOS::Surface> encoderSurface_ = nullptr;
    OHOS::sptr<ScreenGroupListener> screenGroupListener_ = nullptr;
    OHOS::sptr<Rosen::ScreenManager::IScreenListener> screenListener_ = nullptr;
    uint32_t dpi_ = 0;
};
} // namespace Sharing
} // namespace OHOS
#endif
