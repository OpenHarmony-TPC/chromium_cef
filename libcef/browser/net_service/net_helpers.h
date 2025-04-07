// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_

#ifdef OHOS_CUSTOM_DNS
#include <map>
#include <vector>
#endif
#include "net/dns/public/secure_dns_mode.h"
class GURL;

namespace net_service {

#define NETHELPERS_EXPORT __attribute__((visibility("default")))

#ifdef OHOS_NETWORK_CONNINFO
struct NetHelperSetting {
  bool file_access;
  bool block_network;
  int cache_mode;
  std::vector<std::string> file_access_dirs_list;
  bool disallow_sandbox_file_access_from_file_url;
};
#endif

#ifdef OHOS_CUSTOM_DNS
struct CustomDnsEntry {
  std::vector<std::string> address;
  int32_t ttl;
};
#endif // defined(OHOS_CUSTOM_DNS)

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

#if defined(OHOS_CUSTOM_DNS)
  static void SetHostIP(const std::string host_name, const std::string address, int32_t ttl);
  static std::map<std::string, struct CustomDnsEntry> GetHostIP();
  static std::vector<std::string> GetHostIP(const std::string host_name);
  static void ClearHostIP(const std::string host_name);
  static void ClearHostIP();
  static std::map<std::string, struct CustomDnsEntry> custom_dns; 
#endif // defined(OHOS_CUSTOM_DNS)

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
