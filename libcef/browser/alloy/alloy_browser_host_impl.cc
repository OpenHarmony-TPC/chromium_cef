// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "libcef/browser/audio_capturer.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager.h"
#include "libcef/browser/media_access_query.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/features/runtime_checks.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/base/net_errors.h"
#include "ui/events/base_event_utils.h"

#if BUILDFLAG(IS_OHOS)
#include "base/logging.h"
#include "base/ohos/ltpo/include/sliding_observer.h"
#include "base/ohos/ltpo/include/dynamic_frame_rate_decision.h"
#include "base/ohos/sys_info_utils.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "libcef/browser/alloy/render_process_state_handler.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "res_sched_client_adapter.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "gpu/ipc/common/gpu_surface_id_tracker.h"
#include "ohos_adapter_helper.h"
#endif

#if defined(OHOS_MEDIA_POLICY)
#include "content/browser/media/session/media_session_impl.h"
#endif // defined(OHOS_MEDIA_POLICY)

#ifdef OHOS_CSS_INPUT_TIME
#include "content/browser/ohos/date_time_chooser_ohos.h"
#endif  // OHOS_CSS_INPUT_TIME

#if defined(OHOS_WEBRTC)
#include "cef/libcef/browser/permission/alloy_access_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#endif

#ifdef OHOS_EX_GET_ZOOM_LEVEL
#include "base/command_line.h"
#include "net/base/url_util.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "components/zoom/page_zoom.h"
#include "components/prefs/pref_service.h"
#endif

#if defined(OHOS_INPUT_EVENTS)
#include "ohos_nweb/src/nweb_inputmethod_handler.h"
#include "libcef/browser/browser_util.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "libcef/browser/alloy/cef_extension_keybinding_registry_views.h"
#endif

#if defined(OHOS_INCOGNITO_MODE)
#include "libcef/browser/alloy/alloy_off_the_record_browser_context.h"
#endif

#if defined(OHOS_SECURE_JAVASCRIPT_PROXY)
#include "libcef/browser/javascript/oh_javascript_injector.h"
#endif

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
#include "libcef/browser/alloy/custom_media_player_proxy.h"
#include "cef/include/cef_media_player_listener.h"
#include "content/public/browser/custom_media_info.h"
#include "content/public/browser/custom_media_player_listener.h"
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#ifdef OHOS_BFCACHE
#include "content/public/common/content_switches.h"
#endif

using content::KeyboardEventProcessingResult;

#if BUILDFLAG(IS_OHOS)
int32_t AlloyBrowserHostImpl::ltpo_strategy_ = -1;
#endif

#if defined(OHOS_VIDEO_ASSISTANT)
#include "base/json/json_writer.h"
#include "base/values.h"
#include "content/public/browser/media_player_controller.h"
#include "content/public/browser/media_player_listener.h"
#include "content/browser/media/video_assistant/video_assistant.h"
#include "libcef/browser/alloy/cef_media_player_controller_impl.h"
#include "libcef/browser/alloy/media_player_listener_proxy.h"
#include "media/mojo/mojom/media_player.mojom.h"
#ifdef OHOS_NWEB_EX
#include "ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_engine_cloud_config.h"
#include "ohos_nweb_ex/overrides/cef/libcef/browser/video_assistant/media_player_controller_impl.h"
#include "ohos_nweb_ex/overrides/cef/libcef/browser/video_assistant/video_assistant.h"
#endif // OHOS_NWEB_EX
#endif // OHOS_VIDEO_ASSISTANT

namespace {

class ShowDevToolsHelper {
 public:
  ShowDevToolsHelper(CefRefPtr<AlloyBrowserHostImpl> browser,
                     const CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient> client,
                     const CefBrowserSettings& settings,
                     const CefPoint& inspect_element_at)
      : browser_(browser),
        window_info_(windowInfo),
        client_(client),
        settings_(settings),
        inspect_element_at_(inspect_element_at) {}

  CefRefPtr<AlloyBrowserHostImpl> browser_;
  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefBrowserSettings settings_;
  CefPoint inspect_element_at_;
};

void ShowDevToolsWithHelper(ShowDevToolsHelper* helper) {
  helper->browser_->ShowDevTools(helper->window_info_, helper->client_,
                                 helper->settings_,
                                 helper->inspect_element_at_);
  delete helper;
}

static constexpr base::TimeDelta kRecentlyAudibleTimeout = base::Seconds(2);

#ifdef OHOS_CSS_INPUT_TIME
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

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
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
  void OnError(uint32_t error_code, const std::string& error_msg) override {
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
  std::unique_ptr<content::CustomMediaPlayerListener> listener_;
};
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#ifdef OHOS_VIDEO_ASSISTANT
#ifdef OHOS_NWEB_EX
std::string BuildMediaInfo(
    const media::mojom::MediaInfoForVASTPtr& media_info,
    const media::mojom::VideoAssistantConfigPtr& config) {
  bool show_download_btn = media_info->duration > 0 &&
      media_info->duration < std::numeric_limits<double>::max() &&
      config->download_button !=
          media::mojom::VideoAssistantDownloadButton::kDownloadForceHide;
  bool show_playback_rate = config->playback_rate;
  base::Value::Dict root;
  root.Set("id", media_info->id);
  root.Set("title", media_info->title);
  root.Set("duration", media_info->duration);
  root.Set("curTime", media_info->current_time);
  root.Set("playbackrate", media_info->playback_rate);
  root.Set("width", media_info->video_width);
  root.Set("height", media_info->video_height);
  root.Set("muted", media_info->isMuted);
  root.Set("isPlaying", media_info->isPlaying);
  root.Set("downloadBtn", show_download_btn);
  root.Set("playbackrateBtn", show_playback_rate);

  root.Set("fullscreenoverlay", media_info->fullscreen_overlay);

  auto json = base::WriteJson(root);
  return json ? json.value() : std::string();
}
#endif // OHOS_NWEB_EX
#endif // OHOS_VIDEO_ASSISTANT

}  // namespace

// AlloyBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::Create(
    CefBrowserCreateParams& create_params) {
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate =
      CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  const bool is_devtools_popup = !!create_params.devtools_opener;

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
          is_devtools_popup, platform_delegate->IsWindowless(),
          create_params.extra_info);

  bool own_web_contents = false;

  // This call may modify |create_params|.
  auto web_contents =
      platform_delegate->CreateWebContents(create_params, own_web_contents);

  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(create_params.request_context.get());

  CefRefPtr<CefExtension> cef_extension;
  if (create_params.extension) {
    auto cef_browser_context = request_context_impl->GetBrowserContext();
    cef_extension =
        cef_browser_context->GetExtension(create_params.extension->id());
    CHECK(cef_extension);
  }

  auto platform_delegate_ptr = platform_delegate.get();

  CefRefPtr<AlloyBrowserHostImpl> browser = CreateInternal(
      create_params.settings, create_params.client, web_contents,
      own_web_contents, info,
      static_cast<AlloyBrowserHostImpl*>(create_params.devtools_opener.get()),
      is_devtools_popup, request_context_impl, std::move(platform_delegate),
      cef_extension);
  if (!browser) {
    return nullptr;
  }

  GURL url = url_util::MakeGURL(create_params.url, /*fixup=*/true);

  if (create_params.extension) {
    platform_delegate_ptr->CreateExtensionHost(
        create_params.extension, url, create_params.extension_host_type);
  } else if (!url.is_empty()) {
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
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);

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
      std::move(platform_delegate), extension);
  browser->InitializeBrowser();

  if (!browser->CreateHostWindow()) {
    return nullptr;
  }

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  if (opener && opener->platform_delegate_) {
    // 1. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    // Do this first for consistency with the Chrome runtime.
    opener->platform_delegate_->PopupBrowserCreated(browser.get(),
                                                    is_devtools_popup);
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
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<AlloyBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<AlloyBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForContents(contents);
  return static_cast<AlloyBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForGlobalId(global_id);
  return static_cast<AlloyBrowserHostImpl*>(browser.get());
}

