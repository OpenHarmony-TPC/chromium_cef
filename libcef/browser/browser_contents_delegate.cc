// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_contents_delegate.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/native/cursor_util.h"
#include "libcef/common/frame_util.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-test-utils.h"

#if BUILDFLAG(IS_OHOS)
#include "libcef/common/request_impl.h"
#include "net/http/http_request_headers.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

#ifdef OHOS_EX_UA
#include "base/command_line.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/common/content_switches.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_ua_config.h"
#endif

#if defined(OHOS_NAVIGATION)
#include "content/public/browser/navigation_details.h"
#include "libcef/browser/load_committed_details_impl.h"
#endif  // defined(OHOS_NAVIGATION)

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "libcef/browser/extensions/api/tabs/tabs_windows_api.h"
#endif

using content::KeyboardEventProcessingResult;

namespace {
#if BUILDFLAG(IS_OHOS)
// The amount of time to disallow repeated pointer lock calls after the user
// successfully escapes from one lock request.
constexpr base::TimeDelta kEffectiveUserEscapeDuration =
    base::Milliseconds(1250);
#endif

#ifdef OHOS_EX_UA
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
  if (!navigation->HasUserGesture()) {
    const GURL referrer_url = navigation->GetReferrer().url;
    if (referrer_url.is_valid() && !referrer_url.is_empty() &&
        referrer_url.has_host()) {
      std::string referrer_sld =
          net::registry_controlled_domains::GetDomainAndRegistry(
              referrer_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
      std::string sld = net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
      if (referrer_sld == sld) {
        host = referrer_url.host();
      }
    }
  }
  std::string final_ua =
      nweb_ex::AlloyBrowserUAConfig::GetInstance()->GetUserAgentForHost(host);
  LOG(DEBUG) << "DidStartNavigation, host " << host << ", final_ua " << final_ua
             << ", user_gesture " << navigation->HasUserGesture()
             << ", is_main_frame " << navigation->IsInMainFrame()
             << ", is_reload " << is_reload;

  navigation->SetRequestHeader(net::HttpRequestHeaders::kUserAgent, final_ua);
  if (!navigation->IsInMainFrame()) {
    return;
  }

  content::WebContents* web_contents = navigation->GetWebContents();
  if (!web_contents) {
    return;
  }
  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(final_ua), true);
}
#endif  // OHOS_EX_UA

#if defined(OHOS_NAVIGATION)
CefLoadCommittedDetails::NavigationType ConvertToCefLoadCommittedDetailsType(
    const content::LoadCommittedDetails& details) {
  CefLoadCommittedDetails::NavigationType type;
  switch (details.type) {
    case content::OhosNavigationType::OHOS_NAVIGATION_TYPE_UNKNOWN:
      type = CefLoadCommittedDetails::NavigationType::NAVIGATION_TYPE_UNKNOWN;
      break;
    case content::OhosNavigationType::OHOS_NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY:
      type = CefLoadCommittedDetails::NavigationType::
          NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY;
      break;
    case content::OhosNavigationType::
        OHOS_NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY:
      type = CefLoadCommittedDetails::NavigationType::
          NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY;
      break;
    case content::OhosNavigationType::OHOS_NAVIGATION_TYPE_NEW_SUBFRAME:
      type =
          CefLoadCommittedDetails::NavigationType::NAVIGATION_TYPE_NEW_SUBFRAME;
      break;
    case content::OhosNavigationType::OHOS_NAVIGATION_TYPE_AUTO_SUBFRAME:
      type = CefLoadCommittedDetails::NavigationType::
          NAVIGATION_TYPE_AUTO_SUBFRAME;
      break;
    default:
      type = CefLoadCommittedDetails::NavigationType::NAVIGATION_TYPE_UNKNOWN;
      break;
  }
  return type;
}
#endif  // defined(OHOS_NAVIGATION)

class CefWidgetHostInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting,
      public content::RenderWidgetHostObserver {
 public:
  CefWidgetHostInterceptor(CefRefPtr<CefBrowser> browser,
                           content::RenderWidgetHost* render_widget_host)
      : browser_(browser),
        render_widget_host_(render_widget_host),
        impl_(static_cast<content::RenderWidgetHostImpl*>(render_widget_host)
                  ->widget_host_receiver_for_testing()
                  .SwapImplForTesting(this)) {
    render_widget_host_->AddObserver(this);
  }

  CefWidgetHostInterceptor(const CefWidgetHostInterceptor&) = delete;
  CefWidgetHostInterceptor& operator=(const CefWidgetHostInterceptor&) = delete;

  blink::mojom::WidgetHost* GetForwardingInterface() override { return impl_; }

  // WidgetHostInterceptorForTesting method:
  void SetCursor(const ui::Cursor& cursor) override {
    if (cursor_util::OnCursorChange(browser_, cursor)) {
      // Don't change the cursor.
      return;
    }

    GetForwardingInterface()->SetCursor(cursor);
  }

  // RenderWidgetHostObserver method:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override {
    widget_host->RemoveObserver(this);
    delete this;
  }

 private:
  CefRefPtr<CefBrowser> const browser_;
  content::RenderWidgetHost* const render_widget_host_;
  blink::mojom::WidgetHost* const impl_;
};

}  // namespace

CefBrowserContentsDelegate::CefBrowserContentsDelegate(
    scoped_refptr<CefBrowserInfo> browser_info)
    : browser_info_(browser_info) {
  DCHECK(browser_info_->browser());
}

void CefBrowserContentsDelegate::ObserveWebContents(
    content::WebContents* new_contents) {
  WebContentsObserver::Observe(new_contents);

#if BUILDFLAG(IS_OHOS)
  icon_helper_->SetWebContents(new_contents);
#endif

  if (new_contents) {
    registrar_.reset(new content::NotificationRegistrar);

    // When navigating through the history, the restored NavigationEntry's title
    // will be used. If the entry ends up having the same title after we return
    // to it, as will usually be the case, the
    // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since
    // the NavigationEntry's title hasn't changed.
    registrar_->Add(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<content::NavigationController>(
                        &new_contents->GetController()));

    // Make sure MaybeCreateFrame is called at least one time.
    // Create the frame representation before OnAfterCreated is called for a new
    // browser.
    browser_info_->MaybeCreateFrame(new_contents->GetPrimaryMainFrame(),
                                    false /* is_guest_view */);

    // Make sure RenderWidgetCreated is called at least one time. This Observer
    // is registered too late to catch the initial creation.
    RenderWidgetCreated(new_contents->GetRenderViewHost()->GetWidget());
  } else {
    registrar_.reset();
  }
}

void CefBrowserContentsDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CefBrowserContentsDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// |source| may be NULL for navigations in the current tab, or if the
// navigation originates from a guest view via MaybeAllowNavigation.
content::WebContents* CefBrowserContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  bool cancel = false;

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      // May return nullptr for omnibox navigations.
      auto frame = browser()->GetFrame(params.frame_tree_node_id);
      if (!frame) {
        frame = browser()->GetMainFrame();
      }
      cancel = handler->OnOpenURLFromTab(
          browser(), frame, params.url.spec(),
          static_cast<cef_window_open_disposition_t>(params.disposition),
          params.user_gesture);
    }
  }

  // Returning nullptr will cancel the navigation.
  return cancel ? nullptr : web_contents();
}

void CefBrowserContentsDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool should_show_loading_ui) {
  const int current_index =
      source->GetController().GetLastCommittedEntryIndex();
  const int max_index = source->GetController().GetEntryCount() - 1;

  const bool is_loading = source->IsLoading();
  const bool can_go_back = (current_index > 0);
  const bool can_go_forward = (current_index < max_index);

  // This method may be called multiple times in a row with |is_loading|
  // true as a result of https://crrev.com/5e750ad0. Ignore the 2nd+ times.
  if (is_loading_ == is_loading
#if !BUILDFLAG(IS_OHOS)
      && can_go_back_ == can_go_back && can_go_forward_ == can_go_forward
#endif
  ) {
    return;
  }

  is_loading_ = is_loading;

#if !BUILDFLAG(IS_OHOS)
  can_go_back_ = can_go_back;
  can_go_forward_ = can_go_forward;
#endif

  OnStateChanged(State::kNavigation);

  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnLoadingStateChange(browser(), is_loading, can_go_back,
                                    can_go_forward);
    }
  }

