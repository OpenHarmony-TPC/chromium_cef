// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/extensions/alloy_extensions_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

namespace extensions {
namespace alloy {

int GetTabIdForWebContents(content::WebContents* web_contents) {
  auto browser = AlloyBrowserHostImpl::GetBrowserForContents(web_contents);
  if (!browser) {
    return -1;
  }

#ifdef OHOS_ARKWEB_EXTENSIONS
  if (browser->GetTabId() >= 0) {
    return browser->GetTabId();
  }
#endif // OHOS_ARKWEB_EXTENSIONS

  return browser->GetIdentifier();
}

}  // namespace alloy
}  // namespace extensions
