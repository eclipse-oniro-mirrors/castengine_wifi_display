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

#include "screen_capture_consumer.h"
#include <cstdlib>
#include <dlfcn.h>
#include <sys/stat.h>
#include "common/common_macro.h"
#include "common/reflect_registration.h"
#include "common/sharing_log.h"
#include "screen_capture_def.h"
#include "utils/utils.h"
#include "json.hpp"

// using OHOS::AudioStandard::StreamUsage;

namespace {
constexpr int32_t CAPTURER_FLAG = 0;
static const std::string SOURCE_LIBRARY_PATH =
    std::string(SYSTEM_LIB_PATH) + std::string("platformsdk/libaudio_policy_ext_client.z.so");
} // namespace

namespace OHOS {
namespace Sharing {
using nlohmann::json;

static std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
void ScreenCaptureConsumer::AudioEncoderReceiver::OnFrame(const Frame::Ptr &frame)
{
    auto parent = parent_.lock();
    if (parent && !parent->listener_.expired()) {
        auto listener = parent->listener_.lock();
        RETURN_IF_NULL(listener);
        auto dispatcher = listener->GetDispatcher();
        if (dispatcher) {
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            uint64_t pts =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
            auto mediaData = dispatcher->RequestDataBuffer(MEDIA_TYPE_AUDIO, frame->Size());
            if (mediaData == nullptr) {
                SHARING_LOGE("mediaData read failed.");
                return;
            }
            mediaData->mediaType = MEDIA_TYPE_AUDIO;
            mediaData->codecId = frame->GetCodecId();
            mediaData->isRaw = false;
            mediaData->keyFrame = false;
            mediaData->pts = pts;
            if (DebugPts != 0 && pts < DebugPts) {
                DebugPts = 0;
            }
            if ((DebugPts == 0) || (pts - DebugPts > WFD_MSEC_IN_SEC)) {
                SHARING_LOGE("recv audio encode data: codec:%{public}d, pts:%{public} " PRId64
                             ", size: %{public}d, frame count: %{public}u, silent frame count: %{public}u",
                             mediaData->codecId, pts, frame->Size(), parent->audioFrameCount_,
                             parent->silentFrameCount_);
                DebugPts = pts;
                parent->audioFrameCount_ = 0;
                parent->silentFrameCount_ = 0;
            }
            mediaData->buff = move(frame);
            dispatcher->InputData(mediaData);
        }
    }
}

void ScreenCaptureConsumer::OnFrameBufferUsed()
{
    ReleaseScreenBuffer();
}

void ScreenCaptureConsumer::OnFrame(const Frame::Ptr &frame, FRAME_TYPE frameType, bool keyFrame)
{
    if (frame == nullptr || listener_.expired() || IsPaused()) {
        return;
    }

    auto listener = listener_.lock();
    RETURN_IF_NULL(listener);
    BufferDispatcher::Ptr dispatcher = listener->GetDispatcher();
    if (dispatcher == nullptr) {
        return;
    }
    uint32_t len = (uint32_t)frame->Size();
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    uint64_t realPts =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    uint64_t pts;
    if (firstVideoFrame_) {
        pts = realPts;
        firstVideoFrame_ = false;
    } else if (lastPts_ > lastRealPts_ + LARGE_AVPTS_DIFF || lastPts_ + LARGE_AVPTS_DIFF <= lastRealPts_) {
        SHARING_LOGE("Large diff, pts: %{public}" PRIu64 ", realpts: %{public}" PRIu64 ".", lastPts_, lastRealPts_);
        pts = realPts;
    } else if (lastPts_ > lastRealPts_) {
        pts = lastPts_ + lowInterval_;
    } else {
        pts = lastPts_ + highInterval_;
    }
    lastPts_ = pts;
    lastRealPts_ = realPts;
    uint64_t currentSecond = realPts / (uint64_t)MILLISECOND;
    if (currentSecond_ != currentSecond) {
        if (frameCount_ < FRAME_RATE_60 && (uint64_t)videoTrack_.frameRate > frameCount_ + 1) {
            SHARING_LOGE("OnFrame fps: %{public}" PRIu64 ".", frameCount_);
        }
        frameCount_ = 0;
        currentSecond_ = currentSecond;
    }
    frameCount_++;
	auto mediaData = dispatcher->RequestDataBuffer(MEDIA_TYPE_VIDEO, len);
	if (mediaData == nullptr) {
		return;
	}
	mediaData->mediaType = MEDIA_TYPE_VIDEO;
    mediaData->codecId = frame->GetCodecId();
	mediaData->isRaw = false;
	mediaData->keyFrame = keyFrame;
	mediaData->pts = pts;
	if (keyFrame) {
		SHARING_LOGE("[%{public}" PRId64 "] capture a video len:%{public}u.", mediaData->pts, len);
	}
	mediaData->buff = move(frame);
	dispatcher->InputData(mediaData);
}

ScreenCaptureConsumer::ScreenCaptureConsumer()
{
    SHARING_LOGI("capture consumer Id: %{public}u.", GetId());
}

ScreenCaptureConsumer::~ScreenCaptureConsumer()
{
    SHARING_LOGI("~ capture consumer Id: %{public}u.", GetId());
    Release();
}

void ScreenCaptureConsumer::HandleProsumerInitState(SharingEvent &event)
{
    SHARING_LOGI("HandleProsumerInitState");
    auto pPrivateMsg = std::make_shared<ScreenCaptureSessionEventMsg>();
    pPrivateMsg->errorCode = ERR_OK;
    pPrivateMsg->type = EVENT_AGENT_STATE_PROSUMER_INIT;
    pPrivateMsg->toMgr = ModuleType::MODULE_CONTEXT;
    pPrivateMsg->fromMgr = ModuleType::MODULE_MEDIACHANNEL;
    pPrivateMsg->srcId = GetId();
    pPrivateMsg->dstId = event.eventMsg->srcId;
    pPrivateMsg->agentId = GetSinkAgentId();
    pPrivateMsg->prosumerId = GetId();
    pPrivateMsg->requestId = event.eventMsg->requestId;
    NotifyPrivateEvent(pPrivateMsg);
}

void ScreenCaptureConsumer::HandleProsumerPlay(SharingEvent &event)
{
    SHARING_LOGI("HandleProsumerPlay");

    auto msg = ConvertEventMsg<ScreenCaptureConsumerEventMsg>(event);
    if (msg) {
        SHARING_LOGI("msg convert suc, vcodecId:%{public}d, acodecId:%{public}d.",
                     msg->videoTrack.codecId, msg->audioTrack.codecId);
        if (msg->audioTrack.codecId != CodecId::CODEC_NONE) {
            audioTrack_ = msg->audioTrack;
        }

        if (msg->videoTrack.codecId != CodecId::CODEC_NONE) {
            videoTrack_ = msg->videoTrack;
        }
        if (msg->videoCodecId != CodecId::CODEC_NONE) {
            videoTrack_.codecId = msg->videoCodecId;
        }
        if (isInit_) {
            SHARING_LOGE("Capture already inited!");
            return;
        }
        if (!InitCapture(msg->screenId)) {
            SHARING_LOGE("InitCapture failed!");
            return;
        }
        NotifyMiracastScreenId(EVENT_ID_SCREEN_CHANGE, event);
        if (!StartCapture()) {
            SHARING_LOGE("StartCapture failed!");
            return;
        }
    } else {
        SHARING_LOGE("msg convert fail!");
        return;
    }
    auto statusMsg = std::make_shared<ProsumerStatusMsg>();
    auto eventMsg = std::make_shared<SessionEventMsg>();
    statusMsg->eventMsg = std::move(eventMsg);
    statusMsg->errorCode = ERR_OK;
    statusMsg->status = PROSUMER_NOTIFY_PLAYED_SUCCESS;
    Notify(statusMsg);
}

void ScreenCaptureConsumer::NotifyMiracastScreenId(int32_t eventId, SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoSourceScreen_ == nullptr) {
        SHARING_LOGE("videoSourceScreen_ is null");
        return;
    }
    auto eventMsg = std::make_shared<CommonSessionEventMsg>();
    eventMsg->eventId = eventId;
    eventMsg->eventParam = std::to_string(videoSourceScreen_->GetScreenId());
    eventMsg->errorCode = ERR_OK;
    eventMsg->type = EVENT_AGENT_PROSUMER_COMMON;
    eventMsg->toMgr = ModuleType::MODULE_CONTEXT;
    eventMsg->fromMgr = ModuleType::MODULE_MEDIACHANNEL;
    eventMsg->srcId = GetId();
    eventMsg->dstId = event.eventMsg->srcId;
    eventMsg->agentId = GetSinkAgentId();
    eventMsg->prosumerId = GetId();
    eventMsg->requestId = 0;
    NotifyPrivateEvent(eventMsg);
}

void ScreenCaptureConsumer::HandleProsumerSetAudio(SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
}

void ScreenCaptureConsumer::HandleProsumerSetDisplay(SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    auto msg = ConvertEventMsg<ScreenCaptureConsumerDisplayEventMsg>(event);
    if (msg == nullptr) {
        SHARING_LOGE("HandleProsumerSetDisplay, convert event msg error");
        return;
    }
    SHARING_LOGI("msg projectMode: %{public}d, projectRotation: %{public}d.", msg->projectMode, msg->projectRotation);
    if (videoSourceScreen_ == nullptr) {
        SHARING_LOGE("videoSourceScreen_ is null");
        return;
    }
    videoSourceScreen_->SetProjectionDisplay(msg->projectMode, msg->projectRotation, msg->mainScreenRegion);
}

void ScreenCaptureConsumer::HandleProsumerRequestIdr(SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoSourceEncoder_ != nullptr) {
        videoSourceEncoder_->RequestKeyFrame();
    }
}