// AlloyBrowserHostImpl methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::~AlloyBrowserHostImpl() {}

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
#if defined(OHOS_CLOSE_STEPS)
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
  return opener_;
}

double AlloyBrowserHostImpl::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
#if BUILDFLAG(IS_OHOS)
    event_ = std::make_shared<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::GetZoomLevelCallback, this));
    if (!event_->TimedWait(base::Milliseconds(10))) {
      return 0;
    }
    return curFactor_;
#else
    return 0;
#endif
  }
  if (web_contents()) {
#if BUILDFLAG(IS_OHOS)
    curFactor_ = content::HostZoomMap::GetZoomLevel(web_contents());
#else
    return content::HostZoomMap::GetZoomLevel(web_contents());
#endif
  }

#if BUILDFLAG(IS_OHOS)
  return curFactor_;
#else
  return 0;
#endif
}

void AlloyBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents()) {
      content::HostZoomMap::SetZoomLevel(web_contents(), zoomLevel);
    }
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetZoomLevel,
                                          this, zoomLevel));
  }
}

void AlloyBrowserHostImpl::SetBrowserZoomLevel(double zoom_factor) {
#ifdef OHOS_EX_GET_ZOOM_LEVEL
  if (CEF_CURRENTLY_ON_UIT()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableNwebExGetZoomLevel) &&
        web_contents()) {
      zoom::ZoomController* zoom_controller =
          zoom::ZoomController::FromWebContents(web_contents());
      if (!zoom_controller) {
        LOG(ERROR) << "SetBrowserZoomLevel has no zoom controller.";
        return;
      }
      zoom_controller->SetZoomLevel(
          blink::PageZoomFactorToZoomLevel(zoom_factor));
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetBrowserZoomLevel,
                                 this, zoom_factor));
  }
#endif
}
void AlloyBrowserHostImpl::Find(const CefString& searchText,
                                bool forward,
                                bool matchCase,
                                bool findNext
#if BUILDFLAG(IS_OHOS)
                                ,
                                bool newSession
#endif
) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::Find, this, searchText,
                                 forward, matchCase, findNext
#if BUILDFLAG(IS_OHOS)
                                 ,
                                 newSession
#endif
                                 ));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Find(searchText, forward, matchCase, findNext
#if BUILDFLAG(IS_OHOS)
                             ,
                             newSession
#endif
    );
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

void AlloyBrowserHostImpl::ShowDevTools(const CefWindowInfo& windowInfo,
                                        CefRefPtr<CefClient> client,
                                        const CefBrowserSettings& settings,
                                        const CefPoint& inspect_element_at) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    ShowDevToolsHelper* helper = new ShowDevToolsHelper(
        this, windowInfo, client, settings, inspect_element_at);
    CEF_POST_TASK(CEF_UIT, base::BindOnce(ShowDevToolsWithHelper, helper));
    return;
  }

  if (!EnsureDevToolsManager()) {
    return;
  }
  devtools_manager_->ShowDevTools(windowInfo, client, settings,
                                  inspect_element_at);
}

#ifdef OHOS_DEVTOOLS
void AlloyBrowserHostImpl::ShowDevToolsWith(
    CefRefPtr<CefBrowserHost> frontend_browser,
    CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
    const CefPoint& inspect_element_at) {
  LOG(INFO) << "ShowDevToolsWith";
  CEF_REQUIRE_UIT();
  if (!EnsureDevToolsManager()) {
    return;
  }
  devtools_manager_->ShowDevToolsWith(
      frontend_browser, delegate, inspect_element_at);
}
#endif // OHOS_DEVTOOLS

void AlloyBrowserHostImpl::CloseDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::CloseDevTools, this));
    return;
  }

  if (!devtools_manager_) {
    return;
  }
  devtools_manager_->CloseDevTools();
}

bool AlloyBrowserHostImpl::HasDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (!devtools_manager_) {
    return false;
  }
  return devtools_manager_->HasDevTools();
}

void AlloyBrowserHostImpl::SetAccessibilityState(
    cef_state_t accessibility_state) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetAccessibilityState,
                                 this, accessibility_state));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAccessibilityState(accessibility_state);
  }
}

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::GetRootBrowserAccessibilityManager(
    void** manager) {
  if (!platform_delegate_)
    return;
  *manager = static_cast<void*>(
        platform_delegate_->GetRootBrowserAccessibilityManager());
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

CefRefPtr<CefExtension> AlloyBrowserHostImpl::GetExtension() {
  return extension_;
}

bool AlloyBrowserHostImpl::IsBackgroundHost() {
  return is_background_host_;
}

bool AlloyBrowserHostImpl::IsWindowRenderingDisabled() {
  return IsWindowless();
}

#if defined(OHOS_INPUT_EVENTS)
void AlloyBrowserHostImpl::ScrollFocusedEditableNodeIntoView() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::ScrollFocusedEditableNodeIntoView, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ScrollFocusedEditableNodeIntoView();
  }
}
#endif

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
  LOG(DEBUG) << "web was hidden:" << hidden;
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }

  is_hidden_ = hidden;
  ReportWindowStatus(false);

  if (platform_delegate_) {
    platform_delegate_->WasHidden(hidden);
  }
}

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::WasOccluded(bool occluded) {
  LOG(DEBUG) << "web was occluded:" << occluded;
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasOccluded, this, occluded));
    return;
  }

  is_hidden_ = occluded;
  ReportWindowStatus(false);

  if (platform_delegate_)
    platform_delegate_->WasOccluded(occluded);
}

void AlloyBrowserHostImpl::OnWindowShow() {
  TRACE_EVENT0("base", "AlloyBrowserHostImpl::OnWindowShow");
  LOG(DEBUG) << "AlloyBrowserHostImpl::OnWindowShow";
  RenderProcessStateHandler::GetInstance()->UpdateRenderProcessState(GetRenderProcessId(), nweb_id_, false);
  SetVisible(nweb_id_, true);
}

void AlloyBrowserHostImpl::OnWindowHide() {
  TRACE_EVENT0("base", "AlloyBrowserHostImpl::OnWindowHide");
  LOG(DEBUG) << "AlloyBrowserHostImpl::OnWindowHide";
  RenderProcessStateHandler::GetInstance()->UpdateRenderProcessState(GetRenderProcessId(), nweb_id_, true);
  SetVisible(nweb_id_, false);
}

