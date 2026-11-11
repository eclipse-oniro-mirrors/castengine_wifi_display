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

#ifndef OHOS_SHARING_SCREEN_CAPTURE_DEF_H
#define OHOS_SHARING_SCREEN_CAPTURE_DEF_H

#include "common/event_comm.h"
#include "common/event_channel.h"

namespace OHOS {
namespace Sharing {

struct ScreenCaptureSessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureSessionEventMsg>;

    uint64_t screenId = 0;
    MediaType mediaType;
    CodecId codecId;
    CodecId videoCodecId = CodecId::CODEC_H264;
    AudioFormat audioFormat = AUDIO_8000_8_1;
    VideoFormat videoFormat = VIDEO_1280X720_30;
};

struct SetAudioSessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<SetAudioSessionEventMsg>;
    bool isAudioOn = true;
};

struct SetDisplaySessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<SetDisplaySessionEventMsg>;
    uint32_t projectMode = 0;
    uint32_t projectRotation = 0;
    Rect mainScreenRegion;
};

struct CommonSessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<CommonSessionEventMsg>;
    int32_t eventId = 0;
    std::string eventParam;
};

struct AppendSurfaceSessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<AppendSurfaceSessionEventMsg>;
    sptr<IRemoteObject> surface;
};

struct ScreenCaptureConsumerEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerEventMsg>;

    uint64_t screenId = 0;
    AudioTrack audioTrack;
    VideoTrack videoTrack;
    CodecId videoCodecId = CodecId::CODEC_H264;
};

struct ScreenCaptureConsumerAudioEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerEventMsg>;
    bool isAudioOn = true;
};

struct ScreenCaptureConsumerDisplayEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerEventMsg>;
    uint32_t projectMode = 0;
    uint32_t projectRotation = 0;
    Rect mainScreenRegion;
};

struct SetRefreshRateSessionEventMsg : public SessionEventMsg {
    using Ptr = std::shared_ptr<SetRefreshRateSessionEventMsg>;
    int32_t interval = 0;
};

struct ScreenCaptureConsumerRefreshEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerRefreshEventMsg>;
    int32_t interval = 0;
};

struct ScreenCaptureConsumerCommonEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerCommonEventMsg>;
    int32_t eventId = 0;
    std::string eventParam;
};

struct ScreenCaptureConsumerSurfaceEventMsg : public ChannelEventMsg {
    using Ptr = std::shared_ptr<ScreenCaptureConsumerSurfaceEventMsg>;
    sptr<IRemoteObject> surface;
};

} // namespace Sharing
} // namespace OHOS
#endif