#if defined(OHOS_ARKWEB_EXTENSIONS)
  int32_t tab_id = browser()->GetTabId();
  if (tab_id >= 0) {
    std::vector<std::string> changed_properties;
    changed_properties.push_back(extensions::tabs_constants::kStatusKey);
    std::unique_ptr<NWebExtensionTabChangeInfo> info = std::make_unique<NWebExtensionTabChangeInfo>();
    info->url = "";
    extensions::cef::TabsWindowsAPI::Get(
        source->GetBrowserContext())->TabUpdated(
            tab_id, source, changed_properties, std::move(info));
  }
#endif
}

void CefBrowserContentsDelegate::UpdateTargetURL(content::WebContents* source,
                                                 const GURL& url) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnStatusMessage(browser(), url.spec());
    }
  }
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
void CefBrowserContentsDelegate::WebExtensionUpdateTab(
    int32_t tab_id,
    const NWebExtensionTabUpdateProperties* update_properties) {
  if (auto c = client()) {
    if (auto handler = c->GetWebExtensionApiHandler()) {
      handler->OnUpdateTab(tab_id, update_properties);
    }
  }
}
#endif

bool CefBrowserContentsDelegate::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      // Use LOGSEVERITY_DEBUG for unrecognized |level| values.
      cef_log_severity_t cef_level = LOGSEVERITY_DEBUG;
      switch (log_level) {
        case blink::mojom::ConsoleMessageLevel::kVerbose:
          cef_level = LOGSEVERITY_DEBUG;
          break;
        case blink::mojom::ConsoleMessageLevel::kInfo:
          cef_level = LOGSEVERITY_INFO;
          break;
        case blink::mojom::ConsoleMessageLevel::kWarning:
          cef_level = LOGSEVERITY_WARNING;
          break;
        case blink::mojom::ConsoleMessageLevel::kError:
          cef_level = LOGSEVERITY_ERROR;
          break;
      }

      return handler->OnConsoleMessage(browser(), cef_level, message, source_id,
                                       line_no);
    }
  }

  return false;
}

void CefBrowserContentsDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
#if defined(OHOS_MEDIA)
  if (options.video_natural_size.has_value()) {
    OnFullscreenModeChange(/*fullscreen=*/true,
                           CefSize(options.video_natural_size->width(),
                                   options.video_natural_size->height()));
  } else {
    OnFullscreenModeChange(/*fullscreen=*/true, CefSize(0, 0));
  }
#else
  OnFullscreenModeChange(/*fullscreen=*/true);
#endif  // defined(OHOS_MEDIA)
}

void CefBrowserContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  OnFullscreenModeChange(/*fullscreen=*/false
#if defined(OHOS_MEDIA)
                         ,
                         CefSize(0, 0)
#endif  // defined(OHOS_MEDIA)
  );
}

void CefBrowserContentsDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  bool allow = true;

  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetDownloadHandler()) {
        allow = handler->CanDownload(browser(), url.spec(), request_method);
      }
    }
  }

  std::move(callback).Run(allow);
}

#if BUILDFLAG(IS_OHOS)
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

void CefBrowserContentsDelegate::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  LOG(INFO) << "CefBrowserContentsDelegate::ShowRepostFormWarningDialog";
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

bool CefBrowserContentsDelegate::HandleUserKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code != ui::VKEY_ESCAPE) {
    return false;
  }

  if (IsMouseLocked()) {
    if (tab_with_exclusive_access_) {
      UnlockMouse();
      SetTabWithExclusiveAccess(nullptr);
      mouse_lock_state_ = MOUSELOCK_UNLOCKED;
    }
    last_user_escape_time_ = base::TimeTicks::Now();
    return true;
  }

  return false;
}
#endif

KeyboardEventProcessingResult
CefBrowserContentsDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
#if BUILDFLAG(IS_OHOS)
  // Forward keyboard events to the manager for fullscreen / mouse lock. This
  // may consume the event (e.g., Esc exits fullscreen mode).
  if (HandleUserKeyEvent(event)) {
    return KeyboardEventProcessingResult::HANDLED;
  }