void ScreenCaptureConsumer::HandleUpdateBitrate(const std::string &jsonParam)
{
    SHARING_LOGI("param: %{public}s.", jsonParam.c_str());
    if (!json::accept(jsonParam)) {
        return;
    }
    json jsonObject = json::parse(jsonParam);
    if (!jsonObject.contains(KEY_BITRATE)) {
        SHARING_LOGE("update bitrate json param error");
        return;
    }
    if (!jsonObject[KEY_BITRATE].is_number_integer()) {
        SHARING_LOGE("update bitrate json param error");
        return;
    }
    int64_t bitrate = jsonObject[KEY_BITRATE];
    if (videoSourceEncoder_ != nullptr) {
        videoSourceEncoder_->UpdateBitrate(bitrate);
    }
}

void ScreenCaptureConsumer::HandleProsumerEvent(SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    auto msg = ConvertEventMsg<ScreenCaptureConsumerCommonEventMsg>(event);
    if (msg == nullptr) {
        SHARING_LOGE("HandleProsumerEvent, convert event msg error");
        return;
    }
    if (msg->eventId == EVENT_ID_SCREEN_PARAM) {
        HandleScreenParam(msg->eventParam);
        return;
    }
    if (videoSourceScreen_ == nullptr) {
        SHARING_LOGE("videoSourceScreen_ is null");
        return;
    }
    switch (msg->eventId) {
        case NOTIFY_EVENT_REFRESH_RATE_VOTE:
            videoSourceScreen_->SetRefreshRateVote(msg->eventParam);
            break;
        case UPDATE_BITRATE:
            HandleUpdateBitrate(msg->eventParam);
            break;
        case DEAL_VIRTUAL_SCREEN:
            HandleDealVirtualScreen(msg->eventParam);
            NotifyMiracastScreenId(EVENT_ID_SCREEN_CHANGE, event);
            break;
        default:
            break;
    }
}

