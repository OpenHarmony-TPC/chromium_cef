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
#include "libcef/browser/ohos_safe_browsing/ohos_sb_client.h"

#include "base/containers/lru_cache.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_controller_client.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_prefs.h"

namespace {
constexpr int kMaxCachedProfiles = 100;
base::LRUCache<GURL, ohos_safe_browsing::OhosSbClient::SbBlockingPageInfo>
    blocking_page_info_cache(kMaxCachedProfiles);
}  // namespace

namespace ohos_safe_browsing {

OhosSbClient::OhosSbClient(content::WebContents* web_contents, 
                   PrefService* prefs,
                   bool incognito_mode)
    : content::WebContentsObserver(web_contents), 
      prefs_(prefs),
      incognito_mode_(incognito_mode) {}

OhosSbClient::~OhosSbClient() = default;

// static
bool OhosSbClient::InMaliciousAllowlist(const PrefService* prefs,
                                        const std::string& url,
                                        bool incognito_mode) {
  if (!prefs) {
    return false;
  }

  const base::Value::List& list = prefs->GetList(
      incognito_mode ? kIncognitoMaliciousAllowList : kMaliciousAllowList);

  for (const base::Value& value : list) {
    if (value.GetString() == url) {
      return true;
    }
  }
  return false;
}

void OhosSbClient::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!web_contents() || web_contents()->IsBeingDestroyed()) {
    return;
  }

  if (!load_details.is_main_frame) {
    return;
  }

  if (load_details.is_same_document) {
    return;
  }

  BlockingPageInfo* blocking_page_info =
      BlockingPageInfo::FromWebContents(web_contents());

  if (!blocking_page_info) {
    return;
  }
  ShowBlockingPage();
}

void OhosSbClient::ShowBlockingPage() {
  if (!web_contents() || web_contents()->IsBeingDestroyed() ||
      !web_contents()->IsSafeBrowsingDetectionEnabled()) {
    return;
  }

  content::NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();

  if (!visible_entry) {
    LOG(WARNING) << "SafeBrowsing no visible entry, skip.";
    return;
  }

  GURL virtual_url = visible_entry->GetVirtualURL();
  std::vector<GURL> redirect_chain = visible_entry->GetRedirectChain();
  redirect_chain.push_back(virtual_url);
  std::reverse(redirect_chain.begin(), redirect_chain.end());

  bool is_page_type_normal = 
      (visible_entry->GetPageType() == content::PAGE_TYPE_NORMAL);
  if (!is_page_type_normal) {
    return;
  }

  for (auto& url : redirect_chain) {
    const auto& blocking_cached = blocking_page_info_cache.Peek(url);
    if (blocking_cached != blocking_page_info_cache.end()) {
      SbBlockingPageInfo blocking_page_info = blocking_cached->second;
      bool is_display_block_page = false;
      int policy = blocking_page_info.info_policy;
      OHSBThreatType block_type = blocking_page_info.info_block_type;

      if (block_type == OHSBThreatType::THREAT_WARNING &&
          policy == OHSBPolicyType::POLICY_HALF_POPUP) {
        NotifySafeBrowsingCheckResult(block_type);
        return;
      }

      if ((policy == OHSBPolicyType::POLICY_POPUP_AND_DANGER ||
          policy == OHSBPolicyType::POLICY_FORBIDDEN_PROHIBIT_ACCESS) &&
          block_type != OHSBThreatType::THREAT_WARNING) {
        is_display_block_page = true;
        NotifySafeBrowsingCheckResult(block_type);
      }

      if (InMaliciousAllowlist(prefs_, url.has_host() ? url.host() : url.spec(),
                               incognito_mode_)) {
        LOG(WARNING) << "SafeBrowsing in malicious allowlist.";
        break;
      }

      if (is_display_block_page) {
         DisplayBlockingPage(url, policy, block_type, "zh-CN");
         return;
      }
    }
  }
}

