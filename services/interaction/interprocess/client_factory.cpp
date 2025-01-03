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

#include "client_factory.h"
#include "common/sharing_log.h"

namespace OHOS {
namespace Sharing {
    
ClientFactory::ClientFactory()
{
    SHARING_LOGI("ClientFactory.");
    interIpcClient_ = std::make_shared<InterIpcClient>();
    interIpcClient_->Initialize(interIpcClient_->GetSharingProxy());
}

ClientFactory::~ClientFactory()
{
    SHARING_LOGI("~ClientFactory.");
    if (!interIpcClient_) {
        SHARING_LOGE("client is null.");
        return;
    }
    interIpcClient_.reset();
}

std::shared_ptr<InterIpcClient> ClientFactory::CreateClient(std::string key,
    std::string clientClassName, std::string serverClassName)
{
    SHARING_LOGD("trace.");
    if (!interIpcClient_) {
        SHARING_LOGE("create client service null.");
        return nullptr;
    }

    return interIpcClient_->CreateSubService(key, clientClassName, serverClassName);
}

} // namespace Sharing
} // namespace OHOS