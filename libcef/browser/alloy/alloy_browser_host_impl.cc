// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "cef/libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "cef/libcef/browser/audio_capturer.h"
#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/browser_guest_util.h"
#include "cef/libcef/browser/browser_info.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/hang_monitor.h"
#include "cef/libcef/browser/media_access_query.h"
#include "cef/libcef/browser/osr/osr_util.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/common/drag_data_impl.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/url_util.h"
#include "cef/libcef/common/request_impl.h"
#include "cef/libcef/common/values_impl.h"
#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#endif

#include "arkweb/build/features/features.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/zoom/page_zoom.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "libcef/browser/alloy/render_process_state_handler.h"
#include "net/base/net_errors.h"
#include "ui/events/base_event_utils.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_CLIPBOARD)
#include "libcef/browser/osr/touch_selection_controller_client_osr.h"
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
#include "content/public/common/content_switches.h"
#include "include/arkweb_client_ext.h"
#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#endif  // BUILDFLAG(ARKWEB_VIDEO_LTPO)

#if BUILDFLAG(ARKWEB_SAME_LAYER)
#include "arkweb/chromium_ext/gpu/ipc/common/gpu_surface_id_tracker.h"
#include "cef/ohos_cef_ext/include/arkweb_render_handler_ext.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_GET_ZOOM_LEVEL)
#include "base/command_line.h"
#include "components/prefs/pref_service.h"
#include "components/zoom/page_zoom.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#endif
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
#include "arkweb/ohos_nweb/src/nweb_inputmethod_handler.h"
#include "cef/ohos_cef_ext/libcef/browser/alloy/cef_extension_keybinding_registry_views.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "libcef/browser/browser_event_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/types/event_type.h"
#endif
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
#include "base/ohos/ltpo/include/sliding_observer.h"
#endif

#if BUILDFLAG(ARKWEB_BFCACHE)
#include "content/public/common/content_switches.h"
#endif
#if BUILDFLAG(ARKWEB_CSS_INPUT_TIME)
#include "cef/ohos_cef_ext/include/arkweb_dialog_handler_ext.h"
#include "content/browser/ohos/date_time_chooser_ohos.h"
#endif  // ARKWEB_CSS_INPUT_TIME

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
#include "arkweb/chromium_ext/content/public/browser/custom_media_info.h"
#include "arkweb/chromium_ext/content/public/browser/custom_media_player_listener.h"
#include "cef/ohos_cef_ext/include/cef_media_player_listener.h"
#include "cef/ohos_cef_ext/libcef/browser/alloy/custom_media_player_proxy.h"
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
#include "cef/ohos_cef_ext/libcef/browser/ohos_safe_browsing/ohos_safe_browsing_tab_helper.h"
#endif
using content::KeyboardEventProcessingResult;

#if BUILDFLAG(IS_OHOS)
int32_t AlloyBrowserHostImpl::ltpo_strategy_ = -1;
#endif

#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
#include "content/browser/renderer_host/render_widget_host_impl.h"
#endif

#if BUILDFLAG(ARKWEB_PERMISSION)
#include "cef/ohos_cef_ext/libcef/browser/permission/alloy_access_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "ohos_cef_ext/libcef/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#endif

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
#include "content/browser/media/video_assistant/video_assistant.h"
#include "media/mojo/mojom/media_player.mojom.h"
#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_engine_cloud_config.h"
#include "ohos_nweb_ex/overrides/cef/libcef/browser/video_assistant/video_assistant.h"
#endif  // ARKWEB_NWEB_EX
#endif  // ARKWEB_VIDEO_ASSISTANT

namespace {

static constexpr base::TimeDelta kRecentlyAudibleTimeout = base::Seconds(2);

#if BUILDFLAG(ARKWEB_CSS_INPUT_TIME)
class CefDateTimeChooserCallbackImpl : public CefDateTimeChooserCallback {
 public:
  explicit CefDateTimeChooserCallbackImpl(
      content::DateTimeChooserOHOS* date_time_chooser)
      : date_time_chooser_(date_time_chooser) {}

  void Continue(bool success, double value) override {
    if (date_time_chooser_) {
      date_time_chooser_->NotifyResult(success, value);
    }
  }

 private:
  content::DateTimeChooserOHOS* date_time_chooser_ = nullptr;
  IMPLEMENT_REFCOUNTING(CefDateTimeChooserCallbackImpl);
};
#endif

// List of WebUI hosts that have been tested to work in Alloy-style browsers.
// Do not add new hosts to this list without also manually testing all related
// functionality in CEF.
const char* kAllowedWebUIHosts[] = {
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
    chrome::kChromeUIExtensionsHost,
#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
    chrome::kChromeUINetExportHost,
#endif
    chrome::kChromeUIWebUITestHost,
    chrome::kChromeUIVersionHost,
#else
    chrome::kChromeUIAccessibilityHost,
    content::kChromeUIBlobInternalsHost,
    chrome::kChromeUIChromeURLsHost,
    chrome::kChromeUICreditsHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    chrome::kChromeUILicenseHost,
    content::kChromeUIMediaInternalsHost,
    chrome::kChromeUINetExportHost,
    chrome::kChromeUINetInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    chrome::kChromeUIPrintHost,
    content::kChromeUIProcessInternalsHost,
    content::kChromeUIResourcesHost,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    chrome::kChromeUISandboxHost,
#endif
    content::kChromeUIServiceWorkerInternalsHost,
    chrome::kChromeUISystemInfoHost,
    chrome::kChromeUITermsHost,
    chrome::kChromeUIThemeHost,
    content::kChromeUITracingHost,
    chrome::kChromeUIVersionHost,
    content::kChromeUIWebRTCInternalsHost,
#endif
};

bool IsAllowedWebUIHost(const std::string_view& host) {
  for (auto& allowed_host : kAllowedWebUIHosts) {
    if (base::EqualsCaseInsensitiveASCII(allowed_host, host)) {
      return true;
    }
  }
  return false;
}

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
class CefMediaPlayerListenerImpl : public CefMediaPlayerListener {
 public:
  explicit CefMediaPlayerListenerImpl(
      std::unique_ptr<content::CustomMediaPlayerListener> listener)
      : listener_(std::move(listener)) {}
  void OnStatusChanged(uint32_t status) override {
    if (listener_) {
      listener_->OnStatusChanged(status);
    }
  }
  void OnVolumeChanged(double volume) override {
    if (listener_) {
      listener_->OnVolumeChanged(volume);
    }
  }
  void OnMutedChanged(bool muted) override {
    if (listener_) {
      listener_->OnMutedChanged(muted);
    }
  }
  void OnPlaybackRateChanged(double playback_rate) override {
    if (listener_) {
      listener_->OnPlaybackRateChanged(playback_rate);
    }
  }
  void OnDurationChanged(double duration) override {
    if (listener_) {
      listener_->OnDurationChanged(duration);
    }
  }
  void OnTimeUpdate(double current_time) override {
    if (listener_) {
      listener_->OnTimeUpdate(current_time);
    }
  }
  void OnBufferedEndTimeChanged(double buffered_time) override {
    if (listener_) {
      listener_->OnBufferedEndTimeChanged(buffered_time);
    }
  }
  void OnEnded() override {
    if (listener_) {
      listener_->OnEnded();
    }
  }
  void OnNetworkStateChanged(uint32_t state) override {
    if (listener_) {
      listener_->OnNetworkStateChanged(state);
    }
  }
  void OnReadyStateChanged(uint32_t state) override {
    if (listener_) {
      listener_->OnReadyStateChanged(state);
    }
  }
  void OnFullscreenChanged(bool fullscreen) override {
    if (listener_) {
      listener_->OnFullscreenChanged(fullscreen);
    }
  }
  void OnSeeking() override {
    if (listener_) {
      listener_->OnSeeking();
    }
  }
  void OnSeekFinished() override {
    if (listener_) {
      listener_->OnSeekFinished();
    }
  }
  void OnError(uint32_t error_code, const CefString& error_msg) override {
    if (listener_) {
      listener_->OnError(error_code, error_msg);
    }
  }
  void OnVideoSizeChanged(int width, int height) override {
    if (listener_) {
      listener_->OnVideoSizeChanged(width, height);
    }
  }

 private:
  IMPLEMENT_REFCOUNTING(CefMediaPlayerListenerImpl);
  std::unique_ptr<content::CustomMediaPlayerListener> listener_;
};
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

}  // namespace

// AlloyBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::Create(
    CefBrowserCreateParams& create_params) {
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate =
      CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  // Expect runtime style to match.
  CHECK(platform_delegate->IsAlloyStyle());

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
          /*is_devtools_popup=*/false, platform_delegate->IsWindowless(),
          platform_delegate->IsPrintPreviewSupported(),
          create_params.extra_info);

  bool own_web_contents = false;

  // This call may modify |create_params|.
  auto web_contents =
      platform_delegate->CreateWebContents(create_params, own_web_contents);

  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(create_params.request_context.get());

  CefRefPtr<AlloyBrowserHostImpl> browser =
      CreateInternal(create_params.settings, create_params.client, web_contents,
                     own_web_contents, info,
                     /*opener=*/nullptr, /*is_devtools_popup=*/false,
                     request_context_impl, std::move(platform_delegate));
  if (!browser) {
    return nullptr;
  }

  GURL url = url_util::MakeGURL(create_params.url, /*fixup=*/true);

  if (!url.is_empty()) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  CefFrameHostImpl::kPageTransitionExplicit,
                                  /*is_renderer_initiated=*/false);
    browser->LoadMainFrameURL(params);
  }

  return browser.get();
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::CreateInternal(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    bool own_web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<AlloyBrowserHostImpl> opener,
    bool is_devtools_popup,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);
  LOG(INFO) << "AlloyBrowserHostImpl::CreateInternal, begin";

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (opener) {
    if (!opener->platform_delegate_) {
      // The opener window is being destroyed. Cancel the popup.
      if (own_web_contents) {
        delete web_contents;
      }
      return nullptr;
    }

    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings, client, web_contents, platform_delegate.get(),
        is_devtools_popup);
  }

  // Take ownership of |web_contents| if |own_web_contents| is true.
  platform_delegate->WebContentsCreated(web_contents, own_web_contents);

  CefRefPtr<AlloyBrowserHostImpl> browser = new AlloyBrowserHostImpl(
      settings, client, web_contents, browser_info, opener, request_context,
      std::move(platform_delegate));
  browser->InitializeBrowser();

  if (!browser->CreateHostWindow()) {
    return nullptr;
  }

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  if (opener && opener->platform_delegate_) {
    // 1. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    // Do this first for consistency with Chrome style.
    opener->platform_delegate_->PopupBrowserCreated(
        browser->platform_delegate(), browser.get(), is_devtools_popup);
  }

  // 2. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser. Block navigation to avoid issues with focus
  // changes being sent to an unbound interface.
  {
    auto navigation_lock = browser_info->CreateNavigationLock();
    browser->OnAfterCreated();
  }

  // 3. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  browser->platform_delegate_->NotifyBrowserCreated();

  return browser;
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::FromBaseChecked(
    CefRefPtr<CefBrowserHostBase> host_base) {
  if (!host_base) {
    return nullptr;
  }
  CHECK(host_base->IsAlloyStyle());
  return host_base.get()->AsAlloyBrowserHostImpl();
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForContents(contents));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForGlobalId(global_id));
}

// AlloyBrowserHostImpl methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::~AlloyBrowserHostImpl() = default;

void AlloyBrowserHostImpl::CloseBrowser(bool force_close) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Exit early if a close attempt is already pending and this method is
    // called again from somewhere other than WindowDestroyed().
    if (destruction_state_ >= DESTRUCTION_STATE_PENDING &&
        (IsWindowless() || !window_destroyed_)) {
      if (force_close && destruction_state_ == DESTRUCTION_STATE_PENDING) {
        // Upgrade the destruction state.
        destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
      }
      return;
    }

    if (destruction_state_ < DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = (force_close ? DESTRUCTION_STATE_ACCEPTED
                                        : DESTRUCTION_STATE_PENDING);
    }

    content::WebContents* contents = web_contents();
    if (contents && contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
      // Will result in a call to BeforeUnloadFired() and, if the close isn't
      // canceled, CloseContents().
      contents->DispatchBeforeUnload(false /* auto_cancel */);
#if BUILDFLAG(ARKWEB_CLOSE_STEPS)
      // In cef_life_span_handler.h file show DoClose step.
      // Step 1 to Step 3 is over.
      // This will replace Step 4 : User approves the close. Beause both in
      // Android and OH close will not be blocked by beforeunload event.
      CloseContents(contents);
#endif
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::CloseBrowser,
                                          this, force_close));
  }
}

bool AlloyBrowserHostImpl::TryCloseBrowser() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  // Protect against multiple requests to close while the close is pending.
  if (destruction_state_ <= DESTRUCTION_STATE_PENDING) {
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      CloseBrowser(false);
    }

    // Cancel the close.
    return false;
  }

  // Allow the close.
  return true;
}

CefWindowHandle AlloyBrowserHostImpl::GetWindowHandle() {
  if (is_views_hosted_ && CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_) {
      return platform_delegate_->GetHostWindowHandle();
    }
  }
  return host_window_handle_;
}

CefWindowHandle AlloyBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_window_handle_;
}

void AlloyBrowserHostImpl::Find(const CefString& searchText,
                                bool forward,
                                bool matchCase,
                                bool findNext) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::Find, this, searchText,
                                 forward, matchCase, findNext));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Find(searchText, forward, matchCase, findNext);
  }
}

void AlloyBrowserHostImpl::StopFinding(bool clearSelection) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::StopFinding,
                                          this, clearSelection));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->StopFinding(clearSelection);
  }
}

#if BUILDFLAG(ARKWEB_DEVTOOLS)
void AlloyBrowserHostImpl::ShowDevToolsWith(
    CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
    CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
    const CefPoint& inspect_element_at) {
  LOG(INFO) << "ShowDevToolsWith";
  CEF_REQUIRE_UIT();
  if (!EnsureDevToolsProtocolManager()) {
    return;
  }
  devtools_protocol_manager_->ShowDevToolsWith(
      frontend_browser, delegate, inspect_element_at);
}
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)

#if BUILDFLAG(IS_ARKWEB)
void AlloyBrowserHostImpl::GetRootBrowserAccessibilityManager(void** manager) {
  LOG(INFO)
      << "dsf AlloyBrowserHostImpl::GetRootBrowserAccessibilityManager start";
  if (!platform_delegate_) {
    return;
  }
  LOG(INFO)
      << "dsf AlloyBrowserHostImpl::GetRootBrowserAccessibilityManager middle";
  *manager = static_cast<void*>(
      platform_delegate_->GetRootBrowserAccessibilityManager());
  LOG(INFO) << "wulonghui "
               "AlloyBrowserHostImpl::GetRootBrowserAccessibilityManager end";
}
#endif

void AlloyBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                                const CefSize& min_size,
                                                const CefSize& max_size) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetAutoResizeEnabled,
                                 this, enabled, min_size, max_size));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAutoResizeEnabled(enabled, min_size, max_size);
  }
}

bool AlloyBrowserHostImpl::CanExecuteChromeCommand(int command_id) {
  return false;
}

void AlloyBrowserHostImpl::ExecuteChromeCommand(
    int command_id,
    cef_window_open_disposition_t disposition) {
  NOTIMPLEMENTED();
}

bool AlloyBrowserHostImpl::IsWindowRenderingDisabled() {
  return IsWindowless();
}

void AlloyBrowserHostImpl::WasResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::WasResized, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->WasResized();
  }
}

void AlloyBrowserHostImpl::WasHidden(bool hidden) {
#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
  LOG(DEBUG) << "web was hidden:" << hidden;
#endif
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }
#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
  is_hidden_ = hidden;
  ReportWindowStatus(false);
#endif
  if (platform_delegate_) {
    platform_delegate_->WasHidden(hidden);
  }
}

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
void AlloyBrowserHostImpl::WasOccluded(bool occluded) {
  LOG(DEBUG) << "web was occluded:" << occluded;
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::WasOccluded,
                                          this, occluded));
    return;
  }

  is_hidden_ = occluded;
  ReportWindowStatus(false);

  if (platform_delegate_) {
    platform_delegate_->WasOccluded(occluded);
  }
}

void AlloyBrowserHostImpl::SetEnableLowerFrameRate(bool enabled) {
  LOG(DEBUG) << "SetEnableLowerFrameRate:" << enabled;
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetEnableLowerFrameRate,
                                 this, enabled));
    return;
  }

  auto rvh = web_contents()->GetRenderViewHost();
  if (rvh && rvh->GetWidget()) {
    ArkWebRenderWidgetHostViewOSRExt* view =
        static_cast<ArkWebRenderWidgetHostViewOSRExt*>(
            rvh->GetWidget()->GetView());
    if (view) {
      view->SetEnableLowerFrameRate(enabled);
    }
  }
}

void AlloyBrowserHostImpl::ReportWindowStatus(bool first_view_ready) {
  using namespace OHOS::NWeb;
  if (first_view_ready && is_hidden_) {
    LOG(INFO)
        << "no need to report render view ready because the view is hidden";
    return;
  }

  content::WebContents* contents = web_contents();
  if (!contents) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::ReportWindowStatus web_contents is null";
    return;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::ReportWindowStatus "
                    "render_process_host is null";
      return;
    }

    ResSchedStatusAdapter status = is_hidden_
                                       ? ResSchedStatusAdapter::WEB_INACTIVE
                                       : ResSchedStatusAdapter::WEB_ACTIVE;
    base::ProcessId process_id = render_process_host->GetProcess().Pid();
    InactiveUnloadOldProcess(process_id);
    ResSchedClientAdapter::ReportWindowStatus(status, process_id, window_id_,
                                              nweb_id_);
    if (!is_hidden_) {
      ResSchedClientAdapter::ReportScene(ResSchedStatusAdapter::WEB_SCENE_ENTER,
                                         ResSchedSceneAdapter::VISIBLE,
                                         nweb_id_);
    }
  } else {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::ReportWindowStatus render_view_host is null";
    return;
  }
}
#endif

void AlloyBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->NotifyScreenInfoChanged();
  }
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
  base::ohos::SlidingObserver::GetInstance().OnDisplayInfoChange();
#endif
}

void AlloyBrowserHostImpl::Invalidate(PaintElementType type) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Invalidate(type);
  }
}

void AlloyBrowserHostImpl::SendExternalBeginFrame() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendExternalBeginFrame, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendExternalBeginFrame();
  }
}

void AlloyBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SendTouchEvent,
                                          this, event));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendTouchEvent(event);
  }

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  if (event.type == CEF_TET_PRESSED) {
    has_touch_event_ = true;
    if (set_lower_frame_rate_) {
      ResetVSyncFrequency();
    }
  }

  if (event.type == CEF_TET_RELEASED) {
    if (has_video_playing_ && !set_lower_frame_rate_) {
      CEF_POST_DELAYED_TASK(
          CEF_UIT,
          base::BindOnce(&AlloyBrowserHostImpl::UpdateVSyncFrequency, this),
          WAIT_TOUCH_EVENT_DELAY_TIME);
    }
  }
#endif
}

void AlloyBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendCaptureLostEvent();
  }
}

int AlloyBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return 0;
  }

  return osr_util::ClampFrameRate(settings_.windowless_frame_rate);
}

void AlloyBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetWindowlessFrameRate,
                                 this, frame_rate));
    return;
  }

  settings_.windowless_frame_rate = frame_rate;

  if (platform_delegate_) {
    platform_delegate_->SetWindowlessFrameRate(frame_rate);
  }
}

// AlloyBrowserHostImpl public methods.
// -----------------------------------------------------------------------------

bool AlloyBrowserHostImpl::IsWindowless() const {
  return is_windowless_;
}

bool AlloyBrowserHostImpl::IsVisible() const {
  CEF_REQUIRE_UIT();
  if (IsWindowless() && platform_delegate_) {
    return !platform_delegate_->IsHidden();
  }
  return CefBrowserHostBase::IsVisible();
}

bool AlloyBrowserHostImpl::IsPictureInPictureSupported() const {
  // Not currently supported with OSR.
  return !IsWindowless();
}

void AlloyBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
  DCHECK(!window_destroyed_);
  window_destroyed_ = true;

  // Destroy objects that may reference the window.
  menu_manager_.reset(nullptr);

  CloseBrowser(true);
}

bool AlloyBrowserHostImpl::WillBeDestroyed() const {
  CEF_REQUIRE_UIT();
  return destruction_state_ >= DESTRUCTION_STATE_ACCEPTED;
}

void AlloyBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  destruction_state_ = DESTRUCTION_STATE_COMPLETED;

  // Destroy any platform constructs first.
  if (javascript_dialog_manager_.get()) {
    javascript_dialog_manager_->Destroy();
  }
  if (menu_manager_.get()) {
    menu_manager_->Destroy();
  }

#if BUILDFLAG(ARKWEB_EXT_GET_ZOOM_LEVEL)
  // If the WebContents still exists at this point, signal destruction before
  // browser destruction.
  if (web_contents()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableNwebExGetZoomLevel)) {
      auto* browser_context = CefBrowserContext::FromBrowserContext(
          web_contents()->GetBrowserContext());
      if (browser_context) {
        DCHECK(browser_context->AsProfile()->GetPrefs());
        browser_context->AsProfile()->GetPrefs()->CommitPendingWrite();
      }
    }
  }
#endif

  // Disassociate the platform delegate from this browser. This will trigger
  // WebContents destruction in most cases.
  platform_delegate_->BrowserDestroyed(this);

  // Delete objects created by the platform delegate that may be referenced by
  // the WebContents.
  file_dialog_manager_.reset(nullptr);
  javascript_dialog_manager_.reset(nullptr);
  menu_manager_.reset(nullptr);

  // Delete the audio capturer
  if (recently_audible_timer_) {
    recently_audible_timer_->Stop();
    recently_audible_timer_.reset();
  }
  audio_capturer_.reset(nullptr);

  CefBrowserHostBase::DestroyBrowser();
}

void AlloyBrowserHostImpl::CancelContextMenu() {
  CEF_REQUIRE_UIT();
  if (menu_manager_) {
    menu_manager_->CancelContextMenu();
  }
}

bool AlloyBrowserHostImpl::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    const content::OpenURLParams& params) {
  const bool is_guest_view =
      IsBrowserPluginGuest(content::WebContents::FromRenderFrameHost(opener));
  if (is_guest_view && !params.is_pdf &&
      !params.url.SchemeIs(extensions::kExtensionScheme) &&
      !params.url.SchemeIs(content::kChromeUIScheme)
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
      && !params.url.SchemeIs(content::kArkWebUIScheme)
      && !params.url.SchemeIs(extensions::kArkwebExtensionScheme)
#endif
  ) {
    // The PDF viewer will load the PDF extension in the guest view, and print
    // preview will load chrome://print in the guest view. The PDF renderer
    // used with PdfUnseasoned will set |params.is_pdf| when loading the PDF
    // stream (see PdfNavigationThrottle::WillStartRequest). All other guest
    // view navigations are passed to the owner browser.
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      base::IgnoreResult(&AlloyBrowserHostImpl::OpenURLFromTab),
                      this, nullptr, params, base::NullCallback()));

    return false;
  }

  if (!is_guest_view &&
      (params.url.SchemeIs(content::kChromeUIScheme)
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
       || params.url.SchemeIs(content::kArkWebUIScheme)
#endif
           ) &&
      !IsAllowedWebUIHost(params.url.host_piece())) {
    // Block navigation to non-allowlisted WebUI pages.
    LOG(WARNING) << "Navigation to " << params.url.spec()
                 << " is blocked in Alloy-style browser.";
    return false;
  }

  return true;
}

void AlloyBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::OnSetFocus,
                                          this, source));
    return;
  }

  if (contents_delegate_.OnSetFocus(source)) {
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetFocus(true);
  }
}

void AlloyBrowserHostImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  contents_delegate_.EnterFullscreenModeForTab(requesting_frame, options);
  WasResized();
}

void AlloyBrowserHostImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  contents_delegate_.ExitFullscreenModeForTab(web_contents);
  WasResized();
}

bool AlloyBrowserHostImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode AlloyBrowserHostImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
  return is_fullscreen_ ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kBrowser;
}

void AlloyBrowserHostImpl::FindReply(content::WebContents* web_contents,
                                     int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  auto alloy_delegate =
      static_cast<CefBrowserPlatformDelegateAlloy*>(platform_delegate());
  if (alloy_delegate->HandleFindReply(request_id, number_of_matches,
                                      selection_rect, active_match_ordinal,
                                      final_update)) {
    if (client_) {
      if (auto handler = client_->GetFindHandler()) {
        const auto& details = alloy_delegate->last_search_result();
        CefRect rect(details.selection_rect().x(), details.selection_rect().y(),
                     details.selection_rect().width(),
                     details.selection_rect().height());
        handler->OnFindResult(
            this, details.request_id(), details.number_of_matches(), rect,
            details.active_match_ordinal(), details.final_update());
      }
    }
  }
}

void AlloyBrowserHostImpl::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ImeSetComposition, this, text,
                       underlines, replacement_range, selection_range));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeSetComposition(text, underlines, replacement_range,
                                          selection_range);
  }
}

void AlloyBrowserHostImpl::ImeCommitText(const CefString& text,
                                         const CefRange& replacement_range,
                                         int relative_cursor_pos) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::ImeCommitText, this,
                                 text, replacement_range, relative_cursor_pos));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeCommitText(text, replacement_range,
                                      relative_cursor_pos);
  }
}

void AlloyBrowserHostImpl::ImeFinishComposingText(bool keep_selection) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::ImeFinishComposingText,
                                 this, keep_selection));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeFinishComposingText(keep_selection);
  }
}

void AlloyBrowserHostImpl::ImeCancelComposition() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeCancelComposition();
  }
}

void AlloyBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragEnter,
                                 this, drag_data, event, allowed_ops));
    return;
  }

  if (!drag_data.get()) {
    DCHECK(false);
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragEnter(drag_data, event, allowed_ops);
  }
}

void AlloyBrowserHostImpl::DragTargetDragOver(
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragOver, this,
                                event, allowed_ops));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragOver(event, allowed_ops);
  }
}

void AlloyBrowserHostImpl::DragTargetDragLeave() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragLeave();
  }
}

void AlloyBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::DragTargetDrop,
                                          this, event));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDrop(event);
  }
}

void AlloyBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragSourceSystemDragEnded();
  }
}

void AlloyBrowserHostImpl::DragSourceEndedAt(
    int x,
    int y,
    CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragSourceEndedAt, this,
                                 x, y, op));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragSourceEndedAt(x, y, op);
  }
}

void AlloyBrowserHostImpl::SetAudioMuted(bool mute) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetAudioMuted,
                                          this, mute));
    return;
  }
  if (!web_contents()) {
    return;
  }
  web_contents()->SetAudioMuted(mute);
}

bool AlloyBrowserHostImpl::IsAudioMuted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    return false;
  }
  return web_contents()->IsAudioMuted();
}

// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

void AlloyBrowserHostImpl::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client) {
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
  }
}

content::WebContents* AlloyBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto target_contents = contents_delegate_.OpenURLFromTabEx(
      source, params, navigation_handle_callback);
  if (target_contents) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadMainFrameURL(params);
    return target_contents;
  }

  // Cancel the navigation.
  return nullptr;
}

content::WebContents* AlloyBrowserHostImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  auto* new_contents_ptr = new_contents.get();
  platform_delegate_->AddNewContents(source, std::move(new_contents),
                                     target_url, disposition, window_features,
                                     user_gesture, was_blocked);
  return new_contents_ptr;
}

void AlloyBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                               bool should_show_loading_ui) {
  contents_delegate_.LoadingStateChanged(source, should_show_loading_ui);
}

void AlloyBrowserHostImpl::CloseContents(content::WebContents* source) {
  CEF_REQUIRE_UIT();

  if (destruction_state_ == DESTRUCTION_STATE_COMPLETED) {
    return;
  }

  bool close_browser = true;

  // If this method is called in response to something other than
  // WindowDestroyed() ask the user if the browser should close.
  if (client_.get() && (IsWindowless() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
    // |DoClose| will notify the UI to close, |DESTRUCTION_STATE_NONE| means
    // |CloseBrowser| has not been triggered by UI. We should close browser
    // when received |CloseBrowser| request from UI.
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      close_browser = false;
    }
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
    }

    if (!IsWindowless() && !window_destroyed_) {
      // A window exists so try to close it using the platform method. Will
      // result in a call to WindowDestroyed() if/when the window is destroyed
      // via the platform window destruction mechanism.
      platform_delegate_->CloseHostWindow();
    } else {
      // Keep a reference to the browser while it's in the process of being
      // destroyed.
      CefRefPtr<AlloyBrowserHostImpl> browser(this);

      if (source) {
        // Try to fast shutdown the associated process.
        source->GetPrimaryMainFrame()->GetProcess()->FastShutdownIfPossible(
            1, false);
      }

      // No window exists. Destroy the browser immediately. Don't call other
      // browser methods after calling DestroyBrowser().
      DestroyBrowser();
    }
  } else if (destruction_state_ != DESTRUCTION_STATE_NONE) {
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

void AlloyBrowserHostImpl::UpdateTargetURL(content::WebContents* source,
                                           const GURL& url) {
  ReportWindowStatus(false);
  contents_delegate_.UpdateTargetURL(source, url);
}

bool AlloyBrowserHostImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  return contents_delegate_.DidAddMessageToConsole(source, level, message,
                                                   line_no, source_id);
}

