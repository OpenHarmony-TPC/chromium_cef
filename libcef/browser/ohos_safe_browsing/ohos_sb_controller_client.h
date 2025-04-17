/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_CONTROLLER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_CONTROLLER_CLIENT_H_

#include <memory>
#include <string>

#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"

#include "url/gurl.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
} // namespace content

namespace security_interstitials {
class MetricsHelper;
class SettingsPageHelper;
} // namespace security_interstitials


using security_interstitials::MetricsHelper;
using security_interstitials::SecurityInterstitialControllerClient;

class OhosSbControllerClient : public SecurityInterstitialControllerClient {
 public:
  OhosSbControllerClient(content::WebContents* web_contents,
                     PrefService* prefs,
                     const GURL& url,
                     const std::string& app_locale,
                     bool incognito_mode);

  ~OhosSbControllerClient() override;

  void Proceed() override;
  void Reload() override;
  void GoBack() override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() const override;

protected:
  const std::string GetExtendedReportingPrefName() const override;
  raw_ptr<content::WebContents> web_contents_;

private:
  static std::unique_ptr<MetricsHelper> GetMetricsHelper(const GURL& url);

  raw_ptr<PrefService> prefs_;
  const std::string app_locale_;
  GURL url_;
  bool incognito_mode_{false};
};

#endif  // CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_CONTROLLER_CLIENT_H_
