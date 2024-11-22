// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
#define CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "libcef/common/tracker.h"
#include "libcef/renderer/frame_impl.h"

#include "third_party/blink/public/web/web_view_observer.h"

namespace blink {
class WebFrame;
class WebNode;
class WebView;
}  // namespace blink

// Renderer plumbing for CEF features. There is a one-to-one relationship
// between RenderView on the renderer side and RenderViewHost on the browser
// side.
//
// RenderViewObserver: Interface for observing RenderView notifications.
class CefBrowserImpl : public CefBrowser, public blink::WebViewObserver {
 public:
  // Returns the browser associated with the specified RenderView.
  static CefRefPtr<CefBrowserImpl> GetBrowserForView(blink::WebView* view);
  // Returns the browser associated with the specified main WebFrame.
  static CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(
      blink::WebFrame* frame);

  // CefBrowser methods.
  bool IsValid() override;
  CefRefPtr<CefBrowserHost> GetHost() override;
  bool CanGoBack() override;
  void GoBack() override;
  bool CanGoForward() override;
  void GoForward() override;
  bool IsLoading() override;
  void Reload() override;
  void ReloadIgnoreCache() override;
  void StopLoad() override;
  int GetIdentifier() override;
  bool IsSame(CefRefPtr<CefBrowser> that) override;
  bool IsPopup() override;
  bool HasDocument() override;
  CefRefPtr<CefFrame> GetMainFrame() override;
  CefRefPtr<CefFrame> GetFocusedFrame() override;
  CefRefPtr<CefFrame> GetFrame(int64 identifier) override;
  CefRefPtr<CefFrame> GetFrame(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<int64>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;

  CefBrowserImpl(blink::WebView* web_view,
                 int browser_id,
                 bool is_popup,
                 bool is_windowless);

  CefBrowserImpl(const CefBrowserImpl&) = delete;
  CefBrowserImpl& operator=(const CefBrowserImpl&) = delete;

  ~CefBrowserImpl() override;

  // Returns the matching CefFrameImpl reference or creates a new one.
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(blink::WebLocalFrame* frame);
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(int64_t frame_id);

  // Frame objects will be deleted immediately before the frame is closed.
  void AddFrameObject(int64_t frame_id, CefTrackNode* tracked_object);

  int browser_id() const { return browser_id_; }
  bool is_popup() const { return is_popup_; }
  bool is_windowless() const { return is_windowless_; }

  // blink::WebViewObserver methods.
  void OnDestruct() override;
  void FrameDetached(int64_t frame_id);

  void OnLoadingStateChange(bool isLoading);
  void OnEnterBFCache();

#if BUILDFLAG(IS_OHOS)
  void DidCommitCompositorFrame() override;
  void DidUpdateMainFrameLayout() override;
  uint32_t GetAcceleratedWidget() override;
#endif

#if BUILDFLAG(IS_OHOS)
  bool CanGoBackOrForward(int num_steps) override;
  void GoBackOrForward(int num_steps) override;
  void DeleteHistory() override;
  bool CanStoreWebArchive() override;
  void ReloadOriginalUrl() override;
  void SetBrowserUserAgentString(const CefString& user_agent) override {}
  CefRefPtr<CefBrowserPermissionRequestDelegate> GetPermissionRequestDelegate()
      override;
  CefRefPtr<CefGeolocationAcess> GetGeolocationPermissions() override;

#ifdef OHOS_ARKWEB_ADBLOCK
  void EnableAdsBlock(bool enable) override {}
  bool IsAdsBlockEnabled() override { return false; }
  bool IsAdsBlockEnabledForCurPage() override { return false; }
#endif

  // #ifdefined(OHOS_EX_PASSWORD)
  void SetSavePasswordAutomatically(bool enable) override{}
  bool GetSavePasswordAutomatically() override { return false; }
  void SetSavePassword(bool enable) override{}
  bool GetSavePassword() override { return false; }
  void SaveOrUpdatePassword(bool is_update) override{}
  void PasswordSuggestionSelected(int list_index) override{}
  // #endif
  // #ifdefined(OHOS_EX_FREE_COPY)
  void SelectAndCopy() override{}
  bool ShouldShowFreeCopy() override { return false; }
  // #endif
  int GetNWebId() override { return -1; }
  void SetEnableBlankTargetPopupIntercept(
      bool enableBlankTargetPopup) override {}
#if defined(OHOS_NO_STATE_PREFETCH)
  void PrefetchPage(CefString& url, CefString& additionalHttpHeaders) override {
  }
#endif  // defined(OHOS_NO_STATE_PREFETCH)
  // #ifdefined(OHOS_EX_FORCE_ZOOM)
  void SetForceEnableZoom(bool forceEnableZoom) override {}
  bool GetForceEnableZoom() override { return false; }
  // #endif

  // #if defined(OHOS_SECURITY_STATE)
  int GetSecurityLevel() override{ return 0; }
  // #endif
#endif

#if BUILDFLAG(IS_OHOS)
  bool IsSafeBrowsingEnabled() override{ return false; }
  void EnableSafeBrowsing(bool enable) override{}
#endif

#ifdef OHOS_ITP
  void EnableIntelligentTrackingPrevention(bool enable) override {}
  bool IsIntelligentTrackingPreventionEnabled() override { return false; }
#endif

#ifdef OHOS_URL_TRUST_LIST
  int SetUrlTrustListWithErrMsg(
    const CefString& urlTrustList, CefString& detailErrMsg) override {
    return 0;
  }
#endif

  // #if defined(OHOS_NWEB_EX)
  // NOTE: Keep the previous line commented, add NWebEx APIs below.
  bool ShouldShowLoadingUI() override;
  // OHOS_EX_TOPCONTROLS
  void UpdateBrowserControlsState(int constraints,
                                  int current,
                                  bool animate) override {}
  void UpdateBrowserControlsHeight(int height, bool animate) override {}
  // #endif  // defined(OHOS_NWEB_EX)

#ifdef OHOS_BFCACHE
  void SetBackForwardCacheOptions(int32_t size, int32_t timeToLive) override {}
#endif

 private:
  // ID of the browser that this RenderView is associated with. During loading
  // of cross-origin requests multiple RenderViews may be associated with the
  // same browser ID.
  int browser_id_;
  bool is_popup_;
  bool is_windowless_;

  // Map of unique frame ids to CefFrameImpl references.
  using FrameMap = std::map<int64, CefRefPtr<CefFrameImpl>>;
  FrameMap frames_;

  // True if the browser was in the BFCache.
  bool was_in_bfcache_ = false;

  // Map of unique frame ids to CefTrackManager objects that need to be cleaned
  // up when the frame is deleted.
  using FrameObjectMap = std::map<int64, CefRefPtr<CefTrackManager>>;
  FrameObjectMap frame_objects_;

  struct LoadingState {
    LoadingState(bool is_loading, bool can_go_back, bool can_go_forward)
        : is_loading_(is_loading),
          can_go_back_(can_go_back),
          can_go_forward_(can_go_forward) {}

    bool IsMatch(bool is_loading, bool can_go_back, bool can_go_forward) const {
      return is_loading_ == is_loading && can_go_back_ == can_go_back &&
             can_go_forward_ == can_go_forward;
    }

    bool is_loading_;
    bool can_go_back_;
    bool can_go_forward_;
  };
  std::unique_ptr<LoadingState> last_loading_state_;

#if BUILDFLAG(IS_OHOS)
  int content_width_ = 0;
  int content_height_ = 0;
  bool needs_contents_size_update_ = true;
#endif

  IMPLEMENT_REFCOUNTING(CefBrowserImpl);
};

#endif  // CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
