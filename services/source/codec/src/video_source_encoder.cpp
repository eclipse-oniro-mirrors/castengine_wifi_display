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

#include "video_source_encoder.h"
#include "avcodec_errors.h"
#include "buffer/avsharedmemory.h"
#include "common/common_macro.h"
#include "protocol/frame/h264_frame.h"
#include "sharing_log.h"
#include "const_def.h"

#include "avcodec_codec_name.h"
#include "avcodec_info.h"
#include "avcodec_list.h"

using namespace OHOS::Media;

namespace OHOS {
namespace Sharing {

void VideoSourceEncoder::VideoEncodeCallback::OnError(MediaAVCodec::AVCodecErrorType errorType, int32_t errorCode)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (auto parent = parent_.lock()) {
        parent->OnError(errorType, errorCode);
    }
}

void VideoSourceEncoder::VideoEncodeCallback::OnOutputFormatChanged(const MediaAVCodec::Format &format)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (auto parent = parent_.lock()) {
        parent->OnOutputFormatChanged(format);
    }
}

void VideoSourceEncoder::VideoEncodeCallback::OnInputBufferAvailable(
    uint32_t index, std::shared_ptr<MediaAVCodec::AVSharedMemory> buffer)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (auto parent = parent_.lock()) {
        parent->OnInputBufferAvailable(index, buffer);
    }
}

void VideoSourceEncoder::VideoEncodeCallback::OnOutputBufferAvailable(
    uint32_t index, MediaAVCodec::AVCodecBufferInfo info, MediaAVCodec::AVCodecBufferFlag flag,
    std::shared_ptr<MediaAVCodec::AVSharedMemory> buffer)
{
    if (auto parent = parent_.lock()) {
        parent->OnOutputBufferAvailable(index, info, flag, buffer);
    }
}

void VideoSourceEncoder::VideoEncodeParameterCallback::OnInputParameterWithAttrAvailable(
    uint32_t index, std::shared_ptr<Format> attribute, std::shared_ptr<Format> parameter)
{
    if (auto parent = parent_.lock()) {
        if (parent->IsNeedDropFrame()) {
            parameter->PutIntValue(Media::Tag::VIDEO_ENCODER_PER_FRAME_DISCARD, 1);
        }
        parent->videoEncoder_->QueueInputParameter(index);
    }
}

VideoSourceEncoder::~VideoSourceEncoder()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    StopEncoder();
    ReleaseEncoder();
    encoderCb_ = nullptr;
}

bool VideoSourceEncoder::InitEncoder(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    codecType_ = configure.codecType_;
    if (!CreateEncoder(configure)) {
        SHARING_LOGE("Create encoder failed!");
        return false;
    }

    if (!ConfigEncoder(configure)) {
        SHARING_LOGE("Config encoder failed!");
        ReleaseEncoder();
        return false;
    }

    videoEncoderSurface_ = videoEncoder_->CreateInputSurface();
    if (videoEncoderSurface_ == nullptr) {
        SHARING_LOGE("Create encoder surface failed!");
        ReleaseEncoder();
        return false;
    }

    return true;
}

bool VideoSourceEncoder::CreateEncoder(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoEncoder_ != nullptr) {
        ReleaseEncoder();
    }
    switch (configure.codecType_) {
        case CodecId::CODEC_H264:
            videoEncoder_ = OHOS::MediaAVCodec::VideoEncoderFactory::CreateByMime("video/avc");
            break;
        case CodecId::CODEC_H265:
            videoEncoder_ = OHOS::MediaAVCodec::VideoEncoderFactory::CreateByMime("video/hevc");
            break;
        default:
            SHARING_LOGE("Encoder codecType is invalid!");
            videoEncoder_ = nullptr;
    }
    if (videoEncoder_ == nullptr) {
        SHARING_LOGE("Create Video Source Encoder failed!");
        return false;
    }
    encoderCb_ = std::make_shared<VideoEncodeCallback>(shared_from_this());
    int32_t ret = videoEncoder_->SetCallback(encoderCb_);
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Set encoder callback failed!");
        ReleaseEncoder();
        return false;
    }
    parameterCb_ = std::make_shared<VideoEncodeParameterCallback>(shared_from_this());
    ret = videoEncoder_->SetCallback(parameterCb_);
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Set parameter callback failed!");
        ReleaseEncoder();
        return false;
    }

    return true;
}

