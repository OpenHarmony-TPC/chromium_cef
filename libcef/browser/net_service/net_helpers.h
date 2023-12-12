// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_

#include "net/dns/public/secure_dns_mode.h"
class GURL;

namespace net_service {

#define NETHELPERS_EXPORT __attribute__((visibility("default")))

#ifdef OHOS_NETWORK_CONNINFO
struct NetHelperSetting {
  bool file_access;
  bool block_network;
  int cache_mode;
};
#endif

class NETHELPERS_EXPORT NetHelpers {
 public:
  static bool ShouldBlockContentUrls();
  static bool ShouldBlockFileUrls();
  static bool IsAllowAcceptCookies();
  static bool IsThirdPartyCookieAllowed();
#ifdef OHOS_NETWORK_CONNINFO
  static bool ShouldBlockFileUrls(struct NetHelperSetting setting);
#endif

#if defined(OHOS_HTTP_DNS)
  static net::SecureDnsMode DnsOverHttpMode();
  static std::string DnsOverHttpServerConfig();
  static bool HasValidDnsOverHttpConfig();
#endif  // defined(OHOS_HTTP_DNS)

  static bool allow_content_access;
  static bool allow_file_access;
  static bool is_network_blocked;
  static bool accept_cookies;
  static bool third_party_cookies;
  static int cache_mode;
  static int connection_timeout;

#if defined(OHOS_HTTP_DNS)
  static int doh_mode;
  static std::string doh_config;
#endif  // defined(OHOS_HTTP_DNS)
};

bool IsSpecialFileUrl(const GURL& url);

// Update request's |load_flags| based on the settings.
int UpdateLoadFlags(int load_flags
#ifdef OHOS_NETWORK_CONNINFO
                    ,
                    struct NetHelperSetting setting
#endif
);

// Returns true if the given URL should be aborted with
// net::ERR_ACCESS_DENIED.
bool IsURLBlocked(const GURL& url
#ifdef OHOS_NETWORK_CONNINFO
                  ,
                  struct NetHelperSetting setting
#endif
);

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_
