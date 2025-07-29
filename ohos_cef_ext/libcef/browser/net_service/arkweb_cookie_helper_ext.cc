// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/net_service/cookie_helper.h"
#include "cef/ohos_cef_ext/libcef/browser/net_service/arkweb_cookie_helper_ext.h"

#include "arkweb/build/features/features.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/resource_request.h"

#if BUILDFLAG(ARKWEB_EXT_EXCEPTION_LIST)
#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#endif

namespace net_service::cookie_helper {

#if BUILDFLAG(ARKWEB_EXT_EXCEPTION_LIST)
bool CanSaveOrLoadCookies(
    const CefBrowserContext::Getter& browser_context_getter,
    const network::ResourceRequest& request) {
  auto cef_browser_context = GetBrowserContext(browser_context_getter);
  auto browser_context =
      cef_browser_context ? cef_browser_context->AsBrowserContext() : nullptr;
  if (!browser_context) {
    LOG(ERROR) << "Can not get browser_context.";
    return true;
  }

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context);
  if (!host_content_settings_map) {
    LOG(ERROR) << "Can not get host_content_settings_map.";
    return true;
  }

  ContentSettingsForOneType cookie_settings =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::COOKIES);

  const auto& entry = base::ranges::find_if(
      cookie_settings, [&](const ContentSettingPatternSource& entry) {
        // The primary pattern is for the request URL; the secondary pattern
        // is for the first-party URL (which is the top-frame origin [if
        // available] or the site-for-cookies).
        return !entry.IsExpired() &&
               entry.primary_pattern.Matches(request.url) &&
               entry.secondary_pattern.Matches(request.url);
      });
  const ContentSettingPatternSource* match =
      (entry == cookie_settings.end() ? nullptr : &*entry);
  return !(match && match->GetContentSetting() == CONTENT_SETTING_BLOCK);
}
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
// Logic here is the same as in
// network::URLLoader::ShouldForceIgnoreSiteForCookies.
bool ShouldForceIgnoreSiteForCookies(
    const CefBrowserContext::Getter& browser_context_getter,
    const network::ResourceRequest& request) {
  auto cef_browser_context = GetBrowserContext(browser_context_getter);
  auto browser_context =
      cef_browser_context ? cef_browser_context->AsBrowserContext() : nullptr;
  if (!browser_context) {
    LOG(ERROR) << "Get browser_context failed.";
    return false;
  }

  const auto& origin_access_list =
      browser_context->GetSharedCorsOriginAccessList()->GetOriginAccessList();

  if (request.request_initiator.has_value() &&
      network::cors::OriginAccessList::AccessState::kAllowed ==
          origin_access_list.CheckAccessState(request.request_initiator.value(),
                                              request.url)) {
    return true;
  }

  url::Origin site_origin =
      url::Origin::Create(request.site_for_cookies.RepresentativeUrl());
  if (!site_origin.opaque() && request.request_initiator.has_value()) {
    bool site_can_access_target =
        network::cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list.CheckAccessState(site_origin, request.url);
    bool site_can_access_initiator =
        network::cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list.CheckAccessState(
            site_origin, request.request_initiator->GetURL());
    net::SiteForCookies site_of_initiator =
        net::SiteForCookies::FromOrigin(request.request_initiator.value());
    bool are_initiator_and_target_same_site =
        site_of_initiator.IsFirstParty(request.url);
    if (site_can_access_initiator && site_can_access_target &&
        are_initiator_and_target_same_site) {
      return true;
    }
  }

  return false;
}
#endif

}  // namespace net_service::cookie_helper
