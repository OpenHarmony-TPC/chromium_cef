// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/browser/arkweb_browser_contents_delegate_ext.h"

#include "arkweb/build/features/features.h"
#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/native/cursor_util.h"
#include "cef/libcef/common/frame_util.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/common/chrome_result_codes.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-test-utils.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_ARKWEB)
#include "cef/ohos_cef_ext/include/arkweb_load_handler_ext.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_UA)
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_ua_config.h"
#include "base/command_line.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/common/content_switches.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#endif

#if BUILDFLAG(ARKWEB_NAVIGATION)
#include "content/public/browser/navigation_details.h"
#include "ohos_cef_ext/libcef/browser/load_committed_details_impl.h"
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)

namespace {
#if BUILDFLAG(ARKWEB_EXT_UA)
// Return the host of referrer_url only when the main frame
// is not reloaded or triggered by user gestures or serverd from
// back_forward_cache.
std::string GetHostOfReferrerUrlIfNeed(content::NavigationHandle* navigation,
                                       bool is_reload) {
  if (!navigation->IsInMainFrame() || navigation->HasUserGesture() ||
      navigation->IsServedFromBackForwardCache() || is_reload) {
    return std::string();
  }

  const GURL referrer_url = navigation->GetReferrer().url;
  if (!referrer_url.is_valid() || referrer_url.is_empty() ||
      !referrer_url.has_host()) {
    return std::string();
  }

  std::string referrer_sld =
      net::registry_controlled_domains::GetDomainAndRegistry(
          referrer_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string sld = net::registry_controlled_domains::GetDomainAndRegistry(
      navigation->GetURL(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (referrer_sld == sld) {
    return referrer_url.host();
  }
  return std::string();
}

void MaybeSetUserAgentOverrideForMainFrame(
    content::NavigationHandle* navigation) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExUa) ||
      !navigation) {
    return;
  }

  // UserAgent won't be added when the navigation_request created by renderer
  // process.
  if (content::NavigationRequest::From(navigation)
          ->is_synchronous_renderer_commit()) {
    return;
  }

  const GURL& url = navigation->GetURL();
  if (url.is_empty() || !url.is_valid() || !url.has_host()) {
    return;
  }

  std::string host = url.host();
  bool is_reload =
      PageTransitionCoreTypeIs(navigation->GetPageTransition(),
                               ui::PageTransition::PAGE_TRANSITION_RELOAD);
  std::string host_of_referer_url =
      GetHostOfReferrerUrlIfNeed(navigation, is_reload);
  if (!host_of_referer_url.empty()) {
    host = host_of_referer_url;
  }
  content::WebContents* web_contents = navigation->GetWebContents();
  std::string final_ua;
  if (web_contents && !web_contents->GetCustomUA().empty()) {
    final_ua = web_contents->GetCustomUA();
    LOG(INFO) << "GetCustomUA: " << final_ua;
  } else {
    final_ua =
      nweb_ex::AlloyBrowserUAConfig::GetInstance()->GetUserAgentForHost(host);
  }
  LOG(DEBUG) << "DidStartNavigation, host " << host << ", final_ua " << final_ua
             << ", user_gesture " << navigation->HasUserGesture()
             << ", is_main_frame " << navigation->IsInMainFrame()
             << ", is_reload " << is_reload
             << ", is_serverd_from_back_forward_cache "
             << navigation->IsServedFromBackForwardCache();

  navigation->SetRequestHeader(net::HttpRequestHeaders::kUserAgent, final_ua);
  if (!navigation->IsInMainFrame()) {
    return;
  }

  if (!web_contents) {
    return;
  }
  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(final_ua), true);
}
#endif  // ARKWEB_EXT_UA
}  // namespace

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
class DataResubmissionCallbackImpl : public CefCallback {
 public:
  explicit DataResubmissionCallbackImpl(content::WebContents* contents)
      : contents_(contents) {}

  ~DataResubmissionCallbackImpl() override {}

  void Continue() override {
    if (contents_) {
      contents_->GetController().ContinuePendingReload();
    }
  }

  void Cancel() override {
    if (contents_) {
      contents_->GetController().CancelPendingReload();
    }
  }

 private:
  content::WebContents* contents_;

  IMPLEMENT_REFCOUNTING(DataResubmissionCallbackImpl);
};
#endif  // ARKWEB_NETWORK_BASE

