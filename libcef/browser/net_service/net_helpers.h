// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_

class GURL;

namespace net_service {

#define NETHELPERS_EXPORT __attribute__((visibility("default")))

class NETHELPERS_EXPORT NetHelpers {
 public:
  static bool ShouldBlockContentUrls();
  static bool ShouldBlockFileUrls();
  static bool IsAllowAcceptCookies();
  static bool IsThirdPartyCookieAllowed();

  static bool allow_content_access;
  static bool allow_file_access;
  static bool is_network_blocked;
  static bool accept_cookies;
  static bool third_party_cookies;
  static int cache_mode;
};

bool IsSpecialFileUrl(const GURL& url);

// Update request's |load_flags| based on the settings.
int UpdateLoadFlags(int load_flags);

// Returns true if the given URL should be aborted with
// net::ERR_ACCESS_DENIED.
bool IsURLBlocked(const GURL& url);

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_H_
