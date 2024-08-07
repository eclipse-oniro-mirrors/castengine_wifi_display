/*
 * Copyright (c) 2023-2024 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#ifndef OHOS_SHARING_AUDIO_ENCODER_H
#define OHOS_SHARING_AUDIO_ENCODER_H

#include "media_frame_pipeline.h"

namespace OHOS {
namespace Sharing {
class AudioEncoder : public FrameSource,
                     public FrameDestination {
public:
    AudioEncoder() = default;
    virtual ~AudioEncoder() = default;

    virtual int Init(uint32_t channels = 2, uint32_t sampleBit = 16, uint32_t sampleRate = 44100) = 0;

public:
    bool inited_ = false;
};
} // namespace Sharing
} // namespace OHOS
#endif