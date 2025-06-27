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
#include "content/browser/web_contents/web_contents_impl.h"
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

#if BUILDFLAG(ARKWEB_USERAGENT)
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_ua_config.h"
#endif
#include "cef/ohos_cef_ext/libcef/browser/alloy/alloy_browser_ua_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#endif

#if BUILDFLAG(ARKWEB_NAVIGATION)
#include "content/public/browser/navigation_details.h"
#include "ohos_cef_ext/libcef/browser/load_committed_details_impl.h"
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "include/cef_web_extension_api_handler.h"
#endif

namespace {
  
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
// The amount of time to disallow repeated pointer lock calls after the user
// successfully escapes from one lock request.
const int64_t kEffectiveUserEscapeDuration = 1250;
#endif

#if BUILDFLAG(ARKWEB_USERAGENT)
bool IsIllegalUrl(const GURL& url) {
  return url.is_empty() || !url.is_valid() || !url.has_host();
}

std::string GetDomainAndRegistry(const GURL& url) {
  auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return base::ToLowerASCII(domain);
}

void UpdateUserAgentForNavigation(content::NavigationHandle* navigation,
                                  const std::string& user_agent) {
  navigation->SetRequestHeader(net::HttpRequestHeaders::kUserAgent, user_agent);
  if (!navigation->IsInMainFrame()) {
    return;
  }

  if (auto* web_contents = navigation->GetWebContents()) {
    web_contents->SetUserAgentOverride(
        blink::UserAgentOverride::UserAgentOnly(user_agent), true);
  }
}

UserAgentOverridePolicy MatchUserAgent(content::NavigationHandle* navigation,
                                       const std::string& host,
                                       std::string& user_agent) {
  if (auto* web_contents = navigation->GetWebContents()) {
    std::string custom_ua = web_contents->GetCustomUA();
    if (!custom_ua.empty()) {
      user_agent = custom_ua;
      return UserAgentOverridePolicy::CUSTOM;
    }
  }
  std::string user_agent_no_nweb_ex = user_agent;
  std::string user_agent_nweb_ex = user_agent;
  auto match_type_no_nweb_ex =
      AlloyBrowserUAConfig::GetInstance()->MatchUserAgent(
          host, user_agent_no_nweb_ex);
  user_agent = user_agent_no_nweb_ex;
  return match_type_no_nweb_ex;
}

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
  if (IsIllegalUrl(referrer_url)) {
    return std::string();
  }

  if (GetDomainAndRegistry(referrer_url) ==
      GetDomainAndRegistry(navigation->GetURL())) {
    return referrer_url.host();
  }
  return std::string();
}

void MaybeSetUserAgentOverrideForMainFrame(
    content::NavigationHandle* navigation) {
  if (!navigation) {
    return;
  }

  // UserAgent won't be added when the navigation_request created by renderer
  // process.
  if (content::NavigationRequest::From(navigation)
          ->is_synchronous_renderer_commit()) {
    return;
  }

  const GURL& url = navigation->GetURL();
  if (IsIllegalUrl(url)) {
    return;
  }

  bool is_reload = false;

  std::string final_ua;
  std::string host = url.host();

  auto match_type = MatchUserAgent(navigation, url.host(), final_ua);
  if (match_type >= UserAgentOverridePolicy::APP_DEFAULT) {
    is_reload =
        PageTransitionCoreTypeIs(navigation->GetPageTransition(),
                                 ui::PageTransition::PAGE_TRANSITION_RELOAD);
    std::string referer_host =
        GetHostOfReferrerUrlIfNeed(navigation, is_reload);
    if (!referer_host.empty()) {
      host = referer_host;
      MatchUserAgent(navigation, host, final_ua);
    }
  }

  LOG(DEBUG) << __func__ << " host " << host << ", final_ua " << final_ua
             << ", user_gesture " << navigation->HasUserGesture()
             << ", main_frame " << navigation->IsInMainFrame() << ", reload "
             << is_reload << ", serverd_from_bfcache "
             << navigation->IsServedFromBackForwardCache() << ", match_type "
             << match_type;

  UpdateUserAgentForNavigation(navigation, final_ua);
}
#endif  // BUILDFLAG(ARKWEB_USERAGENT)
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
  raw_ptr<content::WebContents> contents_;

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
#endif
  }

#if BUILDFLAG(ARKWEB_USERAGENT)
  // |final_ua| may be added to the navigation of the mainframe and iframe.
  MaybeSetUserAgentOverrideForMainFrame(navigation);
#endif
}
#endif  // BUILDFLAG(ARKWEB_WPT)