void AlloyBrowserHostImpl::OnOnlineRenderToForeground() {
}

void AlloyBrowserHostImpl::NotifyForNextTouchEvent() {
}

void AlloyBrowserHostImpl::SetVisible(int32_t nweb_id, bool visible)
{
  content::WebContents* contents = web_contents();
  if (!contents) {
    LOG(ERROR) << "AlloyBrowserHostImpl::ReportRenderProcessStatus web_contents is null";
    return;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::ReportRenderProcessStatus render_process_host is null";
      return;
    }
    auto strategy = -1;
    if (visible && ltpo_strategy_ < 0) {
      strategy = OHOS::NWeb::OhosAdapterHelper::GetInstance().
        GetSystemPropertiesInstance().GetLTPOStrategy();
      ltpo_strategy_ = strategy;
    }
    if (auto* host = content::GpuProcessHost::Get()) {
      if (auto* host_impl = host->gpu_host()) {
        if (strategy >= 0) {
          host_impl->SetLTPOStrategy(ltpo_strategy_);
        }
        TRACE_EVENT2("base", "AlloyBrowserHostImpl::SetVisible", "nweb_id", nweb_id, "visible", visible);
        LOG(INFO) << "AlloyBrowserHostImpl::SetVisible nweb_id: " << nweb_id << ", visible " << visible;
        host_impl->SetVisible(nweb_id, visible);
      }
    }
  }
}

base::ProcessId AlloyBrowserHostImpl::GetRenderProcessId() {
  content::WebContents* contents = web_contents();
  if (!contents) {
    LOG(ERROR) << "AlloyBrowserHostImpl::GetRenderProcessId web_contents is null";
    return 0;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::GetRenderProcessId render_process_host is null";
      return 0;
    }
    return render_process_host->GetProcess().Pid();
  } else {
    LOG(ERROR) << "AlloyBrowserHostImpl::GetRenderProcessId render_view_host is null";
    return 0;
  }
}

void AlloyBrowserHostImpl::SetEnableLowerFrameRate(bool enabled) {
  LOG(DEBUG) << "SetEnableLowerFrameRate:" << enabled;
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::SetEnableLowerFrameRate, this, enabled));
    return;
  }

  auto rvh = web_contents()->GetRenderViewHost();
  if (rvh && rvh->GetWidget()) {
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());
    if (view) {
      view->SetEnableLowerFrameRate(enabled);
    }
  }
}

void AlloyBrowserHostImpl::SetEnableHalfFrameRate(bool enabled) {
  LOG(DEBUG) << "SetEnableHalfFrameRate:" << enabled;
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::SetEnableHalfFrameRate, this, enabled));
    return;
  }

  auto rvh = web_contents()->GetRenderViewHost();
  if (rvh && rvh->GetWidget()) {
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());
    if (view) {
      view->SetEnableHalfFrameRate(enabled);
    }
  }
}

void AlloyBrowserHostImpl::SendTouchEventList(const std::vector<CefTouchEvent>& event_list) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SendTouchEventList,
                                          this, event_list));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendTouchEventList(event_list);
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
#if BUILDFLAG(IS_OHOS)
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

#if BUILDFLAG(IS_OHOS)
  if (event.type == CEF_TET_PRESSED) {
    has_touch_event_ = true;
    if (set_lower_frame_rate_) {
      ResetVSyncFrequency();
    }
  }

  if (event.type == CEF_TET_RELEASED) {
    if (has_video_playing_ && !set_lower_frame_rate_) {
      CEF_POST_DELAYED_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::UpdateVSyncFrequency,
                                                    this), WAIT_TOUCH_EVENT_DELAY_TIME);
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
  CloseBrowser(true);
}

bool AlloyBrowserHostImpl::WillBeDestroyed() const {
  CEF_REQUIRE_UIT();
  return destruction_state_ >= DESTRUCTION_STATE_ACCEPTED;
}

void AlloyBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  destruction_state_ = DESTRUCTION_STATE_COMPLETED;

  // Notify that this browser has been destroyed. These must be delivered in
  // the expected order.

  // 1. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserDestroyed().
  platform_delegate_->NotifyBrowserDestroyed();

  // 2. Notify the browser's LifeSpanHandler. This must always be the last
  // notification for this browser.
  OnBeforeClose();

  // Destroy any platform constructs first.
  if (javascript_dialog_manager_.get()) {
    javascript_dialog_manager_->Destroy();
  }
  if (menu_manager_.get()) {
    menu_manager_->Destroy();
  }

  // Notify any observers that may have state associated with this browser.
  OnBrowserDestroyed();

  // If the WebContents still exists at this point, signal destruction before
  // browser destruction.
  if (web_contents()) {
#if defined(OHOS_EX_GET_ZOOM_LEVEL)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExGetZoomLevel)) {
      auto* browser_context = CefBrowserContext::FromBrowserContext(
          web_contents()->GetBrowserContext());
      if (browser_context) {
        DCHECK(browser_context->AsProfile()->GetPrefs());
        browser_context->AsProfile()->GetPrefs()->CommitPendingWrite();
      }
    }
#endif
    WebContentsDestroyed();
  }

  // Disassociate the platform delegate from this browser.
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
    bool is_guest_view,
    const content::OpenURLParams& params) {
  if (is_guest_view && !params.is_pdf &&
      !params.url.SchemeIs(extensions::kExtensionScheme) &&
#if defined(OHOS_ARKWEB_EXTENSIONS)
      !params.url.SchemeIs(extensions::kArkwebExtensionScheme) &&
#endif
      !params.url.SchemeIs(content::kChromeUIScheme)) {
    // The PDF viewer will load the PDF extension in the guest view, and print
    // preview will load chrome://print in the guest view. The PDF renderer
    // used with PdfUnseasoned will set |params.is_pdf| when loading the PDF
    // stream (see PdfNavigationThrottle::WillStartRequest). All other
    // navigations are passed to the owner browser.
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      base::IgnoreResult(&AlloyBrowserHostImpl::OpenURLFromTab),
                      this, nullptr, params));

    return false;
  }

  return true;
}

extensions::ExtensionHost* AlloyBrowserHostImpl::GetExtensionHost() const {
  CEF_REQUIRE_UIT();
  DCHECK(platform_delegate_);
  return platform_delegate_->GetExtensionHost();
}

void AlloyBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::OnSetFocus,
                                          this, source));
    return;
  }

  if (contents_delegate_->OnSetFocus(source)) {
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetFocus(true);
  }
}

void AlloyBrowserHostImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  contents_delegate_->EnterFullscreenModeForTab(requesting_frame, options);
  WasResized();
}

void AlloyBrowserHostImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  contents_delegate_->ExitFullscreenModeForTab(web_contents);
  WasResized();
}

bool AlloyBrowserHostImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode AlloyBrowserHostImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
#ifdef OHOS_VIDEO_ASSISTANT
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        "enable-nweb-ex-video-assistant")) {
    return blink::mojom::DisplayMode::kBrowser;
  }