void ScreenCaptureConsumer::HandleDealVirtualScreen(std::string &jsonParam)
{
    HandleScreenParam(jsonParam);
    StopCapture();
    if (!InitVideoCapture(0)) {
        SHARING_LOGE("capture consumerId: %{public}u, init video capture failed.", GetId());
        return;
    }
    if (!StartCapture()) {
        SHARING_LOGE("StartCapture failed!");
    }
}

void ScreenCaptureConsumer::HandleScreenParam(std::string &jsonParam)
{
    SHARING_LOGI("param: %{public}s.", jsonParam.c_str());
    if (!json::accept(jsonParam)) {
        return;
    }
    json jsonObject = json::parse(jsonParam);
    if (!jsonObject.contains("dpi") || !jsonObject.contains("mode") || !jsonObject.contains("isPc")) {
        SHARING_LOGE("set mode json param error");
        return;
    }
    if (!jsonObject["dpi"].is_number() || !jsonObject["mode"].is_number() || !jsonObject["isPc"].is_boolean()) {
        SHARING_LOGE("set mode json param error");
        return;
    }
    dpi_ = jsonObject["dpi"];
    mode_ = jsonObject["mode"];
    isPc_ = jsonObject["isPc"];
    if (jsonObject.contains("serialNumber") && jsonObject["serialNumber"].is_string()) {
        serialNumber_ = jsonObject["serialNumber"];
    }
}

