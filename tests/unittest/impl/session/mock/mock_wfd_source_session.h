/*
 * Copyright (c) 2024 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#ifndef OHOS_SHARING_MOCK_WFD_SOURCE_SESSION_H
#define OHOS_SHARING_MOCK_WFD_SOURCE_SESSION_H

#include <gmock/gmock.h>
#define private public
#define protected public
#define final
#include "wfd_source_session.h"
#undef final
#undef private
#undef protected

namespace OHOS {
namespace Sharing {

class MockWfdSourceSession : public WfdSourceSession {
public:
    void InitSession();

    void SetInterruptState(bool state);
    void SetWfdState(WfdSessionState state);
    void SetServer(std::shared_ptr<IServer> server);
    void SetRunningState(SessionRunningStatus state);
};

} // namespace Sharing
} // namespace OHOS
#endif