#endif // OHOS_VIDEO_ASSISTANT
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

#if defined(OHOS_MEDIA_POLICY)
void AlloyBrowserHostImpl::SetAudioResumeInterval(int resumeInterval) {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(web_contents());
  if (!mediaSession) {
    LOG(ERROR) << "AlloyBrowserHostImpl::SetAudioResumeInterval get mediaSession failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "AlloyBrowserHostImpl::SetAudioResumeInterval get mediaSession failed.";
#endif

    return;
  }
  mediaSession->audioResumeInterval_ = resumeInterval;
}

void AlloyBrowserHostImpl::SetAudioExclusive(bool audioExclusive) {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(web_contents());
  if (!mediaSession) {
    LOG(ERROR) << "AlloyBrowserHostImpl::SetAudioExclusive get mediaSession failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "AlloyBrowserHostImpl::SetAudioExclusive get mediaSession failed.";
#endif

    return;
  }
  mediaSession->audioExclusive_ = audioExclusive;
}
#endif // defined(OHOS_MEDIA_POLICY)

// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

content::WebContents* AlloyBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  auto target_contents = contents_delegate_->OpenURLFromTab(source, params);
  if (target_contents) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadMainFrameURL(params);
    return target_contents;
  }

  // Cancel the navigation.
  return nullptr;
}

bool AlloyBrowserHostImpl::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_main_frame_navigation) {
  return platform_delegate_->ShouldAllowRendererInitiatedCrossProcessNavigation(
      is_main_frame_navigation);
}

void AlloyBrowserHostImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  platform_delegate_->AddNewContents(source, std::move(new_contents),
                                     target_url, disposition, window_features,
                                     user_gesture, was_blocked);
}

void AlloyBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                               bool should_show_loading_ui) {
  contents_delegate_->LoadingStateChanged(source, should_show_loading_ui);
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
#if defined(OHOS_MULTI_WINDOW)
    // |DoClose| will notify the UI to close, |DESTRUCTION_STATE_NONE| means
    // |CloseBrowser| has not been triggered by UI. We should close browser
    // when received |CloseBrowser| request from UI.
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      close_browser = false;
    }
#endif  // defined(OHOS_MULTI_WINDOW)
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
    contents_delegate_->UpdateTargetURL(source, url);
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
void AlloyBrowserHostImpl::WebExtensionUpdateTab(
    int32_t tab_id,
    const NWebExtensionTabUpdateProperties* update_properties) {
  contents_delegate_->WebExtensionUpdateTab(tab_id, update_properties);
}

void AlloyBrowserHostImpl::SetTabId(int32_t tab_id) {
  tab_id_ = tab_id;
}

int32_t AlloyBrowserHostImpl::GetTabId() {
  return tab_id_;
}
#endif

bool AlloyBrowserHostImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  return contents_delegate_->DidAddMessageToConsole(source, level, message,
                                                    line_no, source_id);
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
  contents_delegate_->CanDownload(url, request_method, std::move(callback));
}

KeyboardEventProcessingResult AlloyBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return contents_delegate_->PreHandleKeyboardEvent(source, event);
}

#if defined(OHOS_INPUT_EVENTS)
bool AlloyBrowserHostImpl::IsNeedZoomChange(
  const content::NativeWebKeyboardEvent& event, bool& zoom_in) {
  if (!(event.GetModifiers() & blink::WebKeyboardEvent::kControlKey) ||
      (event.GetType() != blink::WebKeyboardEvent::Type::kRawKeyDown)) {
    LOG(DEBUG) << "not control key down";
    return false;
  }
  int32_t native_key_code = event.native_key_code;
  if (native_key_code  == OHOS::NWeb::ScanKeyCode::EQUAL_SCAN_CODE ||
      native_key_code  == OHOS::NWeb::ScanKeyCode::NUMPADADD_SCAN_CODE) {
    zoom_in = true;
    return true;
  }
  if (native_key_code  == OHOS::NWeb::ScanKeyCode::MINUS_SCAN_CODE ||
      native_key_code  == OHOS::NWeb::ScanKeyCode::NUMPADSUBTRACT_SCAN_CODE) {
    zoom_in = false;
    return true;
  }
  return false;
}

bool AlloyBrowserHostImpl::WebHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser) {
    return false;
  }

  if (contents_delegate_->HandleKeyboardEvent(source, event)) {
    return true;
  }

  if (source) {
    content::BrowserContext *browser_context = source->GetBrowserContext();
    extensions::ExtensionCommandsGlobalRegistry *registry =
        extensions::ExtensionCommandsGlobalRegistry::Get(browser_context);
    if (registry) {
      registry->registry_for_active_window();
    }

    bool run_accelerator_flag = true;
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);
    ui::KeyEvent key_event = accelerator.ToKeyEvent();
    const ui::EventType type = key_event.type();
    if (run_accelerator_flag && (type == ui::ET_KEY_PRESSED &&
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
    const content::NativeWebKeyboardEvent& event) {
#if defined(OHOS_INPUT_EVENTS)
  bool isUsed = WebHandleKeyboardEvent(source, event);
  CefRefPtr<CefKeyboardHandler> handler;
  CefKeyEvent cef_event;
  browser_util::GetCefKeyEvent(event, cef_event);
  if (client_) {
    handler = client_->GetKeyboardHandler();
  }
  if (handler) {
    handler->KeyboardReDispatch(cef_event, isUsed);
  }
  return isUsed;
#else
  // Check to see if event should be ignored.
  if (event.skip_in_browser) {
    return false;
  }

  if (contents_delegate_->HandleKeyboardEvent(source, event)) {
    return true;
  }

  if (platform_delegate_ && platform_delegate_->HandleKeyboardEvent(event)) {
    return true;
  }
  return false;
#endif
}

bool AlloyBrowserHostImpl::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  return platform_delegate_->PreHandleGestureEvent(source, event);
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
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
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
          new_contents, platform_delegate->IsWindowless(), extra_info);
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
  CefRefPtr<AlloyBrowserHostImpl> browser =
      CreateInternal(settings, client, new_contents, /*own_web_contents=*/false,
                     info, opener, /*is_devtools_popup=*/false, request_context,
                     std::move(platform_delegate), /*cef_extension=*/nullptr);
}