#endif

  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetKeyboardHandler()) {
        CefKeyEvent cef_event;
        if (browser_util::GetCefKeyEvent(event, cef_event)) {
          cef_event.focus_on_editable_field = focus_on_editable_field_;

          auto event_handle = delegate->GetEventHandle(event);
          bool is_keyboard_shortcut = false;
          bool result = handler->OnPreKeyEvent(
              browser(), cef_event, event_handle, &is_keyboard_shortcut);
          if (result) {
            return KeyboardEventProcessingResult::HANDLED;
          } else if (is_keyboard_shortcut) {
            return KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
          }
        }
      }
    }
  }

  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool CefBrowserContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser) {
    return false;
  }

  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetKeyboardHandler()) {
        CefKeyEvent cef_event;
        if (browser_util::GetCefKeyEvent(event, cef_event)) {
          cef_event.focus_on_editable_field = focus_on_editable_field_;

          auto event_handle = delegate->GetEventHandle(event);
          if (handler->OnKeyEvent(browser(), cef_event, event_handle)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

#if BUILDFLAG(IS_OHOS)
bool CefBrowserContentsDelegate::IsMouseLocked() const {
  return mouse_lock_state_ == MOUSELOCK_LOCKED ||
         mouse_lock_state_ == MOUSELOCK_LOCKED_SILENTLY;
}

bool CefBrowserContentsDelegate::IsMouseLockedSilently() const {
  return mouse_lock_state_ == MOUSELOCK_LOCKED_SILENTLY;
}

void CefBrowserContentsDelegate::SetTabWithExclusiveAccess(
    content::WebContents* tab) {
  // Tab should never be replaced with another tab, or
  // UpdateNotificationRegistrations would need updating.
  DCHECK(tab_with_exclusive_access_ == tab ||
         tab_with_exclusive_access_ == nullptr || tab == nullptr);
  tab_with_exclusive_access_ = tab;

  if (tab_with_exclusive_access_ && registrar_->IsEmpty()) {
    registrar_->Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    content::Source<content::NavigationController>(
                        &tab_with_exclusive_access_->GetController()));
  } else if (!tab_with_exclusive_access_ && !registrar_->IsEmpty()) {
    registrar_->RemoveAll();
  }
}

void CefBrowserContentsDelegate::RequestToLockMouse(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  DCHECK(!IsMouseLocked());
  if (!last_unlocked_by_target && !is_fullscreen()) {
    if (!user_gesture) {
      web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      return;
    }
    if (base::TimeTicks::Now() <
        last_user_escape_time_ + kEffectiveUserEscapeDuration) {
      web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kUserRejected);
      return;
    }
  }
  SetTabWithExclusiveAccess(web_contents);

  // Lock mouse.
  if (web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kSuccess)) {
    if (last_unlocked_by_target) {
      mouse_lock_state_ = MOUSELOCK_LOCKED_SILENTLY;
    } else {
      mouse_lock_state_ = MOUSELOCK_LOCKED;
    }
  } else {
    SetTabWithExclusiveAccess(nullptr);
    mouse_lock_state_ = MOUSELOCK_UNLOCKED;
  }
}

void CefBrowserContentsDelegate::LostMouseLock() {
  mouse_lock_state_ = MOUSELOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);
}

void CefBrowserContentsDelegate::UnlockMouse() {
  if (!tab_with_exclusive_access_) {
    return;
  }

  content::RenderWidgetHostView* mouse_lock_view = nullptr;
  content::RenderViewHost* rvh =
      tab_with_exclusive_access_->GetPrimaryMainFrame()->GetRenderViewHost();
  if (rvh) {
    mouse_lock_view = rvh->GetWidget()->GetView();
  }

  if (mouse_lock_view) {
    mouse_lock_view->UnlockMouse();
  }
}
#endif

void CefBrowserContentsDelegate::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->MaybeCreateFrame(render_frame_host, false /* is_guest_view */);
  if (render_frame_host->GetParent() == nullptr) {
    auto render_view_host = render_frame_host->GetRenderViewHost();
    auto base_background_color = platform_delegate()->GetBackgroundColor();
    if (browser_info_ && browser_info_->is_popup()) {
      // force reset page base background color because popup window won't get
      // the page base background from web_contents at the creation time
      web_contents()->SetPageBaseBackgroundColor(SkColor());
      web_contents()->SetPageBaseBackgroundColor(base_background_color);
#if defined(OHOS_MULTI_WINDOW)
      web_contents()->OnWebPreferencesChanged();
#endif
    }
    if (render_view_host->GetWidget() &&
        render_view_host->GetWidget()->GetView()) {
      render_view_host->GetWidget()->GetView()->SetBackgroundColor(
          base_background_color);
    }

    platform_delegate()->RenderViewCreated(render_view_host);
  }
}

void CefBrowserContentsDelegate::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Just in case RenderFrameCreated wasn't called for some reason.
  RenderFrameCreated(new_host);
}