void AlloyBrowserHostImpl::ContentsZoomChange(bool zoom_in) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebEx)) {
    content::PageZoom zoomType = zoom_in ? content::PageZoom::PAGE_ZOOM_IN
                                         : content::PageZoom::PAGE_ZOOM_OUT;
    zoom::PageZoom::Zoom(web_contents(), zoomType);
    return;
  }

  double curFactor = GetZoomLevel();
  double tempZoomFactor = zoom_in ? curFactor + 2.0 : curFactor - 2.0;
  if (tempZoomFactor > 10.0 || tempZoomFactor < -10.0) {
    LOG(ERROR) << "The mouse wheel event can no longer be zoomed in or out.";
    return;
  }
  SetBrowserZoomLevel(tempZoomFactor);
#else
  zoom::PageZoom::Zoom(
      web_contents(), zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
#endif
}

void AlloyBrowserHostImpl::BeforeUnloadFired(content::WebContents* source,
                                             bool proceed,
                                             bool* proceed_to_fire_unload) {
  if (destruction_state_ == DESTRUCTION_STATE_ACCEPTED || proceed) {
    *proceed_to_fire_unload = true;
  } else if (!proceed) {
    *proceed_to_fire_unload = false;
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

bool AlloyBrowserHostImpl::TakeFocus(content::WebContents* source,
                                     bool reverse) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get()) {
      handler->OnTakeFocus(this, !reverse);
    }
  }

  return false;
}

void AlloyBrowserHostImpl::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  contents_delegate_.CanDownload(url, request_method, std::move(callback));
}

KeyboardEventProcessingResult AlloyBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return contents_delegate_.PreHandleKeyboardEvent(source, event);
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
bool AlloyBrowserHostImpl::WebHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_if_unhandled) {
    return false;
  }

  if (contents_delegate_.HandleKeyboardEvent(source, event)) {
    return true;
  }

  if (source) {
    content::BrowserContext* browser_context = source->GetBrowserContext();
    extensions::ExtensionCommandsGlobalRegistry* registry =
        extensions::ExtensionCommandsGlobalRegistry::Get(browser_context);
    if (registry) {
      registry->registry_for_active_window();
    }

    bool run_accelerator_flag = true;
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);
    ui::KeyEvent key_event = accelerator.ToKeyEvent();
    const ui::EventType type = key_event.type();
    if (run_accelerator_flag &&
        (type == ui::EventType::kKeyPressed &&
         event.GetType() != blink::WebKeyboardEvent::Type::kChar)) {
      run_accelerator_flag = true;
    } else {
      run_accelerator_flag = false;
    }

    if (run_accelerator_flag) {
      CefExtensionKeybindingRegistryViews cef_key_view(browser_context);
      cef_key_view.AcceleratorPressed(accelerator);
    }
  }

  if (platform_delegate_ && platform_delegate_->HandleKeyboardEvent(event)) {
    return true;
  }

  bool zoom_in;
  if (IsNeedZoomChange(event, zoom_in)) {
    LOG(DEBUG) << "ContentsZoomChange when HandleKeyboardEvent";
    ContentsZoomChange(zoom_in);
    return true;
  }
  return false;
}
#endif

bool AlloyBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  bool isUsed = WebHandleKeyboardEvent(source, event);
  CefRefPtr<CefKeyboardHandler> handler;
  CefKeyEvent cef_event;
  GetCefKeyEvent(event, cef_event);
  if (client_) {
    handler = client_->GetKeyboardHandler();
  }
  if (handler) {
    handler->KeyboardReDispatch(cef_event, isUsed);
  }
  return isUsed;
#else
  // Check to see if event should be ignored.
  if (event.skip_if_unhandled) {
    return false;
  }

  if (contents_delegate_.HandleKeyboardEvent(source, event)) {
    return true;
  }

  if (platform_delegate_) {
    return platform_delegate_->HandleKeyboardEvent(event);
  }
  return false;
#endif
}

bool AlloyBrowserHostImpl::CanDragEnter(content::WebContents* source,
                                        const content::DropData& data,
                                        blink::DragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler;
  if (client_) {
    handler = client_->GetDragHandler();
  }
  if (handler) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(data));
    drag_data->SetReadOnly(true);
    if (handler->OnDragEnter(
            this, drag_data.get(),
            static_cast<CefDragHandler::DragOperationsMask>(mask))) {
      return false;
    }
  }
  return true;
}

void AlloyBrowserHostImpl::GetCustomWebContentsView(
    content::WebContents* web_contents,
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_frame_id,
    raw_ptr<content::WebContentsView>* view,
    raw_ptr<content::RenderViewHostDelegateView>* delegate_view) {
  CefBrowserInfoManager::GetInstance()->GetCustomWebContentsView(
      target_url,
      frame_util::MakeGlobalId(opener_render_process_id,
                               opener_render_frame_id),
      view, delegate_view);
}

void AlloyBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  CefBrowserSettings settings;
  CefRefPtr<CefClient> client;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;
  CefRefPtr<CefDictionaryValue> extra_info;

  CefBrowserInfoManager::GetInstance()->WebContentsCreated(
      target_url,
      frame_util::MakeGlobalId(opener_render_process_id,
                               opener_render_frame_id),
      settings, client, platform_delegate, extra_info, new_contents);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless(),
          platform_delegate->IsPrintPreviewSupported(), extra_info);
  CHECK(info.get());
  CHECK(info->is_popup());

  CefRefPtr<AlloyBrowserHostImpl> opener =
      GetBrowserForContents(source_contents);
  if (!opener) {
    return;
  }

  // Popups must share the same RequestContext as the parent.
  CefRefPtr<CefRequestContextImpl> request_context = opener->request_context();
  CHECK(request_context);

  // We don't officially own |new_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  CefRefPtr<AlloyBrowserHostImpl> browser = CreateInternal(
      settings, client, new_contents, /*own_web_contents=*/false, info, opener,
      /*is_devtools_popup=*/false, request_context,
      std::move(platform_delegate));
}

#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
void AlloyBrowserHostImpl::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter,
    content::RendererIsUnresponsiveReason reason

) {
  content::RenderProcessHost* host =
      source->GetPrimaryMainFrame()->GetProcess();
  if (!host->IsReady() || !source->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    OnDumpJavaScriptStackCallback(host->GetProcess().Pid(), reason, "");
    return;
  }
  host->dumpCurrentJavaScriptStackInMainThread(
      base::BindOnce(&AlloyBrowserHostImpl::OnDumpJavaScriptStackCallback, this,
                     host->GetProcess().Pid(), reason));
}

void AlloyBrowserHostImpl::OnDumpJavaScriptStackCallback(
    int pid,
    content::RendererIsUnresponsiveReason reason,
    const std::string& stack) {
  if (auto handler = client_->GetRequestHandler()) {
    int anr_reason = static_cast<int>(
        reason != content::RendererIsUnresponsiveReason::kOnInputEventAckTimeout
            ? content::RendererIsUnresponsiveReason::
                  kNavigationRequestCommitTimeout
            : content::RendererIsUnresponsiveReason::kOnInputEventAckTimeout);
    handler->AsCefRequestHandlerExt()->OnRenderProcessNotResponding(
        this, stack, pid, anr_reason);
  }
}

void AlloyBrowserHostImpl::RendererResponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  if (auto handler = client_->GetRequestHandler()) {
    handler->AsCefRequestHandlerExt()->OnRenderProcessResponding(this);
  }
}
#else
void AlloyBrowserHostImpl::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  hang_monitor::RendererUnresponsive(this, render_widget_host,
                                     hang_monitor_restarter);
}

void AlloyBrowserHostImpl::RendererResponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  hang_monitor::RendererResponsive(this, render_widget_host);
}
#endif

content::JavaScriptDialogManager*
AlloyBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  return CefBrowserHostBase::GetJavaScriptDialogManager();
}

void AlloyBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // This will eventually call into CefFileDialogManager.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool AlloyBrowserHostImpl::ShowContextMenu(
    const content::ContextMenuParams& params) {
  CEF_REQUIRE_UIT();
  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_ = std::make_unique<CefMenuManager>(
        this, platform_delegate_->CreateMenuRunner());
  }
  return menu_manager_->CreateContextMenu(params);
}

void AlloyBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                               const gfx::Size& pref_size) {
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  CEF_REQUIRE_UIT();
  if (platform_delegate_) {
    platform_delegate_->SizeTo(pref_size.width(), pref_size.height());
  }
#endif
}

void AlloyBrowserHostImpl::ResizeDueToAutoResize(content::WebContents* source,
                                                 const gfx::Size& new_size) {
  CEF_REQUIRE_UIT();

  if (client_) {
    CefRefPtr<ArkWebDisplayHandlerExt> handler = client_->GetDisplayHandler();
    if (handler && handler->OnAutoResize(
                       this, CefSize(new_size.width(), new_size.height()))) {
      return;
    }
  }

  UpdatePreferredSize(source, new_size);
}

void AlloyBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
#if BUILDFLAG(ARKWEB_WEBRTC) && BUILDFLAG(ARKWEB_PERMISSION)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMediaStream)) {
    // Cancel the request.
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  bool microphone_requested =
      (request.audio_type ==
       blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool webcam_requested = (request.video_type ==
                           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  bool screen_requested =
      (request.video_type ==
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) ||
      (request.video_type ==
       blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE);
  LOG(INFO) << "RequestMediaAccessPermission screen_requested: "
            << screen_requested;
  AlloyPermissionRequestHandler* permission_handler =
      GetPermissionRequestHandler();
  if (!permission_handler) {
    // Cancel the request.
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  if (!screen_requested && (microphone_requested || webcam_requested)) {
    permission_handler->SendRequest(
        new AlloyMediaAccessRequest(this, request, std::move(callback)));
  } else {
    permission_handler->SendScreenCaptureRequest(
        new AlloyScreenCaptureAccessRequest(this, request,
                                            std::move(callback)));
  }
#else
  auto returned_callback = media_access_query::RequestMediaAccessPermission(
      this, request, std::move(callback), /*default_disallow=*/true);
  // Callback should not be returned.
  DCHECK(returned_callback.is_null());
#endif  // BUILDFLAG(ARKWEB_WEBRTC) && BUILDFLAG(ARKWEB_PERMISSION)
}

bool AlloyBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return media_access_query::CheckMediaAccessPermission(this, render_frame_host,
                                                        security_origin, type);
}

content::PictureInPictureResult AlloyBrowserHostImpl::EnterPictureInPicture(
    content::WebContents* web_contents) {
  if (!IsPictureInPictureSupported()) {
    return content::PictureInPictureResult::kNotSupported;
  }

  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void AlloyBrowserHostImpl::ExitPictureInPicture() {
  DCHECK(IsPictureInPictureSupported());
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool AlloyBrowserHostImpl::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
#if BUILDFLAG(ARKWEB_BFCACHE)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBFCache)) {
    return true;
  }
#endif
  // Disabled due to issue #3237.
  return false;
}

content::PreloadingEligibility AlloyBrowserHostImpl::IsPrerender2Supported(
    content::WebContents& web_contents) {
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  return content::PreloadingEligibility::kPreloadingUnsupportedByWebContents;
#else
  return content::PreloadingEligibility::kEligible;
#endif
}

void AlloyBrowserHostImpl::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  contents_delegate_.DraggableRegionsChanged(regions, contents);
}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
void AlloyBrowserHostImpl::OnNativeEmbedStatusUpdate(
    const content::NativeEmbedInfo& native_embed_info,
    content::NativeEmbedInfo::TagState state) {
  if (!platform_delegate_) {
    return;
  }

  ArkWebRenderHandlerExt::CefNativeEmbedData data_info;
  data_info.status =
      static_cast<ArkWebRenderHandlerExt::CefEmbedLifeStatus>(state);
  content::GpuProcessHost* host = content::GpuProcessHost::Get();
  data_info.surfaceId =
      host->gpu_host()->GetSurfaceId(native_embed_info.native_embed_id);
  data_info.embedId = std::to_string(native_embed_info.native_embed_id);
  data_info.info.id = native_embed_info.embed_element_id;
  data_info.info.x = native_embed_info.rect.x();
  data_info.info.y = native_embed_info.rect.y();
  data_info.info.width = native_embed_info.rect.width();
  data_info.info.height = native_embed_info.rect.height();
  data_info.info.type = native_embed_info.native_type;
  data_info.info.src = native_embed_info.native_source;
  data_info.info.url = native_embed_info.url.path();
  data_info.info.tag = native_embed_info.tag;
  data_info.info.params = native_embed_info.params;

  platform_delegate_->OnNativeEmbedLifecycleChange(data_info);
}

void AlloyBrowserHostImpl::OnNativeEmbedFirstFramePaint(
    int32_t native_embed_id,
    const std::string& embed_id_attribute) {
  if (!platform_delegate_) {
    return;
  }
  content::GpuProcessHost* host = content::GpuProcessHost::Get();
  content::NativeEmbedFirstPaintEvent event;
  event.embed_id = std::to_string(native_embed_id);
  event.surface_id = host->gpu_host()->GetSurfaceId(native_embed_id);
  event.embed_id_attribute = embed_id_attribute;
  platform_delegate_->OnNativeEmbedFirstFramePaint(event);
}

void AlloyBrowserHostImpl::OnLayerRectVisibilityChange(
    const std::string& embed_id,
    bool visibility) {
  if (!platform_delegate_) {
    return;
  }

  platform_delegate_->OnNativeEmbedVisibilityChange(embed_id, visibility);
}
#endif
// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void AlloyBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (web_contents()) {
#if BUILDFLAG(ARKWEB_INCOGNITO_MODE)
    if (web_contents()->GetBrowserContext() &&
        web_contents()->GetBrowserContext()->IsOffTheRecord()) {
      return;
    }
#endif
    auto cef_browser_context = CefBrowserContext::FromBrowserContext(
        web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(
          navigation_handle->GetURL(), navigation_handle->GetRedirectChain(),
          navigation_handle->GetPageTransition());
    }
  }
}

void AlloyBrowserHostImpl::OnAudioStateChanged(bool audible) {
#if BUILDFLAG(ARKWEB_MEDIA_MUTE_AUDIO)
  LOG(INFO) << "OnAudioStateChanged: " << audible;

  if (client_.get() && client_->AsArkWebClient()->GetMediaHandler().get()) {
    client_->AsArkWebClient()->GetMediaHandler()->OnAudioStateChanged(this,
                                                                      audible);
  }
#endif  // BUILDFLAG(ARKWEB_MEDIA_MUTE_AUDIO)

  if (audible) {
    if (recently_audible_timer_) {
      recently_audible_timer_->Stop();
    }

    StartAudioCapturer();
  } else if (audio_capturer_) {
    if (!recently_audible_timer_) {
      recently_audible_timer_ = std::make_unique<base::OneShotTimer>();
    }

    // If you have a media playing that has a short quiet moment, web_contents
    // will immediately switch to non-audible state. We don't want to stop
    // audio stream so quickly, let's give the stream some time to resume
    // playing.
    recently_audible_timer_->Start(
        FROM_HERE, kRecentlyAudibleTimeout,
        base::BindOnce(&AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired,
                       this));
  }
}

void AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired() {
  audio_capturer_.reset();
}

void AlloyBrowserHostImpl::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityEventReceived(details);
  }
}

void AlloyBrowserHostImpl::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityLocationChangesReceived(tree_id, details);
  }
}

void AlloyBrowserHostImpl::WebContentsDestroyed() {
  // In case we're notified before the CefBrowserContentsDelegate,
  // reset it first for consistent state in DestroyWebContents.
  if (GetWebContents()) {
    contents_delegate_.WebContentsDestroyed();
  }

  auto wc = web_contents();
  content::WebContentsObserver::Observe(nullptr);
  DestroyWebContents(wc);

  if (destruction_state_ < DESTRUCTION_STATE_COMPLETED) {
    // We were not called via DestroyBrowser. This can occur when (for example)
    // a pending popup WebContents is destroyed during parent WebContents
    // destruction. Try to close the associated browser now.
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::CloseBrowser,
                                          this, /*force_close=*/true));
  }
}

#if BUILDFLAG(ARKWEB_FOCUS)
void AlloyBrowserHostImpl::ActivateContents(content::WebContents* contents) {
  LOG(INFO) << "AlloyBrowserHostImpl::ActivateContents";
  OnSetFocus(FOCUS_SOURCE_SYSTEM);
#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
  contents_delegate_.OnActivateContent();
#endif
}
#endif  // BUILDFLAG(ARKWEB_FOCUS)

#if BUILDFLAG(ARKWEB_ACTIVITY_STATE)
void AlloyBrowserHostImpl::OnFormEditingStateChanged(bool state,
                                                     uint64_t form_id) {
  LOG(INFO) << "AlloyBrowserHostImpl::OnFormEditingStateChanged state: "
            << state;
  if (client_.get() && client_->AsArkWebClient()->GetFormHandler().get()) {
    client_->AsArkWebClient()->GetFormHandler()->OnFormEditingStateChanged(
        this, state, form_id);
  }
}

void AlloyBrowserHostImpl::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStartedPlaying, is_video: "
            << video_type.has_video;
#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* host = main_frame->GetProcess();
  if (host && video_type.has_video) {
    LOG(DEBUG) << "AlloyBrowserHostImpl::MediaStartedPlaying, pid: "
               << host->GetProcess().Pid()
               << ", video_stream_cnt: " << video_stream_cnt_;
    if (video_stream_cnt_ == 0) {
      OHOS::NWeb::ResSchedClientAdapter::ReportVideoPlaying(
          OHOS::NWeb::ResSchedStatusAdapter::VIDEO_PLAYING_START,
          host->GetProcess().Pid());
    }
    ++video_stream_cnt_;
  }
#endif
  start_play_ = true;
  cef_media_type_t type =
      video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  if (client_.get() && client_->AsArkWebClient()->GetMediaHandler().get()) {
    client_->AsArkWebClient()->GetMediaHandler()->OnMediaStateChanged(
        this, type, cef_media_playing_state_t::PLAYING);
  }

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  if (type == cef_media_type_t::VIDEO && !set_lower_frame_rate_) {
    has_video_playing_ = true;
    has_touch_event_ = false;
    CEF_POST_DELAYED_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::UpdateVSyncFrequency, this),
        WAIT_TOUCH_EVENT_DELAY_TIME);
  }
#endif
}

