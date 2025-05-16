// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_EXT_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_EXT_H_
#pragma once

#include "build/build_config.h"
#include "cef/include/cef_base.h"
#include "cef/libcef/browser/browser_contents_delegate.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_FAVICON)
#include "ohos_cef_ext/libcef/browser/arkweb_icon_helper_ext.h"
#endif

class ArkWebBrowserContentsDelegateExt : public CefBrowserContentsDelegate {
 public:
  ArkWebBrowserContentsDelegateExt* AsArkWebBrowserContentsDelegateExt()
      override {
    return this;
  }
  ArkWebBrowserContentsDelegateExt(scoped_refptr<CefBrowserInfo> browser_info);

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
  void ViewportFitChanged(blink::mojom::ViewportFit value) override;
#endif

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  void OnRefreshAccessedHistoryEx(CefRefPtr<CefFrame> frame,
                                  const GURL& url,
                                  bool isReload) override {
    OnRefreshAccessedHistory(frame, url, isReload);
  }
  void OnRefreshAccessedHistory(CefRefPtr<CefFrame> frame,
                                const GURL& url,
                                bool isReload);
#endif

#if BUILDFLAG(ARKWEB_WPT)
  void DidStartNavigation(content::NavigationHandle* navigation) override;
#endif  // BUILDFLAG(ARKWEB_WPT)

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
  void OnActivateContent() override;
#endif

#if BUILDFLAG(ARKWEB_FAVICON)
  void InitIconHelper();
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  void OnOldPageNoLongerRendered(const GURL& url, bool success);
#endif
#if BUILDFLAG(ARKWEB_NAVIGATION)
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  // Shows the repost form confirmation dialog box.
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
#endif  // ARKWEB_NETWORK_BASE

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
  void WebExtensionUpdateTab(
      int32_t tab_id,
      const NWebExtensionTabUpdateProperties* update_properties) override;

#endif
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_EXT_H_