int32_t ScreenCaptureConsumer::HandleEvent(SharingEvent &event)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    RETURN_INVALID_IF_NULL(event.eventMsg);

    SHARING_LOGI("eventType: %{public}s = %{public}d, capture consumerId: %{public}u.",
                 std::string(magic_enum::enum_name(event.eventMsg->type)).c_str(), event.eventMsg->type, GetId());
    switch (event.eventMsg->type) {
        case EventType::EVENT_SCREEN_CAPTURE_INIT:
            HandleProsumerInitState(event);
            break;
        case EventType::EVENT_SCREEN_CAPTURE_SET_AUDIO:
            HandleProsumerSetAudio(event);
            break;
        case EventType::EVENT_SCREEN_CAPTURE_SET_DISPLAY:
            HandleProsumerSetDisplay(event);
            break;
        case EventType::EVENT_SCREEN_REQUEST_IDR:
            HandleProsumerRequestIdr(event);
            break;
        case EventType::EVENT_WFD_NOTIFY_RTSP_PLAYED:
            HandleProsumerPlay(event);
            break;
        case EventType::EVENT_SCREEN_CAPTURE_COMMON:
            HandleProsumerEvent(event);
            break;
        default:
            SHARING_LOGI("none process case.");
            break;
    }

    return 0;
}

void ScreenCaptureConsumer::UpdateOperation(ProsumerStatusMsg::Ptr &statusMsg)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    RETURN_IF_NULL(statusMsg);

    SHARING_LOGI("status: %{public}s consumerId: %{public}u.",
                 std::string(magic_enum::enum_name(static_cast<ProsumerOptRunningStatus>(statusMsg->status))).c_str(),
                 GetId());
    switch (statusMsg->status) {
        case ProsumerOptRunningStatus::PROSUMER_INIT:
            statusMsg->status = PROSUMER_NOTIFY_INIT_SUCCESS;
            break;
        case ProsumerOptRunningStatus::PROSUMER_START: {
            paused_ = false;
            statusMsg->status = PROSUMER_NOTIFY_START_SUCCESS;
            break;
        }
        case ProsumerOptRunningStatus::PROSUMER_PAUSE: {
            paused_ = true;
            statusMsg->status = PROSUMER_NOTIFY_PAUSE_SUCCESS;
            break;
        }
        case ProsumerOptRunningStatus::PROSUMER_RESUME: {
            paused_ = false;
            statusMsg->status = PROSUMER_NOTIFY_RESUME_SUCCESS;
            break;
        }
        case ProsumerOptRunningStatus::PROSUMER_STOP: {
            if (StopCapture()) {
                statusMsg->status = PROSUMER_NOTIFY_STOP_SUCCESS;
            } else {
                statusMsg->status = PROSUMER_NOTIFY_ERROR;
                statusMsg->errorCode = ERR_PROSUMER_STOP;
            }
            break;
        }
        case ProsumerOptRunningStatus::PROSUMER_DESTROY:
            Release();
            SHARING_LOGI("prosumer release success, out.");
            statusMsg->status = PROSUMER_NOTIFY_DESTROY_SUCCESS;
            break;
        default:
            SHARING_LOGI("none process case.");
            break;
    }

    Notify(statusMsg);
}