content::JavaScriptDialogManager*
AlloyBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  if (!javascript_dialog_manager_) {
    javascript_dialog_manager_.reset(new CefJavaScriptDialogManager(this));
  }
  return javascript_dialog_manager_.get();
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
    menu_manager_.reset(
        new CefMenuManager(this, platform_delegate_->CreateMenuRunner()));
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
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
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
#if defined(OHOS_WEBRTC)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  bool microphone_requested =
      (request.audio_type ==
       blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool webcam_requested = (request.video_type ==
                           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  bool screen_requested = (request.video_type ==
                           blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) ||
                          (request.video_type ==
                           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE);
  LOG(INFO) << "RequestMediaAccessPermission screen_requested: " << screen_requested;
  AlloyPermissionRequestHandler* permission_handler = GetPermissionRequestHandler();
  if (!permission_handler) {
    // Cancel the request.
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  if (!screen_requested && (microphone_requested || webcam_requested)) {
    permission_handler->SendRequest(new AlloyMediaAccessRequest(this, request, std::move(callback)));
  } else {
    permission_handler->SendScreenCaptureRequest(new AlloyScreenCaptureAccessRequest(this, request, std::move(callback)));
  }
#else
  auto returned_callback = media_access_query::RequestMediaAccessPermission(
      this, request, std::move(callback), /*default_disallow=*/true);
  // Callback should not be returned.
  DCHECK(returned_callback.is_null());
#endif // defined(OHOS_WEBRTC)
}

bool AlloyBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return media_access_query::CheckMediaAccessPermission(this, render_frame_host,
                                                        security_origin, type);
}

bool AlloyBrowserHostImpl::IsNeverComposited(
    content::WebContents* web_contents) {
  return platform_delegate_->IsNeverComposited(web_contents);
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

bool AlloyBrowserHostImpl::IsBackForwardCacheSupported() {
#if BUILDFLAG(IS_OHOS)
  // Turn this switch on and see if there's anything wrong,
  // issue #3237 not reproduced, maybe had been fixed.
#ifdef OHOS_BFCACHE
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableBFCache))
#endif
    return true;

#ifdef OHOS_BFCACHE
  return false;
#endif
#else
  // Disabled due to issue #3237.
  return false;
#endif
}

content::PreloadingEligibility AlloyBrowserHostImpl::IsPrerender2Supported(
    content::WebContents& web_contents) {
#if BUILDFLAG(IS_OHOS)
  return content::PreloadingEligibility::kPreloadingUnsupportedByWebContents;
#else
  return content::PreloadingEligibility::kEligible;
#endif
}

#if defined(OHOS_MULTI_WINDOW)
void AlloyBrowserHostImpl::ActivateContents(content::WebContents* contents) {
  LOG(INFO) << "AlloyBrowserHostImpl::ActivateContents";
  OnSetFocus(FOCUS_SOURCE_SYSTEM);
  if (contents_delegate_) {
    contents_delegate_->OnActivateContent();
  }
}
#endif

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::RequestToLockMouse(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  if (contents_delegate_) {
    contents_delegate_->RequestToLockMouse(web_contents, user_gesture,
                                           last_unlocked_by_target);
  }
}

void AlloyBrowserHostImpl::LostMouseLock() {
  if (contents_delegate_) {
    contents_delegate_->LostMouseLock();
  }
}

void AlloyBrowserHostImpl::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  if (contents_delegate_) {
    contents_delegate_->ShowRepostFormWarningDialog(source);
  }
}

void AlloyBrowserHostImpl::OnNativeEmbedStatusUpdate(
    const content::NativeEmbedInfo& native_embed_info,
    content::NativeEmbedInfo::TagState state) {
  if (!platform_delegate_) {
    return;
  }

  CefRenderHandler::CefNativeEmbedData data_info;
  data_info.status = static_cast<CefRenderHandler::CefEmbedLifeStatus>(state);
  content::GpuProcessHost* host = content::GpuProcessHost::Get();
  data_info.surfaceId = host->gpu_host()->GetSurfaceId(native_embed_info.native_embed_id);
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

void AlloyBrowserHostImpl::OnLayerRectVisibilityChange(const std::string& embed_id, bool visibility) {
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
#if defined(OHOS_INCOGNITO_MODE)
    if (web_contents()->GetBrowserContext() &&
        web_contents()->GetBrowserContext()->IsOffTheRecord()) {
      return;
    }
#endif
    auto cef_browser_context =
        static_cast<AlloyBrowserContext*>(web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(
          navigation_handle->GetRedirectChain());
    }
  }
}

void AlloyBrowserHostImpl::OnAudioStateChanged(bool audible) {
#if defined(OHOS_MEDIA_MUTE_AUDIO)
  LOG(INFO) << "OnAudioStateChanged: " << audible;

#ifdef OHOS_LOGGER_REPORT
  LOG_FEEDBACK(INFO) << "OnAudioStateChanged: " << audible;
#endif

  if (client_.get() && client_->GetMediaHandler().get()) {
    client_->GetMediaHandler()->OnAudioStateChanged(this, audible);
  }
#endif  // defined(OHOS_MEDIA_MUTE_AUDIO)

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

void AlloyBrowserHostImpl::OnFormEditingStateChanged(bool state, uint64_t form_id) {
  LOG(INFO) << "AlloyBrowserHostImpl::OnFormEditingStateChanged state: " << state;
  if (client_.get() && client_->GetFormHandler().get())
    client_->GetFormHandler()->OnFormEditingStateChanged(this, state, form_id);
}

void AlloyBrowserHostImpl::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStartedPlaying, is_video: " << video_type.has_video;
#if BUILDFLAG(IS_OHOS)
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* host = main_frame->GetProcess();
  if (host && video_type.has_video) {
    LOG(DEBUG) << "AlloyBrowserHostImpl::MediaStartedPlaying, pid: " << host->GetProcess().Pid()
        << ", video_stream_cnt: " << video_stream_cnt_;
    if (video_stream_cnt_ == 0) {
        OHOS::NWeb::ResSchedClientAdapter::ReportVideoPlaying(
            OHOS::NWeb::ResSchedStatusAdapter::VIDEO_PLAYING_START, host->GetProcess().Pid());
    }
    ++video_stream_cnt_;
  }
#endif
  start_play_ = true;
  cef_media_type_t type = video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  if (client_.get() && client_->GetMediaHandler().get()) {
    client_->GetMediaHandler()->OnMediaStateChanged(this, type, cef_media_playing_state_t::PLAYING);
  }

#if BUILDFLAG(IS_OHOS)
  if (type == cef_media_type_t::VIDEO && !set_lower_frame_rate_) {
    has_video_playing_ = true;
    has_touch_event_ = false;
    CEF_POST_DELAYED_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::UpdateVSyncFrequency,
                                                    this), WAIT_TOUCH_EVENT_DELAY_TIME);
  }
#endif
}

void AlloyBrowserHostImpl::MediaStoppedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStoppedPlaying, is_video: " << video_type.has_video << " stopped reason: " << static_cast<int>(reason);

  if (!start_play_) {
    return;
  }

#if BUILDFLAG(IS_OHOS)
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* host = main_frame->GetProcess();
  if (host && video_type.has_video) {
    LOG(DEBUG) << "AlloyBrowserHostImpl::MediaStartedPlaying, pid: " << host->GetProcess().Pid()
        << ", video_stream_cnt: " << video_stream_cnt_;
    --video_stream_cnt_;
    if (video_stream_cnt_ == 0) {
        OHOS::NWeb::ResSchedClientAdapter::ReportVideoPlaying(
            OHOS::NWeb::ResSchedStatusAdapter::VIDEO_PLAYING_STOP, host->GetProcess().Pid());
    }
    video_stream_cnt_ = std::max(video_stream_cnt_, 0);
  }