void CefBrowserContentsDelegate::RenderFrameHostStateChanged(
    content::RenderFrameHost* host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  browser_info_->FrameHostStateChanged(host, old_state, new_state);
}

void CefBrowserContentsDelegate::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const auto frame_id =
      frame_util::MakeFrameId(render_frame_host->GetGlobalId());
  browser_info_->RemoveFrame(render_frame_host);

  if (focused_frame_ && focused_frame_->GetIdentifier() == frame_id) {
    focused_frame_ = nullptr;
    OnStateChanged(State::kFocusedFrame);
  }
}

void CefBrowserContentsDelegate::RenderWidgetCreated(
    content::RenderWidgetHost* render_widget_host) {
  new CefWidgetHostInterceptor(browser(), render_widget_host);
}

void CefBrowserContentsDelegate::RenderViewReady() {
  platform_delegate()->RenderViewReady();

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnRenderViewReady(browser());
    }
  }
}

void CefBrowserContentsDelegate::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    ts = TS_PROCESS_WAS_KILLED;
  } else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
    ts = TS_PROCESS_CRASHED;
  } else if (status == base::TERMINATION_STATUS_OOM) {
    ts = TS_PROCESS_OOM;
#ifdef OHOS_RENDER_PROCESS_MODE
  } else if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    if (browser() && browser()->GetHost()) {
      browser()->GetHost()->NotifyNeedsReload(true);
    }
#endif
  } else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    return;
  }

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnRenderProcessTerminated(browser(), ts);
    }
  }
}

void CefBrowserContentsDelegate::OnFrameFocused(
    content::RenderFrameHost* render_frame_host) {
  CefRefPtr<CefFrameHostImpl> frame = static_cast<CefFrameHostImpl*>(
      browser_info_->GetFrameForHost(render_frame_host).get());
  if (!frame || frame->IsFocused()) {
    return;
  }

  CefRefPtr<CefFrameHostImpl> previous_frame = focused_frame_;
  if (frame->IsMain()) {
    focused_frame_ = nullptr;
  } else {
    focused_frame_ = frame;
  }

  if (!previous_frame) {
    // The main frame is focused by default.
    previous_frame = browser_info_->GetMainFrame();
  }

  if (previous_frame->GetIdentifier() != frame->GetIdentifier()) {
    previous_frame->SetFocused(false);
    frame->SetFocused(true);
  }

  OnStateChanged(State::kFocusedFrame);
}

void CefBrowserContentsDelegate::PrimaryMainDocumentElementAvailable() {
  has_document_ = true;
  OnStateChanged(State::kDocument);

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnDocumentAvailableInMainFrame(browser());
    }
  }
}

void CefBrowserContentsDelegate::LoadProgressChanged(double progress) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnLoadingProgressChange(browser(), progress);
    }
  }
}

void CefBrowserContentsDelegate::DidStopLoading() {
  // Notify all renderers that loading has stopped. We used to use
  // RenderFrameObserver::DidStopLoading in the renderer process but that was
  // removed in https://crrev.com/3e37dd0ead. However, that callback wasn't
  // necessarily accurate because it wasn't called in all of the cases where
  // RenderFrameImpl sends the FrameHostMsg_DidStopLoading message. This adds
  // an additional round trip but should provide the same or improved
  // functionality.
  for (const auto& frame : browser_info_->GetAllFrames()) {
    frame->MaybeSendDidStopLoading();
  }
}

#if defined(OHOS_WPT)
void CefBrowserContentsDelegate::DidStartNavigation(
    content::NavigationHandle* navigation) {
  if (icon_helper_) {
    icon_helper_->ClearFailedFaviconUrlSets(navigation);

#if defined(OHOS_FAVICON)
    if (navigation->IsInMainFrame() && !navigation->IsSameDocument()) {
      icon_helper_->SetMainFrameDocumentOnLoadCompleted(false);
    }
#endif  // defined(OHOS_FAVICON)

#ifdef OHOS_EX_UA
    // |final_ua| may be added to the navigation of the mainframe and iframe.
    MaybeSetUserAgentOverrideForMainFrame(navigation);
#endif  // OHOS_EX_UA
  }
}
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
void CefBrowserContentsDelegate::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (icon_helper_) {
    icon_helper_->SetMainFrameDocumentOnLoadCompleted(true);
  }
}
#endif  // defined(OHOS_FAVICON)