bool VideoSourceEncoder::ConfigEncoder(const VideoSourceConfigure &configure)
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoEncoder_ == nullptr) {
        SHARING_LOGE("Encoder is null!");
        return false;
    }
    MediaAVCodec::Format videoFormat;
    switch (configure.codecType_) {
        case CodecId::CODEC_H264:
            videoFormat.PutStringValue("codec_mime", "video/avc");
            break;
        case CodecId::CODEC_H265:
            videoFormat.PutStringValue("codec_mime", "video/hevc");
            break;
        default:
            SHARING_LOGE("Encoder codecType is invalid!");
            return false;
    }
    SHARING_LOGI("codecType_:%{public}d, frameRate_:%{public}f, videoHeight_:%{public}u, videoWidth_:%{public}u",
                 configure.codecType_, configure.frameRate_, configure.videoHeight_, configure.videoWidth_);
    videoFormat.PutIntValue("pixel_format", configure.pixleFormat_);
    videoFormat.PutIntValue("max_input_size", MAX_YUV420_BUFFER_SIZE);
    videoFormat.PutIntValue("width", configure.videoWidth_);
    videoFormat.PutIntValue("height", configure.videoHeight_);
    videoFormat.PutDoubleValue("frame_rate", configure.frameRate_);
    if (configure.frameRate_ == HIVISION_FRAME_RATE) {
        SHARING_LOGI("%{public}s. frameRate_ : 60", __FUNCTION__);
        isHiVisionScreen_ = true;
        videoFormat.PutLongValue("bitrate", HIVISION_ENCODE_BITRATE);
        videoFormat.PutIntValue("video_encoder_repeat_previous_frame_after", REPEAT_DIFF_HIVISION);
    } else {
        videoFormat.PutLongValue("bitrate", SCREEN_CAPTURE_ENCODE_BITRATE);
        videoFormat.PutIntValue("video_encoder_repeat_previous_frame_after", REPEAT_DIFF);
    }
    videoFormat.PutIntValue("i_frame_interval", MILLISECOND + MILLISECOND);
    videoFormat.PutIntValue("video_encoder_qp_max", INITIAL_MAX_QP_HISI);
    videoFormat.PutIntValue("video_encoder_qp_min", INITIAL_MIN_QP_HISI);
    videoFormat.PutIntValue("video_encode_bitrate_mode", BITRATE_MODE_VBR);
    videoFormat.PutIntValue("video_encoder_repeat_previous_max_count", MILLISECOND * MILLISECOND);
    // encoder match GPU output format <GRAPHIC_CM_DISPLAY_SRGB>
    videoFormat.PutIntValue(Tag::VIDEO_COLOR_PRIMARIES, static_cast<int32_t>(Plugins::ColorPrimary::BT709));
    videoFormat.PutIntValue(Tag::VIDEO_COLOR_TRC, static_cast<int32_t>(Plugins::TransferCharacteristic::IEC_61966_2_1));
    videoFormat.PutIntValue(Tag::VIDEO_COLOR_MATRIX_COEFF, static_cast<int32_t>(Plugins::MatrixCoefficient::BT709));
    videoFormat.PutIntValue(Tag::VIDEO_COLOR_RANGE, 0);

    int32_t ret = videoEncoder_->Configure(videoFormat);
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Configure encoder failed!");
        return false;
    }

    return true;
}

sptr<Surface> &VideoSourceEncoder::GetEncoderSurface()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    return videoEncoderSurface_;
}

bool VideoSourceEncoder::StartEncoder()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoEncoder_ == nullptr) {
        SHARING_LOGE("Encoder is null!");
        return false;
    }

    int32_t ret = videoEncoder_->Prepare();
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Prepare encoder failed!");
        return false;
    }
    isMediaDumpEnable_ = OHOS::system::GetBoolParameter(PARAM_MEDIA_DUMP, false);
    if (isMediaDumpEnable_) {
        if (dumpFp_ == nullptr) {
            dumpFp_ = fopen("/data/service/el1/public/sharing_service/sharing_media_video.es", "wb");
        }
        if (dumpFp_ == nullptr) {
            SHARING_LOGE("Open cast_media_video.es failed.");
        }
    }

    ret = videoEncoder_->Start();
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Start encoder failed!");
        return false;
    }
    return true;
}