#endif

  cef_media_type_t type = video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  cef_media_playing_state_t state;

  switch(reason)
  {
    case content::WebContentsObserver::MediaStoppedReason::kReachedEndOfStream:
      state = cef_media_playing_state_t::END_OF_STREAM;
      break;
    case content::WebContentsObserver::MediaStoppedReason::kUnspecified:
      state = cef_media_playing_state_t::PAUSE;
      break;
  }

  if (client_.get() && client_->GetMediaHandler().get()) {
    client_->GetMediaHandler()->OnMediaStateChanged(this, type, state);
  }

#if BUILDFLAG(IS_OHOS)
  if (type == cef_media_type_t::VIDEO) {
    has_video_playing_ = false;
    if (set_lower_frame_rate_) {
      ResetVSyncFrequency();
    }
  }
#endif
}

#if BUILDFLAG(IS_OHOS)
  void AlloyBrowserHostImpl::MediaPlayerGone(const content::WebContentsObserver::MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) {
    LOG(INFO) << "AlloyBrowserHostImpl::MediaPlayerGone, is_video: " << video_type.has_video;
    cef_media_type_t type = video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
    if (client_.get() && client_->GetMediaHandler().get()) {
      client_->GetMediaHandler()->OnMediaStateChanged(this, type, cef_media_playing_state_t::PLAYER_GONE);
    }
  }
#endif

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::UpdateVSyncFrequency() {
  if (!base::ohos::IsMobileDevice()) {
    LOG(DEBUG) << " VSync adjustment is only available for mobile deive";
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
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());
    if (view) {
      LOG(INFO) << "AlloyBrowserHostImpl::UpdateVSyncFrequency";
      view->UpdateVSyncFrequency();
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
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());
    if (view) {
      LOG(INFO) << "AlloyBrowserHostImpl::ResetVSyncFrequency";
      view->ResetVSyncFrequency();
      has_touch_event_ = false;
      set_lower_frame_rate_ = false;
    }
  }
}
#endif

void AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired() {
  audio_capturer_.reset();
}

void AlloyBrowserHostImpl::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& content_event_bundle) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityEventReceived(content_event_bundle);
  }
}

void AlloyBrowserHostImpl::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityLocationChangesReceived(locData);
  }
}

void AlloyBrowserHostImpl::WebContentsDestroyed() {
  auto wc = web_contents();
  content::WebContentsObserver::Observe(nullptr);
  if (platform_delegate_) {
    platform_delegate_->WebContentsDestroyed(wc);
  }
}

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

  audio_capturer_.reset(new CefAudioCapturer(params, this, audio_handler));
}

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::GetZoomLevelCallback() {
  if (web_contents()) {
    curFactor_ = content::HostZoomMap::GetZoomLevel(web_contents());
    if (event_ != nullptr) {
      event_->Signal();
    }
  } else {
    curFactor_ = 0;
  }
}

SkColor AlloyBrowserHostImpl::GetBackgroundColor() const {
  return base_background_color_;
}

void AlloyBrowserHostImpl::AddVisitedLinks(const std::vector<CefString>& urls) {
  std::vector<GURL> urlList = std::vector<GURL>();
  for (auto url : urls) {
    urlList.push_back(url_util::MakeGURL(url, /*fixup=*/false));
  }
  if (web_contents()) {
#if defined(OHOS_INCOGNITO_MODE)
    if (web_contents()->GetBrowserContext() &&
        web_contents()->GetBrowserContext()->IsOffTheRecord()) {
      return;
    }
#endif
    auto cef_browser_context =
        static_cast<AlloyBrowserContext*>(web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(urlList);
    }
  }
}
#endif

// AlloyBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::AlloyBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<AlloyBrowserHostImpl> opener,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension)
    : CefBrowserHostBase(settings,
                         client,
                         std::move(platform_delegate),
                         browser_info,
                         request_context),
      content::WebContentsObserver(web_contents),
      opener_(kNullWindowHandle),
      is_windowless_(platform_delegate_->IsWindowless()),
      extension_(extension) {
  contents_delegate_->ObserveWebContents(web_contents);

  if (opener.get() && !is_views_hosted_) {
    // GetOpenerWindowHandle() only returns a value for non-views-hosted
    // popup browsers.
    opener_ = opener->GetWindowHandle();
  }

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Make sure RenderFrameCreated is called at least one time.
  RenderFrameCreated(web_contents->GetPrimaryMainFrame());

#ifdef OHOS_EX_GET_ZOOM_LEVEL
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

#ifdef OHOS_DRAG_DROP
gfx::Rect AlloyBrowserHostImpl::GetVisibleRectToWeb() {
  if (!GetClient() || !GetClient()->GetRenderHandler()) {
    return gfx::Rect();
  }
  CefRefPtr<CefRenderHandler> handler = GetClient()->GetRenderHandler();
  int visible_x = 0;
  int visible_y = 0;
  int visible_width = 0;
  int visible_height = 0;
  handler->GetVisibleRectToWeb(visible_x, visible_y, visible_width, visible_height);
  return gfx::Rect(visible_x, visible_y, visible_width, visible_height);
}
#endif

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

void AlloyBrowserHostImpl::UpdateDragCursor(
    ui::mojom::DragOperation operation) {
  if (platform_delegate_) {
    platform_delegate_->UpdateDragCursor(operation);
  }
}

#ifdef OHOS_CLIPBOARD
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
#endif  // #ifdef OHOS_CLIPBOARD

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::SetBackgroundColor(int color) {
  if (color == base_background_color_) {
    return;
  }

  base_background_color_ = color;
  OnWebPreferencesChanged();

#if defined(OHOS_BACKGROUND_COLOR)
  UpdateBackgroundColor(color);
#endif // defined(OHOS_BACKGROUND_COLOR)
}

void AlloyBrowserHostImpl::UpdateBackgroundColor(int color) {
  auto rvh = web_contents()->GetRenderViewHost();

  if (rvh->GetWidget()->GetView()) {
    rvh->GetWidget()->GetView()->SetBackgroundColor(color);
  }
}

void AlloyBrowserHostImpl::RenderViewReady() {
  RenderProcessStateHandler::GetInstance()->InitRenderProcessState(GetRenderProcessId(), nweb_id_);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ReportWindowStatus, this, true));
    return;
  }
  ReportWindowStatus(true);
  LOG(DEBUG) << "AlloyBrowserHostImpl::RenderViewReady";
#if BUILDFLAG(IS_OHOS)
  UpdateZoomSupportEnabled();
#endif
}

void AlloyBrowserHostImpl::InactiveUnloadOldProcess(base::ProcessId pid) {
  using namespace OHOS::NWeb;
  if(pid != last_pid_ && last_pid_ != -1) {
    ResSchedClientAdapter::ReportWindowStatus(ResSchedStatusAdapter::WEB_INACTIVE,
                                              last_pid_, window_id_, nweb_id_);
  }
  last_pid_ = pid;
}

void AlloyBrowserHostImpl::ReportWindowStatus(bool first_view_ready) {
  using namespace OHOS::NWeb;
  if (first_view_ready && is_hidden_) {
    LOG(INFO) << "no need to report render view ready because the view is hidden";
    return;
  }

  content::WebContents* contents = web_contents();
  if (!contents) {
    LOG(ERROR) << "AlloyBrowserHostImpl::ReportWindowStatus web_contents is null";
    return;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::ReportWindowStatus render_process_host is null";
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
    LOG(ERROR) << "AlloyBrowserHostImpl::ReportWindowStatus render_view_host is null";
    return;
  }
}

