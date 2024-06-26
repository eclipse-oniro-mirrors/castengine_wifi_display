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

#ifndef OHOS_SHARING_INTER_IPC_INTERFACE_SERVICE_STUB_H
#define OHOS_SHARING_INTER_IPC_INTERFACE_SERVICE_STUB_H
#include "inter_ipc_stub.h"

namespace OHOS {
namespace Sharing {

class InterIpcServiceStub : public InterIpcStub {
public:
    using Ptr = std::shared_ptr<InterIpcServiceStub>;
    InterIpcServiceStub();
    ~InterIpcServiceStub() override;

    void DelPeerProxy(std::string key) override;
    void CreateDeathListener(std::string key) override;

    int32_t SetListenerObject(std::string key, const sptr<IRemoteObject> &object) override;
    int32_t DoIpcCommand(std::shared_ptr<BaseMsg> msg, std::shared_ptr<BaseMsg> &replyMsg) override;
    sptr<IRemoteObject> GetSubSystemAbility(std::string key, std::string className) override;
};

}
}
#endif