ArkWebBrowserContentsDelegateExt::ArkWebBrowserContentsDelegateExt(
    scoped_refptr<CefBrowserInfo> browser_info)
    : CefBrowserContentsDelegate(browser_info) {}

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
void ArkWebBrowserContentsDelegateExt::OnRefreshAccessedHistory(
    CefRefPtr<CefFrame> frame,
    const GURL& url,
    bool isReload) {
  CefRefPtr<CefClient> cefClient = client();
  if (!cefClient.get()) {
    LOG(ERROR) << "cef client is null";
    return;
  }

  auto handler = cefClient->GetLoadHandler();
  if (!handler.get()) {
    LOG(ERROR) << "cef client handler is null";
    return;
  }

  handler->OnRefreshAccessedHistory(browser(), frame, url.spec(), isReload);
}
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)

#if BUILDFLAG(ARKWEB_WPT)
void ArkWebBrowserContentsDelegateExt::DidStartNavigation(
    content::NavigationHandle* navigation) {
  if (icon_helper_) {
    icon_helper_->ClearFailedFaviconUrlSets(navigation);

#if BUILDFLAG(ARKWEB_FAVICON)
    if (navigation->IsInMainFrame() && !navigation->IsSameDocument()) {
      icon_helper_->SetMainFrameDocumentOnLoadCompleted(false);
    }
#endif  // BUILDFLAG(ARKWEB_FAVICON)

#if BUILDFLAG(ARKWEB_EXT_UA)
    // |final_ua| may be added to the navigation of the mainframe and iframe.
    MaybeSetUserAgentOverrideForMainFrame(navigation);
#endif  // BUILDFLAG(ARKWEB_EXT_UA)
  }
}
#endif  // BUILDFLAG(ARKWEB_WPT)

#if BUILDFLAG(ARKWEB_FAVICON)
void ArkWebBrowserContentsDelegateExt::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (icon_helper_) {
    icon_helper_->SetMainFrameDocumentOnLoadCompleted(true);
  }
}
#endif  // BUILDFLAG(ARKWEB_FAVICON)

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
void ArkWebBrowserContentsDelegateExt::OnActivateContent() {
  LOG(INFO) << "ArkWebBrowserContentsDelegateExt::ActivateContent";
  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      handler->OnActivateContent();
    }
  }
}
#endif

#if BUILDFLAG(ARKWEB_NAVIGATION)
void ArkWebBrowserContentsDelegateExt::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!browser_info_) {
    return;
  }

  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      CefLoadCommittedDetails::NavigationType type =
          CefLoadCommittedDetailsImpl::ConvertToCefLoadCommittedDetailsType(
              load_details);
      CefRefPtr<CefLoadCommittedDetails> details =
          new CefLoadCommittedDetailsImpl(
              load_details.current_commit_entry_url.spec(), type,
              load_details.is_main_frame, load_details.is_same_document,
              load_details.did_replace_entry);
      handler->OnNavigationEntryCommitted(details);
    }
  }
}
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
void ArkWebBrowserContentsDelegateExt::ViewportFitChanged(
    blink::mojom::ViewportFit value) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnViewportFitChange(browser(), static_cast<int>(value));
    }
  }
}
#endif
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void ArkWebBrowserContentsDelegateExt::OnOldPageNoLongerRendered(
    const GURL& url,
    bool success) {
  LOG(INFO) << "ArkWebBrowserContentsDelegateExt::OldPageNoLongerRendered";

#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
  LOG_FEEDBACK(INFO)
      << "ArkWebBrowserContentsDelegateExt::OldPageNoLongerRendered";
#endif

  if (!browser_info_) {
    return;
  }
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnPageVisible(browser(), url.spec(), success);
    }
  }
}
#endif

#if BUILDFLAG(ARKWEB_FAVICON)
void ArkWebBrowserContentsDelegateExt::InitIconHelper() {
  if (icon_helper_) {
    return;
  }
  icon_helper_ = new IconHelper();
  icon_helper_->SetBrowser(browser());
  if (client()) {
    if (CefRefPtr<ArkWebDisplayHandlerExt> handler =
            client()->GetDisplayHandler()) {
      icon_helper_->SetDisplayHandler(handler);
    }
  }
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
void ArkWebBrowserContentsDelegateExt::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  LOG(INFO) << "ArkWebBrowserContentsDelegateExt::ShowRepostFormWarningDialog";
  if (!source) {
    return;
  }
  CefRefPtr<DataResubmissionCallbackImpl> callbackImpl =
      new DataResubmissionCallbackImpl(source);
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnDataResubmission(browser(), callbackImpl.get());
    }
  }
}
#endif  // ARKWEB_NETWORK_BASE

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
void ArkWebBrowserContentsDelegateExt::WebExtensionUpdateTab(
    int32_t tab_id,
    const NWebExtensionTabUpdateProperties* update_properties) {
  if (auto c = client()) {
    if (auto handler = c->GetWebExtensionApiHandler()) {
      handler->OnUpdateTab(tab_id, update_properties);
    }
  }
}
#endif
