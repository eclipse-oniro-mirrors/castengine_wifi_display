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

#ifndef OHOS_SHARING_COMMON_H
#define OHOS_SHARING_COMMON_H

#include "const_def.h"
#include "event_comm.h"

namespace OHOS {
namespace Sharing {
class Common {
public:
    static void SetVideoTrack(VideoTrack &videoTrack, VideoFormat videoFormat);
    static void SetAudioTrack(AudioTrack &audioTrack, AudioFormat audioFormat);
    static void SetAudioTrack(AudioTrack &audioTrack, CodecId codecId, AudioFormat audioFormat);
};

} // namespace Sharing
} // namespace OHOS
#endif