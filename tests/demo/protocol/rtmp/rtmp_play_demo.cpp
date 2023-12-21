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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unistd.h>
#include "common/sharing_log.h"
#include "rtmp_client.h"

using namespace OHOS::Sharing;

int main()
{
    auto rtmpClient = std::make_shared<RtmpClient>("rtmp://192.168.62.38:1935/live/0");
    rtmpClient->OpenUrl();
    while (true) {
        /* code */
        sleep(1);
    }

    return 0;
}