void AlloyBrowserHostImpl::MediaStoppedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStoppedPlaying, is_video: "
            << video_type.has_video
            << " stopped reason: " << static_cast<int>(reason);

  if (!start_play_) {
    return;
  }

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* host = main_frame->GetProcess();
  if (host && video_type.has_video) {
    LOG(DEBUG) << "AlloyBrowserHostImpl::MediaStartedPlaying, pid: "
               << host->GetProcess().Pid()
               << ", video_stream_cnt: " << video_stream_cnt_;
    --video_stream_cnt_;
    if (video_stream_cnt_ == 0) {
      OHOS::NWeb::ResSchedClientAdapter::ReportVideoPlaying(
          OHOS::NWeb::ResSchedStatusAdapter::VIDEO_PLAYING_STOP,
          host->GetProcess().Pid());
    }
    video_stream_cnt_ = std::max(video_stream_cnt_, 0);
  }
#endif

  cef_media_type_t type =
      video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  cef_media_playing_state_t state;

  switch (reason) {
    case content::WebContentsObserver::MediaStoppedReason::kReachedEndOfStream:
      state = cef_media_playing_state_t::END_OF_STREAM;
      break;
    case content::WebContentsObserver::MediaStoppedReason::kUnspecified:
      state = cef_media_playing_state_t::PAUSE;
      break;
  }

  if (client_.get() && client_->AsArkWebClient()->GetMediaHandler().get()) {
    client_->AsArkWebClient()->GetMediaHandler()->OnMediaStateChanged(
        this, type, state);
  }

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  if (type == cef_media_type_t::VIDEO) {
    has_video_playing_ = false;
    if (set_lower_frame_rate_) {
      ResetVSyncFrequency();
    }
  }
#endif
}
#endif

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
void AlloyBrowserHostImpl::UpdateVSyncFrequency() {
  if (!base::ohos::IsMobileDevice()) {
    LOG(DEBUG) << " VSync  adjustment is only available for mobile deive";
    return;
  }

  if (!has_video_playing_) {
    LOG(DEBUG) << "UpdateVSyncFrequency Fail due to no video playing";
    return;
  }

  if (has_touch_event_) {
    LOG(DEBUG) << "UpdateVSyncFrequency Fail due to touch event";
    has_touch_event_ = false;
    return;
  }

  auto wc = web_contents();
  if (!wc) {
    LOG(ERROR) << "UpdateVSyncFrequency Fail due to no WebContents";
    return;
  }

  auto rvh = wc->GetRenderViewHost();
  if (rvh && rvh->GetWidget()) {
    ArkWebRenderWidgetHostViewOSRExt* view =
        static_cast<ArkWebRenderWidgetHostViewOSRExt*>(
            rvh->GetWidget()->GetView());
    if (view) {
      LOG(INFO) << "AlloyBrowserHostImpl::UpdateVSyncFrequency";
      view->AsArkWebRenderWidgetHostViewOSRExt()->UpdateVSyncFrequency();
      set_lower_frame_rate_ = true;
    }
  }
}

void AlloyBrowserHostImpl::ResetVSyncFrequency() {
  if (!base::ohos::IsMobileDevice()) {
    LOG(DEBUG) << "VSync adjustment is only available for mobile deive";
    return;
  }

  auto wc = web_contents();
  if (!wc) {
    LOG(ERROR) << "ResetVSyncFrequency Fail due to no WebContents";
    return;
  }

  auto rvh = wc->GetRenderViewHost();
  if (rvh && rvh->GetWidget()) {
    ArkWebRenderWidgetHostViewOSRExt* view =
        static_cast<ArkWebRenderWidgetHostViewOSRExt*>(
            rvh->GetWidget()->GetView());
    if (view) {
      LOG(INFO) << "AlloyBrowserHostImpl::ResetVSyncFrequency";
      view->AsArkWebRenderWidgetHostViewOSRExt()->ResetVSyncFrequency();
      has_touch_event_ = false;
      set_lower_frame_rate_ = false;
    }
  }
}

void AlloyBrowserHostImpl::MediaPlayerGone(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaPlayerGone, is_video: "
            << video_type.has_video;
  cef_media_type_t type =
      video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  if (client_.get() && client_->AsArkWebClient()->GetMediaHandler().get()) {
    client_->AsArkWebClient()->GetMediaHandler()->OnMediaStateChanged(
        this, type, cef_media_playing_state_t::PLAYER_GONE);
  }
}
#endif

void AlloyBrowserHostImpl::StartAudioCapturer() {
  if (!client_.get() || audio_capturer_) {
    return;
  }

  CefRefPtr<CefAudioHandler> audio_handler = client_->GetAudioHandler();
  if (!audio_handler.get()) {
    return;
  }

  CefAudioParameters params;
  params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
  params.sample_rate = media::AudioParameters::kAudioCDSampleRate;
  params.frames_per_buffer = 1024;

  if (!audio_handler->GetAudioParameters(this, params)) {
    return;
  }

  audio_capturer_ =
      std::make_unique<CefAudioCapturer>(params, this, audio_handler);
}

// AlloyBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::AlloyBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<AlloyBrowserHostImpl> opener,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate)
    : CefBrowserHostBase(settings,
                         client,
                         std::move(platform_delegate),
                         browser_info,
                         request_context),
      content::WebContentsObserver(web_contents),
      is_windowless_(platform_delegate_->IsWindowless()) {
  contents_delegate_.ObserveWebContents(web_contents);

  if (opener.get()) {
    opener_id_ = opener->GetIdentifier();

    if (!is_views_hosted_) {
      // GetOpenerWindowHandle() only returns a value for non-views-hosted
      // popup browsers.
      opener_window_handle_ = opener->GetWindowHandle();
    }
  }

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Make sure RenderFrameCreated is called at least one time.
  RenderFrameCreated(web_contents->GetPrimaryMainFrame());

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
  bool is_safe_browsing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebEx);
  if (is_safe_browsing_enabled) {
    LOG(INFO) << "SafeBrowsing enabled, creating safe browsing tab helper";
    ohos_safe_browsing::SafeBrowsingTabHelper::CreateForWebContents(
        web_contents, request_context->IsOffTheRecord(), this);
  }
#endif

#if BUILDFLAG(ARKWEB_EXT_GET_ZOOM_LEVEL)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExGetZoomLevel)) {
    zoom::ZoomController::CreateForWebContents(web_contents);
  }
#endif
}

bool AlloyBrowserHostImpl::CreateHostWindow() {
  // |host_window_handle_| will not change after initial host creation for
  // non-views-hosted browsers.
  bool success = true;
  if (!IsWindowless()) {
    success = platform_delegate_->CreateHostWindow();
  }
  if (success && !is_views_hosted_) {
    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  }
  return success;
}

gfx::Point AlloyBrowserHostImpl::GetScreenPoint(const gfx::Point& view,
                                                bool want_dip_coords) const {
  CEF_REQUIRE_UIT();
  if (platform_delegate_) {
    return platform_delegate_->GetScreenPoint(view, want_dip_coords);
  }
  return gfx::Point();
}

void AlloyBrowserHostImpl::StartDragging(
    const content::DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  if (platform_delegate_) {
    platform_delegate_->StartDragging(drop_data, allowed_ops, image,
                                      image_offset, event_info, source_rwh);
  }
}

void AlloyBrowserHostImpl::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  if (platform_delegate_) {
    platform_delegate_->UpdateDragOperation(operation,
                                            document_is_handling_drag);
  }
}

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::SetVisible(int32_t nweb_id, bool visible) {
  auto strategy = -1;
  if (visible && ltpo_strategy_ < 0) {
    strategy = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                   .GetSystemPropertiesInstance()
                   .GetLTPOStrategy();
    ltpo_strategy_ = strategy;
  }
  if (auto* host = content::GpuProcessHost::Get()) {
    if (auto* host_impl = host->gpu_host()) {
      if (strategy >= 0) {
        host_impl->SetLTPOStrategy(ltpo_strategy_);
      }
      TRACE_EVENT2("base", "AlloyBrowserHostImpl::SetVisible", "nweb_id",
                   nweb_id, "visible", visible);
      LOG(INFO) << "AlloyBrowserHostImpl::SetVisible nweb_id: " << nweb_id
                << ", visible " << visible;
      host_impl->SetVisible(nweb_id, visible);
    }
  }
}

#if BUILDFLAG(ARKWEB_REPORT_RENDER_STATE)
base::ProcessId AlloyBrowserHostImpl::GetRenderProcessId() {
  content::WebContents* contents = web_contents();
  if (!contents) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::GetRenderProcessId web_contents is null";
    return 0;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::GetRenderProcessId "
                    "render_process_host is null";
      return 0;
    }
    return render_process_host->GetProcess().Pid();
  } else {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::GetRenderProcessId render_view_host is null";
    return 0;
  }
}
#endif

void AlloyBrowserHostImpl::OnWindowShow() {
  TRACE_EVENT0("base", "AlloyBrowserHostImpl::OnWindowShow");
  LOG(DEBUG) << "AlloyBrowserHostImpl::OnWindowShow";
#if BUILDFLAG(ARKWEB_REPORT_RENDER_STATE)
  RenderProcessStateHandler::GetInstance()->UpdateRenderProcessState(
      GetRenderProcessId(), nweb_id_, false);
#endif
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
  SetVisible(nweb_id_, true);
#endif
}

void AlloyBrowserHostImpl::OnWindowHide() {
  TRACE_EVENT0("base", "AlloyBrowserHostImpl::OnWindowHide");
  LOG(DEBUG) << "AlloyBrowserHostImpl::OnWindowHide";
#if BUILDFLAG(ARKWEB_REPORT_RENDER_STATE)
  RenderProcessStateHandler::GetInstance()->UpdateRenderProcessState(
      GetRenderProcessId(), nweb_id_, true);
#endif
  SetVisible(nweb_id_, false);
}