int32_t ScreenCaptureConsumer::Release()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    StopCapture();

    SHARING_LOGI("consumerId: %{public}u, capture consumer release=out.", GetId());
    return 0;
}

bool ScreenCaptureConsumer::IsPaused()
{
    return paused_;
}

int32_t ScreenCaptureConsumer::ReleaseScreenBuffer()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoSourceScreen_ == nullptr) {
        SHARING_LOGE("%{public}s.,videoSourceScreen_ == nullptr", __FUNCTION__);
        return ERR_NULL_SCREEN;
    }
    return videoSourceScreen_->ReleaseScreenBuffer();
}

bool ScreenCaptureConsumer::InitCapture(uint64_t screenId)
{
    SHARING_LOGI("capture consumerId: %{public}u, viddeo codecId: %{public}d, audio codecId: %{public}d.", GetId(),
                 videoTrack_.codecId, audioTrack_.codecId);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!InitVideoCapture(0)) {
        SHARING_LOGE("capture consumerId: %{public}u, init video capture failed.", GetId());
        return false;
    }

    if (audioTrack_.codecId != CODEC_NONE) {
        if (!InitAudioEncoder()) {
            SHARING_LOGE("capture consumerId: %{public}u, init audio encoder failed.", GetId());
            return false;
        }

        if (!InitAudioCapture()) {
            SHARING_LOGE("capture consumerId: %{public}u, init audio capture failed.", GetId());
            return false;
        }
    }

    SHARING_LOGI("capture consumerId: %{public}u, success.", GetId());
    isInit_ = true;
    return true;
}

bool ScreenCaptureConsumer::InitVideoCapture(uint64_t screenId)
{
    SHARING_LOGI("consumerId: %{public}u.", GetId());
    VideoSourceConfigure config;
    config.srcScreenId_ = screenId;
    config.videoHeight_ = videoTrack_.height;
    config.videoWidth_ = videoTrack_.width;
    config.screenHeight_ = videoTrack_.height;
    config.screenWidth_ = videoTrack_.width;
    config.frameRate_ = videoTrack_.frameRate;
    config.codecType_ = videoTrack_.codecId;
    config.dpi_ = dpi_;
    config.mode_ = mode_;
    config.isPc_ = isPc_;
    config.serialNumber_ = serialNumber_;
    if (config.frameRate_ == HIVISION_FRAME_RATE) {
        isHiVisionScreen_ = true;
        lowInterval_ = LOW_PTS_INTERVAL_HIVISON;
        highInterval_ = HIGH_PTS_INTERVAL_HIVISON;
    }

    SHARING_LOGI("frameRate_: %{public}f, videoHeight_:%{public}u.,videoWidth_:%{public}u.",
        config.frameRate_, config.videoHeight_, config.videoWidth_);

    videoSourceEncoder_ = std::make_shared<VideoSourceEncoder>(shared_from_this());
    if (!videoSourceEncoder_->InitEncoder(config)) {
        SHARING_LOGE("InitEncoder failed! %{public}u.", GetId());
        OnInitVideoCaptureError();
        return false;
    }
    videoSourceScreen_ = std::make_shared<VideoSourceScreen>(videoSourceEncoder_->GetEncoderSurface());

    int32_t ret = videoSourceScreen_->InitScreenSource(config);
    if (ret != ERR_OK) {
        SHARING_LOGE("InitScreenSource failed, ret: %{public}d.", ret);
        OnInitVideoCaptureError();
        return false;
    }
    return true;
}

