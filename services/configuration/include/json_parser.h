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

#ifndef OHOS_SHARING_JSON_PARSER_H
#define OHOS_SHARING_JSON_PARSER_H

#include <inttypes.h>
#include <string>
#include "json/json.h"
#include "sharing_data.h"

namespace OHOS {
namespace Sharing {

class JsonParser {
public:
    JsonParser() = default;
    ~JsonParser() = default;

    int32_t GetConfig(SharingData::Ptr &value);
    int32_t SaveConfig(SharingData::Ptr &value);
    int32_t ReadModuleConfig(Json::Value &module, SharingData::Ptr &value);

private:
    int32_t SaveModuleConfig(Json::Value &module, const SharingDataGroupByModule::Ptr &value);
};

} // namespace Sharing
} // namespace OHOS
#endif