bool VideoSourceEncoder::StopEncoder()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoEncoder_ == nullptr) {
        SHARING_LOGE("Encoder is null!");
        return false;
    }

    int32_t ret = videoEncoder_->Flush();
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Flush encoder failed!");
    }

    ret = videoEncoder_->Stop();
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Stop encoder failed!");
        return false;
    }

    return true;
}

bool VideoSourceEncoder::ReleaseEncoder()
{
    SHARING_LOGI("%{public}s.", __FUNCTION__);
    if (videoEncoder_ == nullptr) {
        SHARING_LOGE("encoder is null!");
        return false;
    }
    int32_t ret = videoEncoder_->Release();
    if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
        SHARING_LOGE("Release encoder failed.");
        return false;
    }

    if (dumpFp_ != nullptr) {
        fclose(dumpFp_);
        dumpFp_ = nullptr;
    }

    videoEncoder_ = nullptr;

    return true;
}

void VideoSourceEncoder::OnOutputBufferAvailable(uint32_t index, MediaAVCodec::AVCodecBufferInfo info,
                                                 MediaAVCodec::AVCodecBufferFlag flag,
                                                 std::shared_ptr<MediaAVCodec::AVSharedMemory> buffer)
{
    SHARING_LOGD("index: %{public}u, size:%{public}u, pts: %{public}" PRIi64 ".", index, info.size,
                 info.presentationTimeUs);
    if (!videoEncoder_) {
        SHARING_LOGE("encoder is null!");
        return;
    }

    if (buffer == nullptr || buffer->GetBase() == nullptr) {
        SHARING_LOGE("Get output buffer null!");
        return;
    }
    size_t dataSize = static_cast<size_t>(info.size);
    if (dataSize == 0) {
        SHARING_LOGE("params invalid, size: %{public}zu.", dataSize);
        return;
    }
    if (flag == MediaAVCodec::AVCodecBufferFlag::AVCODEC_BUFFER_FLAG_CODEC_DATA) {
        SHARING_LOGE("Save first sps pps");
        spsBufferSize_ = static_cast<uint32_t>(info.size);
        spsBuffer_ = std::make_unique<uint8_t[]>(info.size);
        if (memcpy_s(spsBuffer_.get(), info.size, buffer->GetBase(), info.size) != EOK) {
            SHARING_LOGE("Copy sps pps data failed.");
            spsBuffer_.reset();
            spsBufferSize_ = 0;
            return;
        }
        int32_t ret = videoEncoder_->ReleaseOutputBuffer(index);
        if (ret != MediaAVCodec::AVCS_ERR_OK) {
            SHARING_LOGE("Release output buffer failed, ret: %{public}d.", ret);
        }
        return;
    }
    const char *data = reinterpret_cast<const char *>(buffer->GetBase());
    RETURN_IF_NULL(data);

    if (auto listener = listener_.lock()) {
        bool keyFrame = flag == MediaAVCodec::AVCodecBufferFlag::AVCODEC_BUFFER_FLAG_SYNC_FRAME;
        auto videoFrameImpl = FrameImpl::Create();
        RETURN_IF_NULL(videoFrameImpl);
        Frame::Ptr videoFrame = videoFrameImpl;
        videoFrameImpl->codecId_ = static_cast<CodecId>(codecType_);

        if (keyFrame) {
            std::unique_ptr<uint8_t[]> idrFrame = std::make_unique<uint8_t[]>(spsBufferSize_ + info.size);
            if (idrFrame == nullptr || idrFrame.get() == nullptr) {
                SHARING_LOGE("frameBuffer is nullptr.");
                return;
            }
            if (memcpy_s(idrFrame.get(), spsBufferSize_, spsBuffer_.get(), spsBufferSize_) != EOK) {
                SHARING_LOGE("Copy data failed.");
                return;
            }
            if (memcpy_s(idrFrame.get() + spsBufferSize_, info.size, buffer->GetBase(), info.size) != EOK) {
                SHARING_LOGE("Copy data failed.");
                return;
            }
            videoFrame->Assign(reinterpret_cast<const char*>(idrFrame.get()), spsBufferSize_ + info.size);
            if (dumpFp_ != nullptr) {
                fwrite(idrFrame.get(), spsBufferSize_ + info.size, 1, dumpFp_);
            }
        } else {
            videoFrame->Assign(data, info.size);
            if (dumpFp_ != nullptr) {
                fwrite(data, dataSize, 1, dumpFp_);
            }
        }

        listener->OnFrame(videoFrame, IDR_FRAME, keyFrame);

        videoFrame = nullptr;
    } else {
        SHARING_LOGE("listener_ is null, call OnFrame failed!");
    }

    if (videoEncoder_->ReleaseOutputBuffer(index) != MediaAVCodec::AVCS_ERR_OK) {
        SHARING_LOGW("release output buffer failed!");
    }
}

