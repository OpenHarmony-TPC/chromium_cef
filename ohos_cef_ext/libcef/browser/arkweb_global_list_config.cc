/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "arkweb_global_list_config.h"

#include <cstdio>

#include <fstream>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace fallback_proxy {

namespace prefs {
const char kGlobalListConfigVersion[] = "hw_global_list_config.version";
const char kFallbackProxyBlockList[] = "fallbackProxyBlockList";
const char kFraudWebAllowList[] = "fraudWebAllowList";
}  // namespace prefs

const std::string supported_list_keys[] = {prefs::kFallbackProxyBlockList,
                                           prefs::kFraudWebAllowList};

base::LazyInstance<ArkwebGlobalListConfig>::Leaky g_lazy_instance;
ArkwebGlobalListConfig* ArkwebGlobalListConfig::GetInstance() {
  return g_lazy_instance.Pointer();
}

void ArkwebGlobalListConfig::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGlobalListConfigVersion, std::string());
  registry->RegisterListPref(prefs::kFallbackProxyBlockList);
  registry->RegisterListPref(prefs::kFraudWebAllowList);
}

void ArkwebGlobalListConfig::Init(PrefService* pref_service) {
  LOG(DEBUG) << "Fallback ArkwebGlobalListConfig::Init";
  if (pref_service) {
    pref_service_ = pref_service;
    pref_service_->SetString(prefs::kGlobalListConfigVersion, std::string());
    pref_service_->SetList(prefs::kFallbackProxyBlockList, base::Value::List());
    pref_service_->SetList(prefs::kFraudWebAllowList, base::Value::List());
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service_);
    if (need_set_config_path_) {
      SetGlobalListConfigPath(config_path_, version_);
      need_set_config_path_ = false;
    }
  } else {
    LOG(ERROR) << __func__ << "input pref_service null";
  }
}

ArkwebGlobalListConfig::~ArkwebGlobalListConfig() {
  if (pref_change_registrar_ && !pref_change_registrar_->IsEmpty()) {
    pref_change_registrar_->RemoveAll();
  }
}

void ArkwebGlobalListConfig::SetGlobalListConfigPath(
    const base::FilePath& config_path,
    std::string& version) {
  LOG(DEBUG) << "ArkwebGlobalListConfig::SetGlobalListConfigPath version:"
             << version;
  if (!pref_service_) {
    LOG(DEBUG)
        << "ArkwebGlobalListConfig::SetGlobalListConfigPath pref_service_ is null.";
    config_path_ = config_path;
    version_ = version;
    need_set_config_path_ = true;
    return;
  }
  if (pref_service_->HasPrefPath(prefs::kGlobalListConfigVersion) &&
      version == pref_change_registrar_->prefs()->GetString(
                     prefs::kGlobalListConfigVersion)) {
    LOG(INFO) << "Same version: " << version << " will not handle";
    return;
  }
  pref_service_->SetString(prefs::kGlobalListConfigVersion, version);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ArkwebGlobalListConfig::ParsingConfigFile,
                     base::Unretained(this), config_path),
      base::BindOnce(&ArkwebGlobalListConfig::OnConfigFileReadDone,
                     base::Unretained(this)));
}

void ArkwebGlobalListConfig::OnConfigFileReadDone(
    const std::string& json_data) {
  if (json_data.empty()) {
    LOG(WARNING) << __func__ << " got empty config";
    return;
  }

  std::optional<base::Value> root = base::JSONReader::Read(json_data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!root || !root->is_dict()) {
    LOG(WARNING) << __func__ << " data format is right";
    return;
  }

  for (std::string item : supported_list_keys) {
    base::Value* block_list = root->FindPath(item);
    if (block_list && block_list->is_list()) {
      const base::Value::List& list = block_list->GetList();
      base::Value::List save_list = list.Clone();
      LOG(DEBUG) << "parsing json data done with list key: " << item
                 << ", size " << save_list.size();
      pref_service_->SetList(item, std::move(save_list));
    } else {
      LOG(WARNING) << __func__ << " key " << item
                   << " doesn't exist in json_data";
      pref_service_->SetList(item, base::Value::List());
    }
  }
}

ArkwebGlobalListConfig::ArkwebGlobalListConfig() {}

void ArkwebGlobalListConfig::AddConfigChangeObserver(
    const std::string& path,
    GlobalListConfigObserver* observer) {
  if (!pref_service_) {
    LOG(ERROR) << "ArkwebGlobalListConfig AddConfigChangeObserver failed, for "
                  "init error";
    return;
  }
  pref_change_registrar_->Add(
      path,
      base::BindRepeating(&GlobalListConfigObserver::OnInterestedConfigChanged,
                          base::Unretained(observer)));
}

void ArkwebGlobalListConfig::RemoveConfigChangeObservers(
    const std::string& path) {
  if (!pref_service_) {
    LOG(ERROR) << "ArkwebGlobalListConfig init error";
    return;
  }
  if (pref_change_registrar_->IsObserved(path)) {
    pref_change_registrar_->Remove(path);
  } else {
    LOG(WARNING) << "remove path not exist";
  }
}

std::string ArkwebGlobalListConfig::ParsingConfigFile(
    const base::FilePath& config_path) {
  LOG(DEBUG) << "LoadCloudConfigFromFile, file path:" << config_path.value();
  std::string json_data;
  if (!base::PathExists(config_path) ||
      !base::ReadFileToString(config_path, &json_data)) {
    LOG(WARNING) << "Fail to open cloud control config file.";
    return std::string();
  }

  return json_data;
}

std::vector<std::string> ArkwebGlobalListConfig::GetFallbackProxyBlockList() {
  return GetListByKey(prefs::kFallbackProxyBlockList);
}

std::vector<std::string> ArkwebGlobalListConfig::GetFraudWebWhiteList() {
  return GetListByKey(prefs::kFraudWebAllowList);
}

std::vector<std::string> ArkwebGlobalListConfig::GetListByKey(
    const std::string& key) {
  std::vector<std::string> val;
  if (!pref_service_) {
    LOG(ERROR) << "ArkwebGlobalListConfig init error";
    return val;
  }
  const base::Value::List& list = pref_service_->GetList(key);
  for (const auto& entry : list) {
    val.push_back(entry.GetString());
  }

  return val;
}

}  // namespace fallback_proxy