void AlloyBrowserHostImpl::UpdateZoomSupportEnabled() {
  auto rvh = web_contents()->GetRenderViewHost();
  auto view = rvh->GetWidget()->GetView();

  if (view) {
    view->SetDoubleTapSupportEnabled(settings_.supports_double_tap_zoom);
    view->SetMultiTouchZoomSupportEnabled(settings_.supports_multi_touch_zoom);
  }
}

void AlloyBrowserHostImpl::SetNativeEmbedMode(bool flag) {
    if (platform_delegate_) {
    platform_delegate_->SetNativeEmbedMode(flag);
  }
}

#ifdef OHOS_HTML_SELECT
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
#endif  // #ifdef OHOS_HTML_SELECT

#if defined(OHOS_COMPOSITE_RENDER)
void AlloyBrowserHostImpl::WasKeyboardResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::WasKeyboardResized, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->WasKeyboardResized();
}
#endif  // defined(OHOS_COMPOSITE_RENDER)

#if defined(OHOS_INPUT_EVENTS)
void AlloyBrowserHostImpl::SetVirtualKeyBoardArg(int32_t width, int32_t height, double keyboard) {
  if (platform_delegate_) {
    platform_delegate_->SetVirtualKeyBoardArg(width, height, keyboard);
  }
}

bool AlloyBrowserHostImpl::ShouldVirtualKeyboardOverlay() {
  if (platform_delegate_) {
    return platform_delegate_->ShouldVirtualKeyboardOverlay();
  }
  return false;
}

void AlloyBrowserHostImpl::AdvanceFocusForIME(int focusType) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::AdvanceFocusForIME,
                                          this, focusType));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->AdvanceFocusForIME(focusType);
  }
}
#endif

#if defined(OHOS_INPUT_EVENTS)
void AlloyBrowserHostImpl::ContentsZoomChange(bool zoom_in) {
#ifdef OHOS_NWEB_EX
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebEx)) {
    content::PageZoom zoomType = zoom_in ? content::PageZoom::PAGE_ZOOM_IN
                                         : content::PageZoom::PAGE_ZOOM_OUT;
    zoom::PageZoom::Zoom(web_contents(), zoomType);
    return;
  }
#endif

  double curFactor = GetZoomLevel();
  double tempZoomFactor = zoom_in ? curFactor + 2.0 : curFactor - 2.0;
  if (tempZoomFactor > 10.0 || tempZoomFactor < -10.0) {
    LOG(ERROR) << "The mouse wheel event can no longer be zoomed in or out.";
    return;
  }
  SetZoomLevel(tempZoomFactor);
}
#endif

#ifdef OHOS_CSS_INPUT_TIME
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
#endif  // #ifdef OHOS_CSS_INPUT_TIME
#endif

#ifdef OHOS_ARKWEB_ADBLOCK
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

#if defined(OHOS_EX_PASSWORD)
void AlloyBrowserHostImpl::ShowPasswordDialog(bool is_update,
                                              const std::string& url) {
  if (platform_delegate_) {
    platform_delegate_->ShowPasswordDialog(is_update, url);
  }
}
#endif

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
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

#if defined(OHOS_COMPOSITE_RENDER)
void AlloyBrowserHostImpl::SetDrawRect(int x, int y, int width, int height) {
  if (platform_delegate_)
    platform_delegate_->SetDrawRect(x, y, width, height);
}

void AlloyBrowserHostImpl::SetDrawMode(int mode) {
  if (drawMode_ != mode) {
    drawMode_ = mode;

    if (platform_delegate_)
      platform_delegate_->SetDrawMode(drawMode_);
  }
}

void AlloyBrowserHostImpl::SetFitContentMode(int mode) {
  if (platform_delegate_)
    platform_delegate_->SetFitContentMode(mode);
}

int AlloyBrowserHostImpl::GetDrawMode() {
  return drawMode_;
}

void AlloyBrowserHostImpl::SetShouldFrameSubmissionBeforeDraw(bool should) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      &AlloyBrowserHostImpl::SetShouldFrameSubmissionBeforeDraw,
                      this, should));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SetShouldFrameSubmissionBeforeDraw(should);
}

bool AlloyBrowserHostImpl::GetPendingSizeStatus() {
  if (platform_delegate_) {
    return platform_delegate_->GetPendingSizeStatus();
  }
  return false;
}
#endif  // defined(OHOS_COMPOSITE_RENDER)

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::SetWindowId(int window_id, int nweb_id) {
  window_id_ = window_id;
  nweb_id_ = nweb_id;
  OHOS::NWeb::ResSchedClientAdapter::ReportWindowId(static_cast<int32_t>(window_id), static_cast<int32_t>(nweb_id));
  ReportWindowStatus(false);
}

void AlloyBrowserHostImpl::SetWakeLockHandler(int32_t windowId, CefRefPtr<CefSetLockCallback> callback) {
#if defined(OHOS_SCREEN_LOCK)
  if (!web_contents()) {
    LOG(ERROR) << "set screen lock error, web_contents is nullptr";
    return;
  }
  SetKeepScreenOn handler = nullptr;
  if (callback) {
    handler = [callback](bool key) {
      if (callback) {
        callback->Handle(key);
      }
    };
  }
  web_contents()->SetWakeLockHandler(windowId, std::move(handler));
#endif
}
#endif

#if defined(OHOS_PRINT)
void AlloyBrowserHostImpl::SetToken(void* token) {
  if (platform_delegate_) {
    platform_delegate_->SetToken(token);
  }
}

void AlloyBrowserHostImpl::CreateWebPrintDocumentAdapter(const CefString& jobName,
                                                         void** webPrintDocumentAdapter) {
  if (platform_delegate_) {
    platform_delegate_->CreateWebPrintDocumentAdapter(jobName, webPrintDocumentAdapter);
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
#endif // defined(OHOS_PRINT)


bool AlloyBrowserHostImpl::Discard() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    LOG(ERROR) << "AlloyBrowserHostImpl::Discard failed, called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    LOG(ERROR) << "AlloyBrowserHostImpl::Discard failed, WebContents is nullptr";
    return false;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* render_process = main_frame->GetProcess();
  if (!render_process) {
    LOG(ERROR) << "AlloyBrowserHostImpl::Discard failed, RenderProcessHost is nullptr";
    return false;
  }

  bool fast_shutdown_success = render_process->FastShutdownIfPossible(1u, true);
  if (fast_shutdown_success) {
    web_contents()->SetWasDiscarded(true);
  }

#ifdef OHOS_BFCACHE
  content::NavigationController& controller = web_contents()->GetController();
  controller.GetBackForwardCache().Flush();
#endif

  return fast_shutdown_success;
}

bool AlloyBrowserHostImpl::Restore() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    LOG(ERROR) << "AlloyBrowserHostImpl::Restore failed, called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    LOG(ERROR) << "AlloyBrowserHostImpl::Restore failed, WebContents is nullptr";
    return false;
  }

  web_contents()->GetController().SetNeedsReload();
  web_contents()->GetController().LoadIfNecessary();
  web_contents()->Focus();