void AlloyBrowserHostImpl::OnOnlineRenderToForeground() {}

void AlloyBrowserHostImpl::SetWindowId(int window_id, int nweb_id) {
  window_id_ = window_id;
  nweb_id_ = nweb_id;
  OHOS::NWeb::ResSchedClientAdapter::ReportWindowId(
      static_cast<int32_t>(window_id), static_cast<int32_t>(nweb_id));
  ReportWindowStatus(false);
}

void AlloyBrowserHostImpl::RenderViewReady() {
  RenderProcessStateHandler::GetInstance()->InitRenderProcessState(
      GetRenderProcessId(), nweb_id_);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ReportWindowStatus, this, true));
    return;
  }
  ReportWindowStatus(true);
#if BUILDFLAG(ARKWEB_ZOOM)
  UpdateZoomSupportEnabled();
#endif
}

void AlloyBrowserHostImpl::InactiveUnloadOldProcess(base::ProcessId pid) {
  using namespace OHOS::NWeb;
  if (pid != last_pid_ && last_pid_ != -1) {
    ResSchedClientAdapter::ReportWindowStatus(
        ResSchedStatusAdapter::WEB_INACTIVE, last_pid_, window_id_, nweb_id_);
  }
  last_pid_ = pid;
}
#endif

#if BUILDFLAG(ARKWEB_CLIPBOARD)
bool AlloyBrowserHostImpl::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // This bool value is only used for touch insert handle quick menu.
  auto rvh = web_contents()->GetRenderViewHost();
  CefRenderWidgetHostViewOSR* view =
      static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());
  if (view) {
    CefTouchSelectionControllerClientOSR* touch_client =
        static_cast<CefTouchSelectionControllerClientOSR*>(
            view->selection_controller_client());
    if (touch_client && touch_client->HandleContextMenu(params)) {
      return true;
    }
  }

  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_.reset(
        new CefMenuManager(this, platform_delegate_->CreateMenuRunner()));
  }

  if (menu_manager_->CreateContextMenu(params)) {
    view->ResetGestureDetection(true);
    return true;
  }
  return false;
}
void AlloyBrowserHostImpl::UpdateZoomSupportEnabled() {
  auto rvh = web_contents()->GetRenderViewHost();
  ArkWebRenderWidgetHostViewOSRExt* view =
      static_cast<ArkWebRenderWidgetHostViewOSRExt*>(
          rvh->GetWidget()->GetView());
  if (view) {
    view->AsArkWebRenderWidgetHostViewOSRExt()->SetDoubleTapSupportEnabled(
        settings_.supports_double_tap_zoom);
    view->AsArkWebRenderWidgetHostViewOSRExt()->SetMultiTouchZoomSupportEnabled(
        settings_.supports_multi_touch_zoom);
  }
}
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_PRINT)

void AlloyBrowserHostImpl::SetToken(void* token) {
  if (platform_delegate_) {
    platform_delegate_->SetToken(token);
  }
}

void AlloyBrowserHostImpl::CreateWebPrintDocumentAdapter(
    const CefString& jobName,
    void** webPrintDocumentAdapter) {
  if (platform_delegate_) {
    platform_delegate_->CreateWebPrintDocumentAdapter(jobName,
                                                      webPrintDocumentAdapter);
  }
}

void AlloyBrowserHostImpl::SetPrintBackground(bool enable) {
  if (platform_delegate_) {
    platform_delegate_->SetPrintBackground(enable);
  }
}

bool AlloyBrowserHostImpl::GetPrintBackground() {
  if (platform_delegate_) {
    return platform_delegate_->GetPrintBackground();
  }
  return false;
}
#endif  // BUILDFLAG(ARKWEB_PRINT)

#if BUILDFLAG(ARKWEB_SAME_LAYER)
void AlloyBrowserHostImpl::SetNativeEmbedMode(bool flag) {
  if (platform_delegate_) {
    platform_delegate_->SetNativeEmbedMode(flag);
  }
}
#endif
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void AlloyBrowserHostImpl::WasKeyboardResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::WasKeyboardResized, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->WasKeyboardResized();
  }
}

void AlloyBrowserHostImpl::SetDrawRect(int x, int y, int width, int height) {
  if (platform_delegate_) {
    platform_delegate_->SetDrawRect(x, y, width, height);
  }
}

void AlloyBrowserHostImpl::SetDrawMode(int mode) {
  if (platform_delegate_) {
    platform_delegate_->SetDrawMode(mode);
  }
}

void AlloyBrowserHostImpl::SetFitContentMode(int mode) {
  if (platform_delegate_) {
    platform_delegate_->SetFitContentMode(mode);
  }
}

bool AlloyBrowserHostImpl::GetPendingSizeStatus() {
  if (platform_delegate_) {
    return platform_delegate_->GetPendingSizeStatus();
  }
  return false;
}

void AlloyBrowserHostImpl::SetShouldFrameSubmissionBeforeDraw(bool should) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      &AlloyBrowserHostImpl::SetShouldFrameSubmissionBeforeDraw,
                      this, should));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetShouldFrameSubmissionBeforeDraw(should);
  }
}

std::string AlloyBrowserHostImpl::GetCurrentLanguage() {
  if (!platform_delegate_) {
    return "";
  }
  return platform_delegate_->GetCurrentLanguage();
}
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

#if BUILDFLAG(ARKWEB_DISCARD)
bool AlloyBrowserHostImpl::Discard() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::Discard failed, called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::Discard failed, WebContents is nullptr";
    return false;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* render_process = main_frame->GetProcess();
  if (!render_process) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::Discard failed, RenderProcessHost is nullptr";
    return false;
  }

  bool fast_shutdown_success = render_process->FastShutdownIfPossible(1u, true);
  if (fast_shutdown_success) {
    web_contents()->SetWasDiscarded(true);
  }

#if BUILDFLAG(ARKWEB_BFCACHE)
  content::NavigationController& controller = web_contents()->GetController();
  controller.GetBackForwardCache().Flush();
#endif

  return fast_shutdown_success;
}

bool AlloyBrowserHostImpl::Restore() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::Restore failed, called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::Restore failed, WebContents is nullptr";
    return false;
  }

  web_contents()->GetController().SetNeedsReload();
  web_contents()->GetController().LoadIfNecessary();
  web_contents()->Focus();

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
  needs_reload_ = false;
#endif

  return true;
}
#endif  // BUILDFLAG(ARKWEB_DISCARD)

#if BUILDFLAG(ARKWEB_HTML_SELECT)
void AlloyBrowserHostImpl::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  if (platform_delegate_) {
    platform_delegate_->ShowPopupMenu(std::move(popup_client), bounds,
                                      item_height, item_font_size,
                                      selected_item, std::move(menu_items),
                                      right_aligned, allow_multiple_selection);
  }
}
#endif  // #if BUILDFLAG(ARKWEB_HTML_SELECT)

#if BUILDFLAG(ARKWEB_CSS_INPUT_TIME)
void AlloyBrowserHostImpl::OpenDateTimeChooser() {
  content::DateTimeChooserOHOS* date_time_chooser =
      content::DateTimeChooserOHOS::FromWebContents(web_contents());
  if (date_time_chooser && client_) {
    if (auto handler = client_->GetDialogHandler()) {
      blink::mojom::DateTimeDialogValuePtr& date_time_dialog_ptr =
          date_time_chooser->GetDialogValue();
      if (!date_time_dialog_ptr) {
        LOG(ERROR) << "OpenDateTime chooser failed";
        date_time_chooser->NotifyResult(false, 0);
        return;
      }
      CefDateTimeChooser chooser(
          static_cast<cef_text_input_type_t>(date_time_dialog_ptr->dialog_type),
          date_time_dialog_ptr->dialog_value, date_time_dialog_ptr->minimum,
          date_time_dialog_ptr->maximum, date_time_dialog_ptr->step);
      CefRefPtr<CefDateTimeChooserCallback> callback =
          new CefDateTimeChooserCallbackImpl(date_time_chooser);
      std::vector<CefDateTimeSuggestion> suggestions;
      for (size_t index = 0; index < date_time_dialog_ptr->suggestions.size();
           index++) {
        CefDateTimeSuggestion sug;
        CefString label;
        CefString localized_value;
        label.FromString16(date_time_dialog_ptr->suggestions[index]->label);
        localized_value.FromString16(
            date_time_dialog_ptr->suggestions[index]->localized_value);
        sug.value = date_time_dialog_ptr->suggestions[index]->value;
        cef_string_set(label.c_str(), label.length(), &(sug.label), true);
        cef_string_set(localized_value.c_str(), localized_value.length(),
                       &(sug.localized_value), true);
        suggestions.push_back(sug);
      }
      handler->OnDateTimeChooserPopup(this, chooser, suggestions, callback);
    }
  }
}