void VideoSourceEncoder::OnInputBufferAvailable(uint32_t index, std::shared_ptr<MediaAVCodec::AVSharedMemory> buffer)
{
    SHARING_LOGI("index:%{public}u.", index);
    if (auto listener = listener_.lock()) {
        listener->OnFrameBufferUsed();
    }
}

void VideoSourceEncoder::OnOutputFormatChanged(const MediaAVCodec::Format &format)
{
    SHARING_LOGE("%{public}s.", __FUNCTION__);
    (void)format;
}

void VideoSourceEncoder::OnError(MediaAVCodec::AVCodecErrorType errorType, int32_t errorCode)
{
    SHARING_LOGE("Encoder error, errorType(%{public}d), errorCode(%{public}d)!", errorType, errorCode);
}

void VideoSourceEncoder::UpdateBitrate(int64_t bitrate)
{
    SHARING_LOGI("Update bitrate, nowBitrate = %{public}ld.", bitrate);
    if (videoEncoder_) {
        Format format{};
        format.PutLongValue("bitrate", bitrate);
        int32_t ret = videoEncoder_->SetParameter(format);
        if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
            SHARING_LOGE("updateBitrate failed, ret: %{public}d.", ret);
        }
    }
}

void VideoSourceEncoder::RequestKeyFrame()
{
    SHARING_LOGI("RequestKeyFrame");
    if (videoEncoder_) {
        Format format{};
        format.PutIntValue("req_i_frame", 1);
        int32_t ret = videoEncoder_->SetParameter(format);
        if (ret != MediaAVCodec::AVCodecServiceErrCode::AVCS_ERR_OK) {
            SHARING_LOGE("RequestKeyFrame failed, ret: %{public}d.", ret);
        }
    }
}

bool VideoSourceEncoder::IsNeedDropFrame()
{
    if (firstFrame_) {
        start_ = std::chrono::steady_clock::now();
        firstFrame_ = false;
        return false;
    }
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    uint64_t pts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count());
    callbackCount_++;
    uint64_t expectIndex;
    uint64_t frameRate;
    if (isHiVisionScreen_) {
        expectIndex = pts / 50 * 3 + pts % 50 / 17; // 3: frames, 50: three intervals, 17: interval
        frameRate = FRAME_RATE_60;
    } else {
        expectIndex = pts / 100 * 3 + pts % 100 / 33; // 3: frames, 100: three intervals, 33: interval
        frameRate = FRAME_RATE_30;
    }
    if (currentIndex_ != expectIndex) {
        lastPts_ = pts;
        currentIndex_ = expectIndex;
        uint64_t currentSecond = pts / (uint64_t)MILLISECOND;
        if (currentSecond_ != currentSecond) {
            if (frameCount_ < frameRate - 1) { // 1 fixed size
                SHARING_LOGE("input buffer fps: %{public}" PRIu64 ", Parameter callback fps: %{public}" PRIu64 ".",
                             frameCount_, callbackCount_);
            }
            callbackCount_ = 0;
            frameCount_ = 0;
            currentSecond_ = currentSecond;
        }
        frameCount_++;
        return false;
    }
    return true;
}
} // namespace Sharing
} // namespace OHOS
