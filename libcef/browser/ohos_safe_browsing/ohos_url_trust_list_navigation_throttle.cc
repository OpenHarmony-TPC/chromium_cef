/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "libcef/browser/ohos_safe_browsing/ohos_url_trust_list_navigation_throttle.h"

#include "base/i18n/rtl.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "libcef/browser/ohos_safe_browsing/ohos_url_trust_list_interface.h"
#include "libcef/browser/ohos_safe_browsing/ohos_safe_browsing_response.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_block_page.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_threat_type.h"

namespace ohos_safe_browsing {
std::unique_ptr<content::NavigationThrottle> OhosUrlTrustListNavigationThrottle::Create(
  content::NavigationHandle* handle) {
  return base::WrapUnique(new OhosUrlTrustListNavigationThrottle(handle));
}

OhosUrlTrustListNavigationThrottle::OhosUrlTrustListNavigationThrottle(
  content::NavigationHandle* navigation_handle)
  : content::NavigationThrottle(navigation_handle) {}

OhosUrlTrustListNavigationThrottle::~OhosUrlTrustListNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
OhosUrlTrustListNavigationThrottle::WillStartRequest() {
  content::NavigationHandle *handle = navigation_handle();
  if (!handle || !handle->IsInMainFrame() ||
    handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    return PROCEED;
  }
  content::WebContents* webContents = handle->GetWebContents();
  if (!webContents) {
    return PROCEED;
  }
  auto manager =
    reinterpret_cast<OhosUrlTrustListInterface *>(
      webContents->GetUserData(&OhosUrlTrustListInterface::interfaceKey));
  if (!manager) {
    return PROCEED;
  }
  if (manager->CheckUrlTrustList(handle->GetURL()) !=
    UrlTrustCheckResult::RESULT_ALLOW) {
    auto gurl = handle->GetURL();
    auto locale = base::i18n::GetConfiguredLocale();
    auto controller = std::make_unique<OhosSbControllerClient>(
      webContents, nullptr, gurl, locale, false);
    std::unique_ptr<OhosSbBlockPage> blocking_page =
      std::make_unique<OhosSbBlockPage>(
      webContents, gurl, OHSBPolicyType::POLICY_FORBIDDEN_PROHIBIT_ACCESS,
      OHSBThreatType::THREAT_URL_TRUST_LIST, std::move(controller));
    std::string html = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
      AssociateBlockingPage(handle, std::move(blocking_page));
    return { BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT, html };
  }
  return PROCEED;
}

const char* OhosUrlTrustListNavigationThrottle::GetNameForLogging() {
  return "OhosUrlTrustListNavigationThrottle";
}
} // namespace ohos_safe_browsing