#if defined(OHOS_NAVIGATION)
void CefBrowserContentsDelegate::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!browser_info_) {
    return;
  }

  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      CefLoadCommittedDetails::NavigationType type =
          ConvertToCefLoadCommittedDetailsType(load_details);
      CefRefPtr<CefLoadCommittedDetails> details =
          new CefLoadCommittedDetailsImpl(
              load_details.current_commit_entry_url.spec(), type,
              load_details.is_main_frame, load_details.is_same_document,
              load_details.did_replace_entry);
      handler->OnNavigationEntryCommitted(details);
    }
  }
}
#endif  // defined(OHOS_NAVIGATION)

void CefBrowserContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const net::Error error_code = navigation_handle->GetNetErrorCode();

  // Skip calls where the navigation has not yet committed and there is no
  // error code. For example, when creating a browser without loading a URL.
  if (!navigation_handle->HasCommitted() && error_code == net::OK) {
    return;
  }

  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    // A primary main frame navigation has occured.
    has_document_ = false;
    OnStateChanged(State::kDocument);
  }

#if defined(OHOS_FAVICON)
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      !navigation_handle->IsErrorPage() && icon_helper_) {
    icon_helper_->SetLastPageUrl(navigation_handle->GetURL());
  }
#endif  // defined(OHOS_FAVICON)

  const bool is_main_frame = navigation_handle->IsInMainFrame();
  const auto global_id = frame_util::GetGlobalId(navigation_handle);
  const GURL& url =
      (error_code == net::OK ? navigation_handle->GetURL() : GURL());

  auto browser_info = browser_info_;
  if (!browser_info->browser()) {
    // Ignore notifications when the browser is closing.
    return;
  }

  // May return NULL when starting a new navigation if the previous navigation
  // caused the renderer process to crash during load.
  CefRefPtr<CefFrameHostImpl> frame =
      browser_info->GetFrameForGlobalId(global_id);
  if (!frame) {
    if (is_main_frame) {
      frame = browser_info->GetMainFrame();
    } else {
      frame = browser_info->CreateTempSubFrame(frame_util::InvalidGlobalId());
    }
  }
  frame->RefreshAttributes();

  if (error_code == net::OK) {
    // The navigation has been committed and there is no error.
    DCHECK(navigation_handle->HasCommitted());

    // Don't call OnLoadStart for same page navigations (fragments,
    // history state).
    if (!navigation_handle->IsSameDocument()) {
      OnLoadStart(frame.get(), navigation_handle->GetPageTransition());
      if (navigation_handle->IsServedFromBackForwardCache()) {
        // We won't get an OnLoadEnd notification from anywhere else.
#ifdef OHOS_BFCACHE
        LOG(INFO) << "[Favicon] page load form bfcache.";
        if (auto c = client()) {
          if (auto handler = c->GetLoadHandler()) {
            auto navigation_lock = browser_info_->CreateNavigationLock();
            handler->UpdateFavicon(browser());
          }
        }
#endif // OHOS_BFCACHE
        OnLoadEnd(frame.get(), navigation_handle->GetURL(), 0);
      }
    }

#if BUILDFLAG(IS_OHOS)
    if (!navigation_handle->IsSameDocument()) {
      content::RenderFrameHost* render_frame_host =
          navigation_handle->GetRenderFrameHost();
      if (render_frame_host) {
        auto invokeVisualStateCallback = base::BindOnce(
            [](base::WeakPtr<CefBrowserContentsDelegate> self, const GURL url,
               bool success) {
              LOG(INFO) << "invokeVisualStateCallback success: " << success;
              if (!self) {
                return;
              }
              self->OnOldPageNoLongerRendered(url, success);
            },
            weak_factory_.GetWeakPtr(), url);
        render_frame_host->InsertVisualStateCallback(
            std::move(invokeVisualStateCallback));
      }
    }
#endif

    if (is_main_frame) {
      OnAddressChange(url);
    }
#if BUILDFLAG(IS_OHOS)
    bool isReload =
        PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                 ui::PageTransition::PAGE_TRANSITION_RELOAD);
    LOG(INFO) << "load type = "
              << PageTransitionStripQualifier(
                     navigation_handle->GetPageTransition());
    OnRefreshAccessedHistory(frame.get(), url, isReload);
