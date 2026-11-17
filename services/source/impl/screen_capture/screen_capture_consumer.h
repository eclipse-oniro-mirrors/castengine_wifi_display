/*
 * Copyright (c) 2023-2024 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#ifndef OHOS_SHARING_SCREEN_CAPTURE_CONSUMER_H
#define OHOS_SHARING_SCREEN_CAPTURE_CONSUMER_H

#include <mutex>
#include "audio_aac_encoder.h"
#include "audio_capturer.h"
#include "source_codec_factory.h"
#include "magic_enum.hpp"
#include "mediachannel/base_consumer.h"
#include "video_source_encoder.h"
#include "video_source_screen.h"

namespace OHOS {
namespace Sharing {
class ScreenCaptureConsumer : public BaseConsumer,
                              public VideoSourceEncoderListener,
                              public std::enable_shared_from_this<ScreenCaptureConsumer> {
public:
    class AudioEncoderReceiver : public FrameDestination {
    public:
        explicit AudioEncoderReceiver(std::weak_ptr<ScreenCaptureConsumer> parent) : parent_(parent){};

        void OnFrame(const Frame::Ptr &frame) override;

    private:
        std::weak_ptr<ScreenCaptureConsumer> parent_;
        uint64_t DebugPts = 0;
    };

    ScreenCaptureConsumer();
    ~ScreenCaptureConsumer();

public:
    int32_t Release() override;
    int32_t ReleaseScreenBuffer();
    int32_t HandleEvent(SharingEvent &event) override;

    void OnInitVideoCaptureError();
    void OnFrameBufferUsed() override;
    void UpdateOperation(ProsumerStatusMsg::Ptr &statusMsg) override;
    void OnFrame(const Frame::Ptr &frame, FRAME_TYPE frameType, bool keyFrame) override;

private:
    bool IsPaused();
    bool StopCapture();
    bool StartCapture();
    bool StopAudioCapture();
    bool StopVideoCapture();
    bool StartAudioCapture();
    bool StartVideoCapture();

    bool InitAudioCapture();
    bool InitAudioEncoder();
    bool InitCapture(uint64_t screenId);
    bool InitVideoCapture(uint64_t screenId);

    void AudioCaptureThreadWorker();

    void HandleProsumerInitState(SharingEvent &event);
    void HandleProsumerPlay(SharingEvent &event);
    void HandleProsumerSetAudio(SharingEvent &event);
    void HandleProsumerSetDisplay(SharingEvent &event);
    void HandleProsumerEvent(SharingEvent &event);
    void HandleProsumerRequestIdr(SharingEvent &event);

    bool IsSilentFrame(const uint8_t *buffer, size_t bufLen);
    void HandleUpdateBitrate(const std::string &jsonParam);
    void NotifyMiracastScreenId(int32_t eventId, SharingEvent &event);
    void HandleScreenParam(std::string &jsonParam);
    void HandleDealVirtualScreen(std::string &jsonParam);

private:
    std::atomic<bool> paused_ = false;

    std::mutex mutex_;

    std::shared_ptr<VideoSourceScreen> videoSourceScreen_ = nullptr;
    std::shared_ptr<VideoSourceEncoder> videoSourceEncoder_ = nullptr;

    size_t audioBufferLen_ = 0;
    std::shared_ptr<AudioEncoder> audioEncoder_ = nullptr;
    std::unique_ptr<std::thread> audioCaptureThread_ = nullptr;
    std::shared_ptr<AudioEncoderReceiver> audioEncoderReceiver_ = nullptr;
    std::unique_ptr<AudioStandard::AudioCapturer> audioCapturer_ = nullptr;

    bool isHiVisionScreen_ = false;
    bool firstVideoFrame_ = true;
    uint64_t lastPts_ = 0;
    uint64_t lastRealPts_ = 0;
    uint64_t frameCount_ = 0;
    uint64_t currentSecond_ = 0;
    uint64_t lowInterval_ = LOW_PTS_INTERVAL;
    uint64_t highInterval_ = HIGH_PTS_INTERVAL;
    uint32_t audioFrameCount_ = 0;
    uint32_t silentFrameCount_ = 0;
    int32_t dpi_ = 0;
    int32_t mode_ = 0;
    bool isPc_ = false;
    std::string serialNumber_ = "";
};

} // namespace Sharing
} // namespace OHOS
#endif