void AlloyBrowserHostImpl::CloseDateTimeChooser() {
  if (client_) {
    if (auto handler = client_->GetDialogHandler()) {
      handler->OnDateTimeChooserClose();
    }
  }
}
#endif  // #if BUILDFLAG(ARKWEB_CSS_INPUT_TIME)

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
std::unique_ptr<content::CustomMediaPlayer>
AlloyBrowserHostImpl::CreateCustomMediaPlayer(
    std::unique_ptr<content::CustomMediaPlayerListener> listener,
    const content::MediaInfo& media_info) {
  if (!client_) {
    LOG(ERROR) << "CreateCustomMediaPlayer failed, client_ is null";
    return nullptr;
  }

  CefOwnPtr<CefCustomMediaPlayerDelegate> delegate;
  static_assert(sizeof(CefCustomMediaInfo) == sizeof(content::MediaInfo));
  CefCustomMediaInfo cef_media_info;

  cef_media_info.embed_id = media_info.embed_id;
  cef_media_info.media_type = static_cast<uint32_t>(media_info.media_type);
  cef_media_info.media_src_list.reserve(media_info.media_src_list.size());
  for (const auto& info : media_info.media_src_list) {
    cef_media_info.media_src_list.push_back(
        {static_cast<uint32_t>(info.source_type), info.media_source,
         info.media_format});
  }
  cef_media_info.surface_info.id = media_info.surface_info.id;
  cef_media_info.surface_info.x = media_info.surface_info.x;
  cef_media_info.surface_info.y = media_info.surface_info.y;
  cef_media_info.surface_info.width = media_info.surface_info.width;
  cef_media_info.surface_info.height = media_info.surface_info.height;

  cef_media_info.controls = media_info.controls;
  cef_media_info.controlslist.reserve(media_info.controlslist.size());
  for (const auto& item : media_info.controlslist) {
    cef_media_info.controlslist.push_back(item);
  }

  cef_media_info.muted = media_info.muted;
  cef_media_info.poster_url = media_info.poster_url;
  cef_media_info.preload = static_cast<uint32_t>(media_info.preload);
  cef_media_info.https_headers = media_info.https_headers;
  cef_media_info.attributes = media_info.attributes;

  delegate = client_->AsArkWebClient()->OnCreateCustomMediaPlayer(
      CefOwnPtr<CefMediaPlayerListenerImpl>(
          new CefMediaPlayerListenerImpl(std::move(listener))),
      cef_media_info);
  if (!delegate) {
    LOG(ERROR) << "CreateCustomMediaPlayer, no media player";
    return nullptr;
  }
  return std::make_unique<CustomMediaPlayerProxy>(std::move(delegate));
}
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
void AlloyBrowserHostImpl::OnShowToast(double duration,
                                       const std::string& toast) {
  if (!client_) {
    LOG(WARNING) << "client is nullptr when notify to show toast";
    return;
  }

  client_->OnShowToast(duration, toast);
}

void AlloyBrowserHostImpl::OnShowVideoAssistant(
    const std::string& videoAssistantItems) {
  if (!client_) {
    LOG(WARNING) << "client is nullptr when notify to show video assistant";
    return;
  }

  client_->OnShowVideoAssistant(videoAssistantItems);
}

void AlloyBrowserHostImpl::OnReportStatisticLog(const std::string& content) {
  if (!client_) {
    LOG(WARNING) << "client is nullptr when notify to report statistic log";
    return;
  }

  client_->OnReportStatisticLog(content);
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
void AlloyBrowserHostImpl::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  contents_delegate_.ShowRepostFormWarningDialog(source);
}
#endif  // ARKWEB_NETWORK_BASE

#if BUILDFLAG(ARKWEB_ADBLOCK)
void AlloyBrowserHostImpl::OnAdsBlocked(
    const std::string& main_frame_url,
    const std::map<std::string, int32_t>& subresource_blocked,
    bool is_site_first_report) {
  if (platform_delegate_) {
    platform_delegate_->OnAdsBlocked(main_frame_url, subresource_blocked,
                                     is_site_first_report);
  }
}

bool AlloyBrowserHostImpl::TrigAdBlockEnabledForSiteFromUi(
    const std::string& main_frame_url,
    int main_frame_tree_node_id) {
  if (platform_delegate_) {
    return platform_delegate_->TrigAdBlockEnabledForSiteFromUi(
        main_frame_url, main_frame_tree_node_id);
  }

  return false;
}
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
void AlloyBrowserHostImpl::WebExtensionTabUpdated(
    int tab_id,
    const std::vector<CefString>& changed_property_names,
    const CefString& url) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "TabUpdated get contents failed.";
    return;
  }

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "TabUpdated get browser context failed.";
    return;
  }

  std::vector<std::string> changed_properties;
  std::for_each(changed_property_names.begin(), changed_property_names.end(),
                [&changed_properties](const CefString& name) {
                  changed_properties.emplace_back(name.ToString());
                });

  extensions::cef::TabsWindowsAPI::Get(browser_context)
      ->TabUpdated(tab_id, web_contents, changed_properties, url.ToString());
}

void AlloyBrowserHostImpl::WebExtensionUpdateTabUrl(int32_t tab_id,
                                                    const GURL& url) {
  contents_delegate_.WebExtensionUpdateTabUrl(tab_id, url);
}

void AlloyBrowserHostImpl::WebExtensionUpdateTab(
    int32_t tab_id,
    const NWebExtensionTabUpdateProperties* update_properties) {
  contents_delegate_.WebExtensionUpdateTab(tab_id, update_properties);
}

void AlloyBrowserHostImpl::ExtensionSetTabId(int32_t tab_id) {
  tab_id_ = tab_id;
}

int32_t AlloyBrowserHostImpl::ExtensionGetTabId() const {
  return tab_id_;
}

void AlloyBrowserHostImpl::WebExtensionTabUpdated(
    int tab_id,
    const std::vector<CefString>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "TabUpdated get contents failed.";
    return;
  }
 
  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "TabUpdated get browser context failed.";
    return;
  }
 
  std::vector<std::string> changed_properties;
  std::for_each(changed_property_names.begin(), changed_property_names.end(),
      [&changed_properties] (const CefString& name) {
    changed_properties.emplace_back(name.ToString());
  });
 
  extensions::cef::TabsWindowsAPI::Get(browser_context)->TabUpdated(
      tab_id, web_contents, changed_properties, std::move(changeInfo));
}
 
void AlloyBrowserHostImpl::WebExtensionTabActivated(int tab_id, int window_id) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "TabActivated get contents failed.";
    return;
  }
  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "TabActivated get browser context failed.";
    return;
  }
  extensions::cef::TabsWindowsAPI::Get(browser_context)
      ->TabActived(tab_id, window_id, web_contents);
}
 
void AlloyBrowserHostImpl::WebExtensionActionClicked(
    std::string extensionId,
    const NWebExtensionTab* tab) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    LOG(ERROR) << "WebExtensionActionClicked get web_contents failed.";
 
  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    LOG(ERROR) << "WebExtensionActionClicked get browser_context failed.";
    
  extensions::ExtensionActionDispatcher::Get(browser_context)
      ->DispatchExtensionActionClickedWithCustomArgs(web_contents, extensionId,
                                                     tab);
}

#endif // #if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
void AlloyBrowserHostImpl::OnBeforeUnloadFired(bool proceed) {
  if (platform_delegate_) {
    platform_delegate_->OnBeforeUnloadFired(proceed);
  }
}
#endif  // ARKWEB_DISATCH_BEFORE_UNLOAD

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
void AlloyBrowserHostImpl::OnShareFile(const std::string& filePath,
                                       const std::string& utdTypeId) {
  if (platform_delegate_) {
    platform_delegate_->OnShareFile(filePath, utdTypeId);
  }
}
#endif

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
std::unique_ptr<content::VideoAssistant>
AlloyBrowserHostImpl::CreateVideoAssistant() {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  return std::make_unique<nweb_ex::VideoAssistant>(this);
#else
  return content::WebContentsDelegate::CreateVideoAssistant();
#endif  // ARKWEB_NWEB_EX
}

void AlloyBrowserHostImpl::PopluateVideoAssistantConfig(
    const std::string& url,
    media::mojom::VideoAssistantConfigPtr& config) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  auto cloud_control = nweb_ex::AlloyBrowserEngineCloudConfig::GetInstance();
  nweb_ex::BrowserEngineCloudControlConfig cloud_config;
  bool match = cloud_control->GetControlConfigByUrl(url, cloud_config);
  if (!match) {
    config->video_assistant = true;
    config->playback_rate = true;
    config->download_button =
        media::mojom::VideoAssistantDownloadButton::kDownloadPerPage;
    return;
  }
  config->video_assistant = cloud_config.videoAssistant;
  config->playback_rate = cloud_config.playbackRate;
  switch (cloud_config.downloadBtn) {
    case nweb_ex::DownloadBtn::kDownloadPerPage:
      config->download_button =
          media::mojom::VideoAssistantDownloadButton::kDownloadPerPage;
      break;
    case nweb_ex::DownloadBtn::kDownloadForceShow:
      config->download_button =
          media::mojom::VideoAssistantDownloadButton::kDownloadForceShow;
      break;
    case nweb_ex::DownloadBtn::kDownloadForceHide:
      config->download_button =
          media::mojom::VideoAssistantDownloadButton::kDownloadForceHide;
      break;
  }
#endif  // ARKWEB_NWEB_EX
}
#endif  // ARKWEB_VIDEO_ASSISTANT

#if BUILDFLAG(ARKWEB_DATALIST)
void AlloyBrowserHostImpl::OnShowAutofillPopup(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions,
    bool is_password_popup_type) {
  if (platform_delegate_) {
    platform_delegate_->OnShowAutofillPopup(element_bounds, is_rtl, suggestions,
                                            is_password_popup_type);
  }
}

void AlloyBrowserHostImpl::OnHideAutofillPopup() {
  if (platform_delegate_) {
    platform_delegate_->OnHideAutofillPopup();
  }
}
#endif

#if BUILDFLAG(ARKWEB_EX_REFRESH_IFRAME)
bool AlloyBrowserHostImpl::IsIframe() {
  if (web_contents() && web_contents()->GetFocusedFrame()) {
    return !!web_contents()->GetFocusedFrame()->GetParentOrOuterDocument();
  }
  return false;
}

void AlloyBrowserHostImpl::ReloadFocusedFrame() {
  if (web_contents()) {
    web_contents()->ReloadFocusedFrame();
  }
}
#endif