#endif
  } else {
    // The navigation failed with an error. This may happen before commit
    // (e.g. network error) or after commit (e.g. response filter error).
    // If the error happened before commit then this call will originate from
    // RenderFrameHostImpl::OnDidFailProvisionalLoadWithError.
    // OnLoadStart/OnLoadEnd will not be called.
#if BUILDFLAG(IS_OHOS)
#if OHOS_NETWORK_LOAD
    // See also InterceptedRequestHandlerWrapper.OnRequestError
#else
    CefRefPtr<CefRequestImpl> request = new CefRequestImpl();
    CefString cef_url(navigation_handle->GetURL().spec());
    CefString cef_method(navigation_handle->IsPost() ? "POST" : "GET");
    request->SetURL(cef_url);
    request->SetMethod(cef_method);
#ifdef OHOS_NETWORK_LOAD
    request->Set(navigation_handle->GetRequestHeaders());
#endif
    OnLoadError(request, navigation_handle->IsInMainFrame(),
                navigation_handle->HasUserGesture(), error_code);
#endif
#else
    OnLoadError(frame.get(), navigation_handle->GetURL(), error_code);
#endif
  }
}

void CefBrowserContentsDelegate::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  // The navigation failed after commit. OnLoadStart was called so we also
  // call OnLoadEnd.
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  frame->RefreshAttributes();
  OnLoadError(frame, validated_url, error_code);
  OnLoadEnd(frame, validated_url, error_code);
}

void CefBrowserContentsDelegate::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  frame->RefreshAttributes();

  int http_status_code = 0;
  if (auto response_headers = render_frame_host->GetLastResponseHeaders()) {
    http_status_code = response_headers->response_code();
  }

  OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserContentsDelegate::TitleWasSet(content::NavigationEntry* entry) {
  // |entry| may be NULL if a popup is created via window.open and never
  // navigated.
  if (entry) {
    OnTitleChange(entry->GetTitle());
  } else if (web_contents()) {
    OnTitleChange(web_contents()->GetTitle());
  }
}

void CefBrowserContentsDelegate::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      std::vector<CefString> icon_urls;
      for (const auto& icon : candidates) {
        if (icon->icon_type == blink::mojom::FaviconIconType::kFavicon) {
          icon_urls.push_back(icon->icon_url.spec());
        }
      }
      if (!icon_urls.empty()) {
        handler->OnFaviconURLChange(browser(), icon_urls);
      }
    }
  }
#if BUILDFLAG(IS_OHOS)
  icon_helper_->OnUpdateFaviconURL(render_frame_host, candidates);
#endif
}

void CefBrowserContentsDelegate::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      handler->OnGotFocus(browser());
    }
  }
}

void CefBrowserContentsDelegate::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  focus_on_editable_field_ =
      details->focus_type != blink::mojom::blink::FocusType::kNone &&
      details->is_editable_node;
}

void CefBrowserContentsDelegate::WebContentsDestroyed() {
  auto wc = web_contents();
  ObserveWebContents(nullptr);
  for (auto& observer : observers_) {
    observer.OnWebContentsDestroyed(wc);
  }
}

void CefBrowserContentsDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_STOP);

  if (type == content::NOTIFICATION_LOAD_STOP) {
    OnTitleChange(web_contents()->GetTitle());
  }
}

bool CefBrowserContentsDelegate::OnSetFocus(cef_focus_source_t source) {
  // SetFocus() might be called while inside the OnSetFocus() callback. If
  // so, don't re-enter the callback.
  if (is_in_onsetfocus_) {
    return true;
  }

  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      is_in_onsetfocus_ = true;
      bool handled = handler->OnSetFocus(browser(), source);
      is_in_onsetfocus_ = false;

      return handled;
    }
  }

  return false;
}

#if defined(OHOS_MULTI_WINDOW)
void CefBrowserContentsDelegate::OnActivateContent() {
  LOG(INFO) << "CefBrowserContentsDelegate::ActivateContent";
  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
       handler->OnActivateContent();
    }
  }
}
#endif

CefRefPtr<CefClient> CefBrowserContentsDelegate::client() const {
  if (auto b = browser()) {
    return b->GetHost()->GetClient();
  }
  return nullptr;
}

CefRefPtr<CefBrowser> CefBrowserContentsDelegate::browser() const {
  return browser_info_->browser();
}