bool ScreenCaptureConsumer::InitAudioCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);

    AudioStandard::AudioCapturerOptions options;
    options.streamInfo.samplingRate = AudioStandard::AudioSamplingRate::SAMPLE_RATE_48000;
    options.streamInfo.encoding = AudioStandard::AudioEncodingType::ENCODING_PCM;
    options.streamInfo.format = AudioStandard::AudioSampleFormat::SAMPLE_S16LE;
    options.streamInfo.channels = AudioStandard::AudioChannel::STEREO;
    options.capturerInfo.sourceType = AudioStandard::SourceType::SOURCE_TYPE_REMOTE_CAST;
    options.capturerInfo.capturerFlags = CAPTURER_FLAG;
    options.playbackCaptureConfig.silentCapture = true;
    options.playbackCaptureConfig.filterOptions.usages.push_back(AudioStandard::StreamUsage::STREAM_USAGE_MEDIA);
    options.playbackCaptureConfig.filterOptions.usages.push_back(AudioStandard::StreamUsage::STREAM_USAGE_MOVIE);
    options.playbackCaptureConfig.filterOptions.usages.push_back(AudioStandard::StreamUsage::STREAM_USAGE_GAME);
    options.playbackCaptureConfig.filterOptions.usages.push_back(AudioStandard::StreamUsage::STREAM_USAGE_AUDIOBOOK);
    AudioStandard::AppInfo appInfo;
    audioCapturer_ = AudioStandard::AudioCapturer::Create(options, "", appInfo);
    if (audioCapturer_ == nullptr) {
        SHARING_LOGE("create AudioCapturer failed! consumerId: %{public}u.", GetId());
        return false;
    }

    return true;
}

bool ScreenCaptureConsumer::InitAudioEncoder()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    audioEncoder_ = SourceCodecFactory::CreateAudioEncoder(audioTrack_.codecId);
    if (!audioEncoder_) {
        SHARING_LOGE("create audio encoder failed.");
        return false;
    }

    int32_t ret = audioEncoder_->Init(AUDIO_CHANNEL_STEREO, AUDIO_SAMPLE_BIT_S16LE, AUDIO_SAMPLE_RATE_48000);
    if (ret != 0) {
        SHARING_LOGE("audio encoder init failed, ret: %{public}d.", ret);
        return false;
    }
    audioEncoderReceiver_ = std::make_shared<AudioEncoderReceiver>(shared_from_this());
    audioEncoder_->AddAudioDestination(audioEncoderReceiver_);
    return true;
}

bool ScreenCaptureConsumer::StartCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRunning_ || !isInit_) {
        return false;
    }

    isRunning_ = true;
    if ((videoTrack_.codecId != CODEC_NONE) && !StartVideoCapture()) {
        SHARING_LOGE("consumerId: %{public}u, start video capture failed.", GetId());
        isRunning_ = false;
        return false;
    }

    if ((audioTrack_.codecId != CODEC_NONE) && !StartAudioCapture()) {
        SHARING_LOGE("consumerId: %{public}u, start audio capture failed, only viedo works!", GetId());
    }

    SHARING_LOGI("consumerId: %{public}u, success.", GetId());
    return true;
}

bool ScreenCaptureConsumer::StartAudioCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (audioCapturer_ == nullptr) {
        SHARING_LOGE("start capture fail - invalid capturer, consumerId: %{public}u.", GetId());
        return false;
    }

    if (!audioCapturer_->Start()) {
        SHARING_LOGE("audioCapturer Start() failed, consumerId: %{public}u.", GetId());
        return false;
    }

    if (audioCapturer_->GetBufferSize(audioBufferLen_) != 0) {
        audioBufferLen_ = 0;
        SHARING_LOGE("audioCapturer GetBufferSize failed, consumerId: %{public}u.", GetId());
        return false;
    }

    audioCaptureThread_ = std::make_unique<std::thread>([this]() {
        SetThreadPriority(gettid(), THREAD_PRIORITY);
        this->AudioCaptureThreadWorker();
    });
    std::string name = "audioCapture";
    pthread_setname_np(audioCaptureThread_->native_handle(), name.c_str());

    SHARING_LOGI("consumerId: %{public}u, audio capture start successful.", GetId());
    return true;
}