#ifdef OHOS_RENDER_PROCESS_MODE
  needs_reload_ = false;
#endif

  return true;
}

#ifdef OHOS_EX_GET_ZOOM_LEVEL
void AlloyBrowserHostImpl::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (data.web_contents != web_contents()) {
    return;
  }
  if (client_) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler) {
      handler->OnContentsBrowserZoomChange(
          blink::PageZoomLevelToZoomFactor(data.new_zoom_level),
          data.can_show_bubble);
    }
  }
  if (web_contents()) {
    if (auto* browser_context = CefBrowserContext::FromBrowserContext(
            web_contents()->GetBrowserContext())) {
      DCHECK(browser_context->AsProfile()->GetPrefs());
      browser_context->AsProfile()->GetPrefs()->CommitPendingWrite();
    }
  }
}
#endif

#if defined(OHOS_SECURE_JAVASCRIPT_PROXY)
CefString AlloyBrowserHostImpl::GetLastJavascriptProxyCallingFrameUrl() {
  if (!web_contents()) {
    return base::EmptyString();
  }

  NWEB::OhJavascriptInjector* javascriptInjector =
     NWEB::OhJavascriptInjector::FromWebContents(web_contents());
  if (!javascriptInjector) {
    return base::EmptyString();
  }
  return javascriptInjector->GetLastCallingFrameUrl();
}
#endif

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
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
    cef_media_info.media_src_list.push_back({
        static_cast<uint32_t>(info.source_type),
        info.media_source, info.media_format});
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

  delegate = client_->OnCreateCustomMediaPlayer(
      CefOwnPtr<CefMediaPlayerListenerImpl>(
          new CefMediaPlayerListenerImpl(std::move(listener))),
      cef_media_info);
  if (!delegate) {
    LOG(ERROR) << "CreateCustomMediaPlayer, no media player";
    return nullptr;
  }
  return std::make_unique<CustomMediaPlayerProxy>(std::move(delegate));

}
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#if defined(OHOS_VIDEO_ASSISTANT)
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
    LOG(WARNING) << "client is nullptr when notify to report sttistic log";
    return;
  }

  client_->OnReportStatisticLog(content);
}

std::unique_ptr<content::MediaPlayerListener>
AlloyBrowserHostImpl::OnFullScreenOverlayEnter(
    media::mojom::MediaInfoForVASTPtr media_info_ptr,
    const content::MediaPlayerId& media_player_id) {
  if (!client_) {
    LOG(WARNING) << "client is null, OnFullScreenOverlayEnter failed";
    return nullptr;
  }

  if (!GetWebContents()) {
    return nullptr;
  }
  std::unique_ptr<CefMediaPlayerListenerForVAST> listener;
#ifdef OHOS_NWEB_EX
  auto config = media::mojom::VideoAssistantConfig::New(true, true,
      media::mojom::VideoAssistantDownloadButton::kDownloadPerPage);
  auto url =
      GetWebContents()->GetLastCommittedURL().DeprecatedGetOriginAsURL().spec();
  PopluateVideoAssistantConfig(url, config);

  auto media_info = BuildMediaInfo(media_info_ptr, config);
  auto media_player_controller =
      std::make_unique<nweb_ex::MediaPlayerControllerImpl>(
          this, media_player_id, std::move(media_info_ptr), std::move(config));

  listener = client_->OnFullScreenOverlayEnter(
      std::make_unique<CefMediaPlayerControllerImpl>(
          std::move(media_player_controller)),
      media_info);
#endif // OHOS_NWEB_EX
  if (!listener) {
    return nullptr;
  }
  return std::make_unique<MediaPlayerListenerProxy>(std::move(listener));
}
#endif  // defined(OHOS_VIDEO_ASSISTANT)

#ifdef OHOS_RENDER_PROCESS_MODE
void AlloyBrowserHostImpl::NotifyNeedsReload(bool needs_reload) {
  if (is_hidden_ && needs_reload) {
    web_contents()->GetController().SetNeedsReload();
  }
  LOG(INFO) << "NotifyNeedsReload set needs reload: " << needs_reload;
  needs_reload_ = needs_reload;
}

bool AlloyBrowserHostImpl::NeedsReload() {
  return needs_reload_;
}
#endif

#ifdef OHOS_AI
void AlloyBrowserHostImpl::CreateOverlay(const gfx::ImageSkia& image,
                                         const gfx::Rect& image_rect,
                                         const gfx::Point& touch_point) {
  if (platform_delegate_) {
    platform_delegate_->CreateOverlay(image, image_rect, touch_point);
  }
}

void AlloyBrowserHostImpl::OnTextSelected(bool flag) {
  if (platform_delegate_) {
    platform_delegate_->OnTextSelected(flag);
  }
}

void AlloyBrowserHostImpl::OnDestroyImageAnalyzerOverlay() {
  if (platform_delegate_) {
    platform_delegate_->OnDestroyImageAnalyzerOverlay();
  }
}

float AlloyBrowserHostImpl::GetPageScaleFactor() {
  if (platform_delegate_) {
    return platform_delegate_->GetPageScaleFactor();
  }
  return 1;
}

void AlloyBrowserHostImpl::OnFoldStatusChanged(uint32_t foldstatus) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (platform_delegate_) {
      platform_delegate_->OnFoldStatusChanged(foldstatus);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::OnFoldStatusChanged,
                                 this, foldstatus));
  }
}
#endif

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::MaximizeResize() {
  if (platform_delegate_) {
    platform_delegate_->MaximizeResize();
  }
}
#endif

#ifdef OHOS_EX_REFRESH_IFRAME
bool AlloyBrowserHostImpl::IsIframe()
{
  if (web_contents() && web_contents()->GetFocusedFrame()) {
    return !!web_contents()->GetFocusedFrame()->GetParentOrOuterDocument();
  }
  return false;
}

void AlloyBrowserHostImpl::ReloadFocusedFrame()
{
  if (web_contents()) {
    web_contents()->ReloadFocusedFrame();
  }
}
#endif

#if defined(OHOS_VIDEO_ASSISTANT)
std::unique_ptr<content::VideoAssistant>
AlloyBrowserHostImpl::CreateVideoAssistant() {
#ifdef OHOS_NWEB_EX
  return std::make_unique<nweb_ex::VideoAssistant>(this);
#else
  return content::WebContentsDelegate::CreateVideoAssistant();
#endif // OHOS_NWEB_EX
}
void AlloyBrowserHostImpl::PopluateVideoAssistantConfig(
    const std::string& url,
    media::mojom::VideoAssistantConfigPtr& config) {
#ifdef OHOS_NWEB_EX
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
#endif // OHOS_NWEB_EX
}
#endif // OHOS_VIDEO_ASSISTANT

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
void AlloyBrowserHostImpl::OnBeforeUnloadFired(bool proceed) {
  if (platform_delegate_) {
    platform_delegate_->OnBeforeUnloadFired(proceed);
  }
}
#endif // OHOS_DISPATCH_BEFORE_UNLOAD