CefBrowserPlatformDelegate* CefBrowserContentsDelegate::platform_delegate()
    const {
  auto browser = browser_info_->browser();
  if (browser) {
    return browser->platform_delegate();
  }
  return nullptr;
}

void CefBrowserContentsDelegate::OnAddressChange(const GURL& url) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      // On the handler of an address change.
      handler->OnAddressChange(browser(), browser_info_->GetMainFrame(),
                               url.spec());
    }
  }
}

void CefBrowserContentsDelegate::OnLoadStart(
    CefRefPtr<CefFrame> frame,
    ui::PageTransition transition_type) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has started.
      handler->OnLoadStart(browser(), frame, frame->GetURL(),
                           static_cast<cef_transition_type_t>(transition_type));
    }
  }
}

void CefBrowserContentsDelegate::OnLoadEnd(CefRefPtr<CefFrame> frame,
                                           const GURL& url,
                                           int http_status_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnLoadEnd(browser(), frame, http_status_code);
    }
  }
}

void CefBrowserContentsDelegate::OnLoadError(CefRefPtr<CefFrame> frame,
                                             const GURL& url,
                                             int error_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has failed.
      handler->OnLoadError(browser(), frame,
                           static_cast<cef_errorcode_t>(error_code),
                           net::ErrorToShortString(error_code), url.spec());
    }
  }
}

void CefBrowserContentsDelegate::OnTitleChange(const std::u16string& title) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnTitleChange(browser(), title);
    }
  }
}

void CefBrowserContentsDelegate::OnFullscreenModeChange(
    bool fullscreen
#if defined(OHOS_MEDIA)
    ,
    const CefSize& video_natural_size
#endif  // defined(OHOS_MEDIA)
) {
  if (fullscreen == is_fullscreen_) {
    return;
  }

  is_fullscreen_ = fullscreen;
  OnStateChanged(State::kFullscreen);

  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnFullscreenModeChange(browser(), fullscreen
#if defined(OHOS_MEDIA)
                                      ,
                                      video_natural_size
#endif  // defined(OHOS_MEDIA)
      );
    }
  }
}

void CefBrowserContentsDelegate::OnStateChanged(State state_changed) {
  for (auto& observer : observers_) {
    observer.OnStateChanged(state_changed);
  }
}

#if BUILDFLAG(IS_OHOS)
void CefBrowserContentsDelegate::OnLoadError(CefRefPtr<CefRequest> request,
                                             bool is_main_frame,
                                             bool has_user_gesture,
                                             int error_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has failed.
      handler->OnLoadErrorWithRequest(request, is_main_frame, has_user_gesture,
                                      error_code,
                                      net::ErrorToShortString(error_code));
    }
  }
}

void CefBrowserContentsDelegate::OnOldPageNoLongerRendered(const GURL& url,
                                                           bool success) {
  LOG(INFO) << "CefBrowserContentsDelegate::OldPageNoLongerRendered";

#ifdef OHOS_LOGGER_REPORT
  LOG_FEEDBACK(INFO) << "CefBrowserContentsDelegate::OldPageNoLongerRendered";
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

void CefBrowserContentsDelegate::InitIconHelper() {
  icon_helper_ = new IconHelper();
  icon_helper_->SetBrowser(CefBrowserContentsDelegate::browser());
  CefRefPtr<CefClient> client = CefBrowserContentsDelegate::client();
  if (client) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler) {
      icon_helper_->SetDisplayHandler(handler);
    }
  }
}

void CefBrowserContentsDelegate::OnRefreshAccessedHistory(
    CefRefPtr<CefFrame> frame,
    const GURL& url,
    bool isReload) {
  CefRefPtr<CefClient> cefClient = client();
  if (!cefClient.get()) {
    LOG(ERROR) << "cef client is null";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "cef client is null";
#endif

    return;
  }

  auto handler = cefClient->GetLoadHandler();
  if (!handler.get()) {
    LOG(ERROR) << "cef client handler is null";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "cef client handler is null";
#endif

    return;
  }

  handler->OnRefreshAccessedHistory(browser(), frame, url.spec(), isReload);
}
#endif

#if defined(OHOS_DISPLAY_CUTOUT)
void CefBrowserContentsDelegate::ViewportFitChanged(
    blink::mojom::ViewportFit value) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnViewportFitChange(browser(), static_cast<int>(value));
    }
  }
}
#endif
