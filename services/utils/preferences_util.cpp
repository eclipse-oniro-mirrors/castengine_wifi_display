/*
 * Copyright (c) 2025 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#include "preferences_util.h"
#include "common/sharing_log.h"

namespace OHOS {
namespace Sharing {
const std::string SPLIT = ",";

PreferencesUtil::PreferencesUtil(const std::string &path)
{
    path_ = path;
}

std::shared_ptr<NativePreferences::Preferences> PreferencesUtil::GetProfiles(const std::string &path, int &errCode)
{
    return NativePreferences::PreferencesHelper::GetPreferences(path_, errCode);
}

int PreferencesUtil::PutString(const std::string &key, const std::string &value)
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return NativePreferences::E_ERROR;
    }
    preferencesMutex_.lock();
    int result = ptr->PutString(key, value);
    ptr->FlushSync();
    preferencesMutex_.unlock();
    return result;
}

std::string PreferencesUtil::GetString(const std::string &key)
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return "";
    }
    std::string defaultValue = "";
    preferencesMutex_.lock();
    std::string result = ptr->GetString(key, defaultValue);
    preferencesMutex_.unlock();
    return result;
}

bool PreferencesUtil::HasKey(const std::string &key)
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return NativePreferences::E_ERROR;
    }
    preferencesMutex_.lock();
    int result = ptr->HasKey(key);
    preferencesMutex_.unlock();
    return result;
}

int PreferencesUtil::DeleteKey(const std::string &key)
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return NativePreferences::E_ERROR;
    }
    preferencesMutex_.lock();
    int result = ptr->Delete(key);
    ptr->FlushSync();
    preferencesMutex_.unlock();
    return result;
}

int PreferencesUtil::Clear()
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return NativePreferences::E_ERROR;
    }
    preferencesMutex_.lock();
    int result = ptr->Clear();
    preferencesMutex_.unlock();
    return result;
}

void PreferencesUtil::Flush()
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr != nullptr) {
        ptr->Flush();
    }
}

int PreferencesUtil::FlushSync()
{
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return NativePreferences::E_ERROR;
    }
    return ptr->FlushSync();
}

std::map<std::string, std::string> PreferencesUtil::GetAll()
{
    std::map<std::string, std::string> allDevices;
    std::shared_ptr<NativePreferences::Preferences> ptr = GetProfiles(path_, errCode_);
    if (ptr == nullptr) {
        SHARING_LOGE("get profiles failed. errcode_ %{public}d.", errCode_);
        return allDevices;
    }
    std::map<std::string, NativePreferences::PreferencesValue> datas = ptr->GetAll();

    for (auto &[key, value] : datas) {
        allDevices[key] = std::string(value);
    }
    return allDevices;
}
} // namespace Sharing
} // namespace OHOS