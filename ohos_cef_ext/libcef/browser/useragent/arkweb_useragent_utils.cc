// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/browser/useragent/arkweb_useragent_utils.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_ua_config.h"
#endif
#include "cef/ohos_cef_ext/libcef/browser/alloy/alloy_browser_ua_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace arkweb_useragent_utils {

bool IsIllegalUrl(const GURL& url) {
  return url.is_empty() || !url.is_valid() || !url.has_host();
}

std::string GetDomainAndRegistry(const GURL& url) {
  auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return base::ToLowerASCII(domain);
}

void UpdateUserAgentForNavigation(content::NavigationHandle* navigation,
                                  const std::string& user_agent,
                                  const UserAgentOverridePolicy& match_policy) {
  navigation->SetRequestHeader(net::HttpRequestHeaders::kUserAgent, user_agent);
  if (!navigation->IsInMainFrame()) {
    return;
  }

  if (auto* web_contents = navigation->GetWebContents()) {
    blink::UserAgentOverride user_agent_override =
        blink::UserAgentOverride::UserAgentOnly(user_agent);
    user_agent_override.from_app =
        (match_policy < UserAgentOverridePolicy::ARKWEB_DEFAULT);
    web_contents->SetUserAgentOverride(user_agent_override, true);
  }
}

UserAgentOverridePolicy MatchUserAgent(content::NavigationHandle* navigation,
                                       const std::string& host,
                                       std::string& user_agent) {
  if (auto* web_contents = navigation->GetWebContents()) {
    std::string custom_ua = web_contents->GetCustomUA();
    if (!custom_ua.empty()) {
      user_agent = custom_ua;
      return UserAgentOverridePolicy::CUSTOM;
    }
  }
  std::string user_agent_no_nweb_ex = user_agent;
  std::string user_agent_nweb_ex = user_agent;
  auto match_type_no_nweb_ex =
      AlloyBrowserUAConfig::GetInstance()->MatchUserAgent(
          host, user_agent_no_nweb_ex);
  user_agent = user_agent_no_nweb_ex;
  return match_type_no_nweb_ex;
}

// Return the host of referrer_url only when the main frame
// is not reloaded or triggered by user gestures or serverd from
// back_forward_cache.
std::string GetHostOfReferrerUrlIfNeed(content::NavigationHandle* navigation,
                                       bool is_reload) {
  if (!navigation->IsInMainFrame() || navigation->HasUserGesture() ||
      navigation->IsServedFromBackForwardCache() || is_reload) {
    return std::string();
  }

  const GURL referrer_url = navigation->GetReferrer().url;
  if (IsIllegalUrl(referrer_url)) {
    return std::string();
  }

  if (GetDomainAndRegistry(referrer_url) ==
      GetDomainAndRegistry(navigation->GetURL())) {
    return referrer_url.host();
  }
  return std::string();
}

void MaybeOverrideUserAgentOnStartNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation) {
    return;
  }

  // UserAgent won't be added when the navigation_request created by renderer
  // process.
  if (content::NavigationRequest::From(navigation)
          ->is_synchronous_renderer_commit()) {
    return;
  }

  const GURL& url = navigation->GetURL();
  if (IsIllegalUrl(url)) {
    return;
  }

  bool is_reload = false;

  std::string final_ua;
  std::string host = url.host();

  auto match_type = MatchUserAgent(navigation, url.host(), final_ua);
  if (match_type >= UserAgentOverridePolicy::APP_DEFAULT) {
    is_reload =
        PageTransitionCoreTypeIs(navigation->GetPageTransition(),
                                 ui::PageTransition::PAGE_TRANSITION_RELOAD);
    std::string referer_host =
        GetHostOfReferrerUrlIfNeed(navigation, is_reload);
    if (!referer_host.empty()) {
      host = referer_host;
      MatchUserAgent(navigation, host, final_ua);
    }
  }

  LOG(DEBUG) << __func__ << " host " << host << ", final_ua " << final_ua
             << ", user_gesture " << navigation->HasUserGesture()
             << ", main_frame " << navigation->IsInMainFrame() << ", reload "
             << is_reload << ", serverd_from_bfcache "
             << navigation->IsServedFromBackForwardCache() << ", match_type "
             << match_type;

  UpdateUserAgentForNavigation(navigation, final_ua, match_type);
}

void MaybeOverrideUserAgentOnRedirectNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation) {
    return;
  }

  const auto& redirect_chain = navigation->GetRedirectChain();
  int chain_size = redirect_chain.size();
  constexpr int kLeastLengthForRedirectChain = 2;
  if (chain_size < kLeastLengthForRedirectChain) {
    return;
  }

  const auto& current_url = navigation->GetURL();
  if (IsIllegalUrl(current_url)) {
    return;
  }

  std::string final_ua;
  auto match_type = MatchUserAgent(navigation, current_url.host(), final_ua);
  if (match_type >= UserAgentOverridePolicy::APP_DEFAULT) {
    for (int i = chain_size - 1; i > 0; i--) {
      if (IsIllegalUrl(redirect_chain[i]) ||
          IsIllegalUrl(redirect_chain[i - 1])) {
        continue;
      }
      if (GetDomainAndRegistry(redirect_chain[i]) !=
          GetDomainAndRegistry(redirect_chain[i - 1])) {
        continue;
      }
      match_type =
          MatchUserAgent(navigation, redirect_chain[i - 1].host(), final_ua);
      if (match_type < UserAgentOverridePolicy::APP_DEFAULT) {
        break;
      }
    }
  }

  LOG(DEBUG) << __func__ << " host " << current_url.host()
             << " match_type:" << match_type << ", final_ua " << final_ua
             << ", user_gesture " << navigation->HasUserGesture()
             << ", main_frame " << navigation->IsInMainFrame()
             << ", serverd_from_bfcache "
             << navigation->IsServedFromBackForwardCache();
  UpdateUserAgentForNavigation(navigation, final_ua, match_type);
} // arkweb_useragent_utils