void OhosSbClient::SetEvilUrlPolicyAndHwCode(const GURL& url,
                                         int policy,
                                         OHSBThreatType block_type,
                                         int hw_code,
                                         const GURL& redirect_url) {
  BlockingPageInfo::SetBlockingPageInfo(web_contents(), url, policy, block_type,
                                        hw_code, redirect_url);
  content::NavigationEntry* pending_entry =
      web_contents()->GetController().GetPendingEntry();
  if (!pending_entry && !web_contents()->IsWaitingForResponse()) {
    ShowBlockingPage();
  }
}

OhosSbClient::BlockingPageInfo::BlockingPageInfo(content::WebContents* web_contents)
    : content::WebContentsUserData<OhosSbClient::BlockingPageInfo>(*web_contents) {}

OhosSbClient::SbBlockingPageInfo::SbBlockingPageInfo() {}

OhosSbClient::SbBlockingPageInfo::SbBlockingPageInfo(
    const GURL& url,
    ohos_safe_browsing::OHSBThreatType block_type,
    int hw_code,
    int policy,
    const GURL& redirect_url)
    : info_url(url),
      info_hw_code(hw_code),
      info_block_type(block_type),
      info_policy(policy),
      info_redirect_url(redirect_url) {}

OhosSbClient::SbBlockingPageInfo::~SbBlockingPageInfo() {}

// static
void OhosSbClient::BlockingPageInfo::SetBlockingPageInfo(
    content::WebContents* web_contents,
    const GURL& url,
    int policy,
    OHSBThreatType block_type,
    int hw_code,
    const GURL& redirect_url) {
  BlockingPageInfo::CreateForWebContents(web_contents);
  BlockingPageInfo* blocking_page_info =
      BlockingPageInfo::FromWebContents(web_contents);
  blocking_page_info->url_ = url;
  blocking_page_info->policy_ = policy;
  blocking_page_info->block_type_ = block_type;
  blocking_page_info->hw_code_ = hw_code;
  blocking_page_info->redirect_url_ = redirect_url;
  if (policy == OHSBPolicyType::POLICY_POPUP_AND_DANGER ||
      policy == OHSBPolicyType::POLICY_FORBIDDEN_PROHIBIT_ACCESS ||
      policy == OHSBPolicyType::POLICY_HALF_POPUP) {
    SbBlockingPageInfo info(url, block_type, hw_code, policy, redirect_url);
    blocking_page_info_cache.Put(url, info);
  }
}

void OhosSbClient::DisplayBlockingPage(const GURL& url,
                                   int policy,
                                   OHSBThreatType block_type,
                                   const std::string& app_locale) {
  LOG(INFO) << "SafeBrowsing " << __func__ << " url: " << url.spec()
            << ", type: " << block_type;
  auto controller = std::make_unique<OhosSbControllerClient>(
      web_contents(), prefs_, url, app_locale, incognito_mode_);
  std::unique_ptr<OhosSbBlockPage> blocking_page = std::make_unique<OhosSbBlockPage>(
      web_contents(), url, policy, block_type, std::move(controller));
  base::WeakPtr<content::NavigationHandle> error_page_navigation_handle =
      web_contents()->GetController().LoadPostCommitErrorPage(
          web_contents()->GetPrimaryMainFrame(), url,
          blocking_page->GetHTMLContents(), net::ERR_BLOCKED_BY_CLIENT);
  if (error_page_navigation_handle.get()) {
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(error_page_navigation_handle.get(),
                              std::move(blocking_page));
  }
}

bool OhosSbClient::IsBlockPageShowing() {
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(web_contents());
  return security_interstitial_tab_helper &&
         security_interstitial_tab_helper->IsDisplayingInterstitial();
}

void OhosSbClient::NotifySafeBrowsingCheckResult(OHSBThreatType threat_type) {
  if (!web_contents()) {
    return;
  }
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents());
  if (!browser.get()) {
    return;
  }
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get()) {
    return;
  }
  CefRefPtr<CefLoadHandler> load_handler = client->GetLoadHandler();
  if (!load_handler.get()) {
    return;
  }
  int type = static_cast<int>(threat_type);
  load_handler->OnSafeBrowsingCheckResult(type);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhosSbClient::BlockingPageInfo);

}  // namespace ohos_safe_browsing
