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

#ifndef CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_BLOCK_PAGE_H_
#define CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_BLOCK_PAGE_H_

#include <memory>

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#include "libcef/browser/ohos_safe_browsing/ohos_sb_controller_client.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_threat_type.h"

namespace ohos_safe_browsing {

using security_interstitials::MetricsHelper;
using security_interstitials::SecurityInterstitialControllerClient;
using security_interstitials::SecurityInterstitialPage;

class OhosSbBlockPage : public SecurityInterstitialPage {
 public:
  OhosSbBlockPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      int policy,
      OHSBThreatType block_type,
      std::unique_ptr<SecurityInterstitialControllerClient> controller_client);
  ~OhosSbBlockPage() override;

  // DISALLOW_COPY_AND_ASSIGN
  OhosSbBlockPage(const OhosSbBlockPage&) = delete;
  OhosSbBlockPage& operator=(const OhosSbBlockPage&) = delete;

  std::string GetHTMLContents() override;
  void OnInterstitialClosing() override;

  bool ShouldDisplayURL() const override;

  void CommandReceived(const std::string& command) override;

  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
 
 private:
  static std::unique_ptr<MetricsHelper> GetMetricsHelper(const GURL& url);

  void HandleCommand(security_interstitials::SecurityInterstitialCommand command);

  OHSBThreatType block_type_;
  int policy_;
};

}  // namespace ohos_safe_browsing

#endif  // CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_BLOCK_PAGE_H_