bool ScreenCaptureConsumer::StartVideoCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);

    if (videoSourceEncoder_ != nullptr) {
        if (!videoSourceEncoder_->StartEncoder()) {
            SHARING_LOGE("StartEncoder failed.");
            return false;
        }
    }
    if (videoSourceScreen_ != nullptr) {
        videoSourceScreen_->StartScreenSourceCapture();
    }

    return true;
}

bool ScreenCaptureConsumer::StopCapture()
{
    SHARING_LOGI("stop capture consumer, consumerId: %{public}u.", GetId());
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_ || !isInit_) {
        SHARING_LOGI("capture consumer already stoped, consumerId: %{public}u.", GetId());
        return true;
    }

    isRunning_ = false;
    StopAudioCapture();
    StopVideoCapture();

    return true;
}

bool ScreenCaptureConsumer::StopAudioCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (audioTrack_.codecId != CODEC_NONE) {
        if (audioCaptureThread_ != nullptr && audioCaptureThread_->joinable()) {
            audioCaptureThread_->join();
            audioCaptureThread_.reset();
            audioCaptureThread_ = nullptr;
        }

        if (audioCapturer_) {
            audioCapturer_->Flush();
            audioCapturer_->Stop();
            audioCapturer_->Release();
        }
    }
    return true;
}

bool ScreenCaptureConsumer::StopVideoCapture()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoTrack_.codecId != CODEC_NONE) {
        if (videoSourceEncoder_ != nullptr) {
            videoSourceEncoder_->StopEncoder();
        }
        if (videoSourceScreen_ != nullptr) {
            videoSourceScreen_->StopScreenSourceCapture();
        }
    }
    return true;
}

__attribute__((no_sanitize("cfi"))) void ScreenCaptureConsumer::AudioCaptureThreadWorker()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    int32_t bytesRead = 0;
    bool isBlockingRead = true;

    auto pcmFrame = FrameImpl::Create();
    pcmFrame->codecId_ = CODEC_PCM;

    uint8_t *frame = (uint8_t *)malloc(audioBufferLen_);
    SHARING_LOGI("audio capture buffer size: %{public}zu.", audioBufferLen_);
    if (!frame) {
        SHARING_LOGE("malloc buffer failed.");
        return;
    }

    while (isRunning_) {
        SHARING_LOGD("audio capture thread get pcm data, size: %{public}u.", bytesRead);
        bytesRead = audioCapturer_->Read(*frame, audioBufferLen_, isBlockingRead);
        if (bytesRead > 0 && audioEncoder_) {
            pcmFrame->Clear();
            pcmFrame->Assign((char *)frame, static_cast<uint32_t>(bytesRead));
            audioFrameCount_++;
            if (IsSilentFrame(frame, bytesRead)) {
                silentFrameCount_++;
            }
            audioEncoder_->OnFrame(pcmFrame);
        }
    }

    if (frame) {
        free(frame);
    }

    SHARING_LOGI("audio capture thread exit.");
}

void ScreenCaptureConsumer::OnInitVideoCaptureError()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    auto statusMsg = std::make_shared<ProsumerStatusMsg>();
    statusMsg->status = PROSUMER_NOTIFY_ERROR;
    statusMsg->errorCode = ERR_PROSUMER_VIDEO_CAPTURE;
    statusMsg->eventMsg = std::make_shared<EventMsg>();

    Notify(statusMsg);
}

bool ScreenCaptureConsumer::IsSilentFrame(const uint8_t *buffer, size_t bufLen)
{
    if (!buffer) {
        SHARING_LOGE(".buffer is null! ");
        return false;
    }

    for (size_t index = 0; index  < bufLen; ++index) {
        if (buffer[index] != 0 && buffer[index] != 0xFF) {
            return false;
        }
    }
    return true;
}

REGISTER_CLASS_REFLECTOR(ScreenCaptureConsumer);
} // namespace Sharing
} // namespace OHOS
