// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef ARKWEB_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_EXT_H_
#define ARKWEB_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_EXT_H_

#include "base/functional/callback_forward.h"
#include "cef/libcef/browser/browser_context.h"
#include "net/cookies/canonical_cookie.h"

namespace net_service::cookie_helper {
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
bool ShouldForceIgnoreSiteForCookies(
    const CefBrowserContext::Getter& browser_context_getter,
    const network::ResourceRequest& request);
#endif

}  // namespace net_service::cookie_helper

#endif  // ARKWEB_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_EXT_H_
