// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ARKWEB_USERAGENT_UTILS_H_
#define CEF_LIBCEF_BROWSER_ARKWEB_USERAGENT_UTILS_H_
#pragma once

#include "content/public/browser/navigation_handle.h"
#if BUILDFLAG(ARKWEB_USERAGENT)
#include "content/public/browser/web_contents.h"
#endif

namespace arkweb_useragent_utils {
  void MaybeOverrideUserAgentOnStartNavigation(content::NavigationHandle* navigation);
  void MaybeOverrideUserAgentOnRedirectNavigation(content::NavigationHandle* navigation);
  std::string GetUAStringForHost(content::WebContents* web_contents, const std::string& host);
} // namespace arkweb_useragent_utils

#endif // CEF_LIBCEF_BROWSER_ARKWEB_USERAGENT_UTILS_H_