#if BUILDFLAG(ARKWEB_USERAGENT)
void ArkWebBrowserContentsDelegateExt::DidRedirectNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation) {
    return;
  }

  const auto& redirect_chain = navigation->GetRedirectChain();
  constexpr int kLeastLengthForRedirectChain = 2;
  if (redirect_chain.size() < kLeastLengthForRedirectChain) {
    return;
  }

  const auto& current_url = navigation->GetURL();
  const auto& prev_redirect_url = redirect_chain[redirect_chain.size() - 2];
  if (IsIllegalUrl(current_url) || IsIllegalUrl(prev_redirect_url)) {
    return;
  }

  std::string final_ua;
  auto match_type = MatchUserAgent(navigation, current_url.host(), final_ua);
  if (match_type >=
      UserAgentOverridePolicy::APP_DEFAULT) {
    if (GetDomainAndRegistry(current_url) ==
        GetDomainAndRegistry(prev_redirect_url)) {
      match_type = MatchUserAgent(navigation, prev_redirect_url.host(), final_ua);
    }
  }

  LOG(DEBUG) << __func__ << " host " << current_url.host() << ", final_ua "
             << final_ua << ", user_gesture " << navigation->HasUserGesture()
             << ", main_frame " << navigation->IsInMainFrame()
             << ", serverd_from_bfcache "
             << navigation->IsServedFromBackForwardCache();
  if (match_type == UserAgentOverridePolicy::ARKWEB_DEFAULT) {
    return;
  }
  UpdateUserAgentForNavigation(navigation, final_ua);
}
#endif  // BUILDFLAG(ARKWEB_EXT_UA)

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

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
bool ArkWebBrowserContentsDelegateExt::IsPointerLocked() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED ||
         pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

bool ArkWebBrowserContentsDelegateExt::IsPointerLockedSilently() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

void ArkWebBrowserContentsDelegateExt::SetTabWithExclusiveAccess(content::WebContents* tab) {
  // Tab should never be replaced with another tab, or
  // UpdateNotificationRegistrations would need updating.
  DCHECK(tab_with_exclusive_access_ == tab ||
         tab_with_exclusive_access_ == nullptr || tab == nullptr);
  tab_with_exclusive_access_ = tab;
}

bool ArkWebBrowserContentsDelegateExt::HandleUserKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code != ui::VKEY_ESCAPE) {
    return false;
  }
 
  if (IsPointerLocked()) {
    if (tab_with_exclusive_access_) {
      UnlockPointer();
      SetTabWithExclusiveAccess(nullptr);
      pointer_lock_state_ = POINTERLOCK_UNLOCKED;
    }
    last_user_escape_time_ = base::TimeTicks::Now();
    return true;
  }
 
  return false;
}

void ArkWebBrowserContentsDelegateExt::RequestPointerLock(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  DCHECK(!IsPointerLocked());
  if (!last_unlocked_by_target && !is_fullscreen()) {
    if (!user_gesture) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      return;
    }

    base::TimeDelta userEscapeDuration = base::TimeTicks::Now() - last_user_escape_time_;
    if (userEscapeDuration.InMilliseconds() < kEffectiveUserEscapeDuration) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kUserRejected);
      return;
    }
  }
  SetTabWithExclusiveAccess(web_contents);

  // Lock pointer.
  if (web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kSuccess)) {
    if (last_unlocked_by_target) {
      pointer_lock_state_ = POINTERLOCK_LOCKED_SILENTLY;
    } else {
      pointer_lock_state_ = POINTERLOCK_LOCKED;
    }
  } else {
    SetTabWithExclusiveAccess(nullptr);
    pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  }
}

void ArkWebBrowserContentsDelegateExt::LostPointerLock() {
  pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);
}

void ArkWebBrowserContentsDelegateExt::UnlockPointer() {
  if (!tab_with_exclusive_access_) {
    return;
  }

  content::RenderWidgetHostView* pointer_lock_view = nullptr;
  content::RenderViewHost* rvh =
      tab_with_exclusive_access_->GetPrimaryMainFrame()->GetRenderViewHost();
  if (rvh) {
    pointer_lock_view = rvh->GetWidget()->GetView();
  }

  if (pointer_lock_view) {
    pointer_lock_view->UnlockPointer();
  }
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
void ArkWebBrowserContentsDelegateExt::OnLoadStarted(CefRefPtr<CefFrame> frame,
                                                     const CefString& url) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has started.
      handler->OnLoadStarted(frame, url);
    }
  }
}

void ArkWebBrowserContentsDelegateExt::OnLoadFinished(CefRefPtr<CefFrame> frame,
                                                      const CefString& url) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnLoadFinished(frame, url);
    }
  }
}

void ArkWebBrowserContentsDelegateExt::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  bool is_popup_window = false;
  bool has_accessed_initial_document = false;
  if (auto browser = browser_info_->browser()) {
    is_popup_window = browser->IsPopup();
    if (browser->GetWebContents()) {
      content::WebContentsImpl* web_contents_impl =
          static_cast<content::WebContentsImpl*>(browser->GetWebContents());
      if (web_contents_impl) {
        has_accessed_initial_document =
            web_contents_impl->HasAccessedInitialDocument();
      }
    }
  }

  bool should_synthesize_page_load =
      is_popup_window && has_accessed_initial_document &&
      (changed_flags == content::InvalidateTypes::INVALIDATE_TYPE_URL) &&
      !did_synthesize_page_load_;
  if (should_synthesize_page_load) {
    auto main_frame = browser_info_->GetMainFrame();
    OnLoadStarted(main_frame.get(), main_frame->GetURL());
    OnLoadFinished(main_frame.get(), main_frame->GetURL());
    did_synthesize_page_load_ = true;
  }
}
#endif
