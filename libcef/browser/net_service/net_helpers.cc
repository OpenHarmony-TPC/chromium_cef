// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net_service/net_helpers.h"

#include "base/base_paths_ohos.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "include/base/cef_logging.h"
#include "net/base/load_flags.h"
#include "url/gurl.h"

#if defined(OHOS_HTTP_DNS)
#include <string>
#include "build/build_config.h"
#include "net/dns/public/secure_dns_mode.h"
#endif  // defined(OHOS_HTTP_DNS)

namespace net_service {

namespace {
int UpdateCacheLoadFlags(int load_flags, int cache_control_flags) {
  const int all_cache_control_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_VALIDATE_CACHE |
      net::LOAD_SKIP_CACHE_VALIDATION | net::LOAD_ONLY_FROM_CACHE;
  DCHECK_EQ((cache_control_flags & all_cache_control_flags),
            cache_control_flags);
  load_flags &= ~all_cache_control_flags;
  load_flags |= cache_control_flags;
  return load_flags;
}
}  // namespace

bool NetHelpers::allow_content_access = false;
bool NetHelpers::allow_file_access = false;
bool NetHelpers::is_network_blocked = false;
bool NetHelpers::accept_cookies = true;
bool NetHelpers::third_party_cookies = false;
int NetHelpers::cache_mode = 0;
int NetHelpers::connection_timeout = 30;

#if defined(OHOS_HTTP_DNS)
int NetHelpers::doh_mode = -1;
std::string NetHelpers::doh_config = "";
#endif  // defined(OHOS_HTTP_DNS)

bool NetHelpers::ShouldBlockContentUrls() {
  return !allow_content_access;
}

#ifdef OHOS_NETWORK_CONNINFO
bool NetHelpers::ShouldBlockFileUrls(struct NetHelperSetting setting) {
  return !setting.file_access;
}
#endif

bool NetHelpers::ShouldBlockFileUrls() {
  return !allow_file_access;
}

bool NetHelpers::IsAllowAcceptCookies() {
  return accept_cookies;
}

bool NetHelpers::IsThirdPartyCookieAllowed() {
  return third_party_cookies;
}

#if defined(OHOS_HTTP_DNS)
net::SecureDnsMode NetHelpers::DnsOverHttpMode() {
  net::SecureDnsMode enum_sec_dns_mode = net::SecureDnsMode::kAutomatic;
  switch (doh_mode) {
    case 2:
      enum_sec_dns_mode = net::SecureDnsMode::kSecure;
      break;
    case 1:
      enum_sec_dns_mode = net::SecureDnsMode::kAutomatic;
      break;
    case 0:
      enum_sec_dns_mode = net::SecureDnsMode::kOff;
      break;
    default:
      LOG(WARNING) << __func__ << " User input mal mode:" << doh_mode;
      break;
  }

  return enum_sec_dns_mode;
}

std::string NetHelpers::DnsOverHttpServerConfig() {
  return doh_config;
}

bool NetHelpers::HasValidDnsOverHttpConfig() {
  if (doh_config.empty()) {
    return false;
  }

  if (doh_mode > static_cast<int>(net::SecureDnsMode::kSecure) ||
      doh_mode < static_cast<int>(net::SecureDnsMode::kOff)) {
    return false;
  }

  return true;
}
#endif  // defined(OHOS_HTTP_DNS)

bool IsSpecialFileUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsFile() || !url.has_path()) {
    return false;
  }
  base::FilePath app_data_path;
  if (!base::PathService::Get(base::DIR_OHOS_APP_DATA, &app_data_path)) {
    return false;
  }
  base::FilePath app_bundle_path;
  if (!base::PathService::Get(base::DIR_OHOS_APP_INSTALLATION,
                              &app_bundle_path)) {
    return false;
  }
  LOG(INFO) << "app_data_path:" << app_data_path.value()
            << ", app_bundle_path:" << app_bundle_path.value();

  if (base::StartsWith(url.path(), app_data_path.value(),
                       base::CompareCase::SENSITIVE)) {
    return true;
  }

  if (base::StartsWith(url.path(), app_bundle_path.value(),
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  return false;
}

bool IsURLBlocked(const GURL& url
#ifdef OHOS_NETWORK_CONNINFO
                  ,
                  struct NetHelperSetting setting
#endif
) {
  // Part of implementation of NWebPreference.allowContentAccess.
  if (url.SchemeIs(url::kContentScheme) &&
      NetHelpers::ShouldBlockContentUrls()) {
    return true;
  }

  // Part of implementation of NWebPreference.allowFileAccess.
#ifdef OHOS_NETWORK_CONNINFO
  if (url.SchemeIsFile() && NetHelpers::ShouldBlockFileUrls(setting)) {
#else
  if (url.SchemeIsFile() && NetHelpers::ShouldBlockFileUrls()) {
#endif
    // Appdatas are always available.
    return !IsSpecialFileUrl(url);
  }

#ifdef OHOS_NETWORK_CONNINFO
  return setting.block_network && url.SchemeIs(url::kFtpScheme);
#else
  return NetHelpers::is_network_blocked && url.SchemeIs(url::kFtpScheme);
#endif
}

int UpdateLoadFlags(int load_flags
#ifdef OHOS_NETWORK_CONNINFO
                    ,
                    NetHelperSetting setting
#endif
) {
#ifdef OHOS_NETWORK_CONNINFO
  if (setting.block_network) {
#else
  if (NetHelpers::is_network_blocked) {
#endif
    LOG(INFO) << "Update cache control flag to block network.";
    return UpdateCacheLoadFlags(
        load_flags,
        net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION);
  }

#ifdef OHOS_NETWORK_CONNINFO
  if (!setting.cache_mode) {
#else
  if (!NetHelpers::cache_mode) {
#endif
    return load_flags;
  }

#ifdef OHOS_NETWORK_CONNINFO
  return UpdateCacheLoadFlags(load_flags, setting.cache_mode);
#else
  return UpdateCacheLoadFlags(load_flags, NetHelpers::cache_mode);
#endif
}

}  // namespace net_service
