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

#ifndef ARKWEB_GLOBAL_LIST_CONFIG_H
#define ARKWEB_GLOBAL_LIST_CONFIG_H

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace fallback_proxy {

class ArkwebGlobalListConfig {
 public:
  // Interface to implement for observers that wish to be informed of changes
  // to the context. All methods will be called on the UI thread.
  class GlobalListConfigObserver {
   public:
    // Called before the context is destroyed.
    virtual void OnInterestedConfigChanged() = 0;

    virtual ~GlobalListConfigObserver() {}
  };

  static ArkwebGlobalListConfig* GetInstance();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Init(PrefService* prefs);

  void SetGlobalListConfigPath(const base::FilePath& path,
                               std::string& version);

  std::vector<std::string> GetFallbackProxyBlockList();

  std::vector<std::string> GetFraudWebWhiteList();

  // Monitor the path change from prefs and notify the observers to handle
  void AddConfigChangeObserver(const std::string& path,
                               GlobalListConfigObserver* observer);
  void RemoveConfigChangeObservers(const std::string& path);

  ~ArkwebGlobalListConfig();

 private:
  ArkwebGlobalListConfig();
  friend struct base::LazyInstanceTraitsBase<ArkwebGlobalListConfig>;
  std::vector<std::string> GetListByKey(const std::string& key);

  // Parse the global config file, should only be called on the IO thread.
  std::string ParsingConfigFile(const base::FilePath& config_path);

  void OnConfigFileReadDone(const std::string& json_data);
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::raw_ptr<PrefService> pref_service_;
  base::FilePath config_path_;
  std::string version_;
  bool need_set_config_path_ = false;
};

}  // namespace fallback_proxy
#endif  // ARKWEB_BROWSER_BROWSER_HOST_EXT_H_