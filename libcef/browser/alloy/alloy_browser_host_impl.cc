// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

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
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/native/cursor_util.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/features/runtime_checks.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
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
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-test-utils.h"
#include "ui/events/base_event_utils.h"

#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

#include "libcef/browser/osr/touch_selection_controller_client_osr.h"

#if BUILDFLAG(IS_OHOS)
#include "base/logging.h"
#include "content/browser/ohos/date_time_chooser_ohos.h"
#include "libcef/browser/autofill/oh_autofill_client.h"
#include "libcef/browser/permission/alloy_access_request.h"
#include "res_sched_client_adapter.h"
#endif
using content::KeyboardEventProcessingResult;

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

class CefWidgetHostInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting,
      public content::RenderWidgetHostObserver {
 public:
  CefWidgetHostInterceptor(AlloyBrowserHostImpl* browser,
                           content::RenderViewHost* render_view_host)
      : browser_(browser),
        render_widget_host_(
            content::RenderWidgetHostImpl::From(render_view_host->GetWidget())),
        impl_(render_widget_host_->widget_host_receiver_for_testing()
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
  AlloyBrowserHostImpl* const browser_;
  content::RenderWidgetHostImpl* const render_widget_host_;
  blink::mojom::WidgetHost* const impl_;
};

static constexpr base::TimeDelta kRecentlyAudibleTimeout = base::Seconds(2);

#if BUILDFLAG(IS_OHOS)
class CefDateTimeChooserCallbackImpl : public CefDateTimeChooserCallback {
 public:
  explicit CefDateTimeChooserCallbackImpl(
    content::DateTimeChooserOHOS* date_time_chooser)
      : date_time_chooser_(date_time_chooser){}

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
  if (!browser)
    return nullptr;

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
      if (own_web_contents)
        delete web_contents;
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

  if (!browser->CreateHostWindow())
    return nullptr;

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  // 1. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser. Block navigation to avoid issues with focus
  // changes being sent to an unbound interface.
  {
    auto navigation_lock = browser_info->CreateNavigationLock();
    browser->OnAfterCreated();
  }

  // 2. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  browser->platform_delegate_->NotifyBrowserCreated();

  if (opener && opener->platform_delegate_) {
    // 3. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    opener->platform_delegate_->PopupBrowserCreated(browser.get(),
                                                    is_devtools_popup);
  }

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
#if BUILDFLAG(IS_OHOS)
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
    NOTREACHED() << "called on invalid thread";
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

void AlloyBrowserHostImpl::SetFocus(bool focus) {
  // Always execute asynchronously to work around issue #3040.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetFocusInternal,
                                        this, focus));
}

void AlloyBrowserHostImpl::SetFocusInternal(bool focus) {
  CEF_REQUIRE_UIT();
  if (focus)
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  else if (platform_delegate_)
    platform_delegate_->SetFocus(false);
}

CefWindowHandle AlloyBrowserHostImpl::GetWindowHandle() {
  if (is_views_hosted_ && CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_)
      return platform_delegate_->GetHostWindowHandle();
  }
  return host_window_handle_;
}

CefWindowHandle AlloyBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_;
}

double AlloyBrowserHostImpl::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    event_ = std::make_shared<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::GetZoomLevelCallback, this));
    if (!event_->TimedWait(base::Milliseconds(10))) {
      return 0;
    }
    return curFactor_;
  }
  if (web_contents()) {
    curFactor_ = content::HostZoomMap::GetZoomLevel(web_contents());
  }
  return curFactor_;
}

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

void AlloyBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents())
      content::HostZoomMap::SetZoomLevel(web_contents(), zoomLevel);
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetZoomLevel,
                                          this, zoomLevel));
  }
}

void AlloyBrowserHostImpl::RunFileDialog(
    FileDialogMode mode,
    const CefString& title,
    const CefString& default_file_path,
    const std::vector<CefString>& accept_filters,
    int selected_accept_filter,
    CefRefPtr<CefRunFileDialogCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::RunFileDialog, this,
                                 mode, title, default_file_path, accept_filters,
                                 selected_accept_filter, callback));
    return;
  }

  EnsureFileDialogManager();
  file_dialog_manager_->RunFileDialog(mode, title, default_file_path,
                                      accept_filters, selected_accept_filter,
                                      callback);
}

void AlloyBrowserHostImpl::Print() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::Print, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->Print();
}

void AlloyBrowserHostImpl::PrintToPDF(const CefString& path,
                                      const CefPdfPrintSettings& settings,
                                      CefRefPtr<CefPdfPrintCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::PrintToPDF,
                                          this, path, settings, callback));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->PrintToPDF(path, settings, callback);
  }
}

void AlloyBrowserHostImpl::Find(const CefString& searchText,
                                bool forward,
                                bool matchCase,
                                bool findNext,
                                bool newSession) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::Find, this, searchText,
                                 forward, matchCase, findNext, newSession));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Find(searchText, forward, matchCase, findNext,
                             newSession);
  }
}

void AlloyBrowserHostImpl::StopFinding(bool clearSelection) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::StopFinding,
                                          this, clearSelection));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->StopFinding(clearSelection);
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

  if (!EnsureDevToolsManager())
    return;
  devtools_manager_->ShowDevTools(windowInfo, client, settings,
                                  inspect_element_at);
}

void AlloyBrowserHostImpl::CloseDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::CloseDevTools, this));
    return;
  }

  if (!devtools_manager_)
    return;
  devtools_manager_->CloseDevTools();
}

bool AlloyBrowserHostImpl::HasDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  if (!devtools_manager_)
    return false;
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

SkColor AlloyBrowserHostImpl::GetBackgroundColor() const {
  return base_background_color_;
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

  if (platform_delegate_)
    platform_delegate_->WasResized();
}

void AlloyBrowserHostImpl::WasHidden(bool hidden) {
  LOG(DEBUG) << "web was hidden:" << hidden;
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }

  is_hidden_ = hidden;
  ReportWindowStatus(false);

  if (platform_delegate_)
    platform_delegate_->WasHidden(hidden);
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
#endif

void AlloyBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->NotifyScreenInfoChanged();
}

void AlloyBrowserHostImpl::Invalidate(PaintElementType type) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->Invalidate(type);
}

void AlloyBrowserHostImpl::SendExternalBeginFrame() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendExternalBeginFrame, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendExternalBeginFrame();
}

void AlloyBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SendTouchEvent,
                                          this, event));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendTouchEvent(event);
}

void AlloyBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendCaptureLostEvent();
}

void AlloyBrowserHostImpl::NotifyMoveOrResizeStarted() {
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::NotifyMoveOrResizeStarted, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->NotifyMoveOrResizeStarted();
#endif
}

int AlloyBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
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

  if (platform_delegate_)
    platform_delegate_->SetWindowlessFrameRate(frame_rate);
}

// AlloyBrowserHostImpl public methods.
// -----------------------------------------------------------------------------

bool AlloyBrowserHostImpl::IsWindowless() const {
  return is_windowless_;
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
  if (file_dialog_manager_.get())
    file_dialog_manager_->Destroy();
  if (javascript_dialog_manager_.get())
    javascript_dialog_manager_->Destroy();
  if (menu_manager_.get())
    menu_manager_->Destroy();

  // Notify any observers that may have state associated with this browser.
  OnBrowserDestroyed();

  // If the WebContents still exists at this point, signal destruction before
  // browser destruction.
  if (web_contents()) {
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
  if (menu_manager_)
    menu_manager_->CancelContextMenu();
}

bool AlloyBrowserHostImpl::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    bool is_guest_view,
    const content::OpenURLParams& params) {
  if (is_guest_view && !params.is_pdf &&
      !params.url.SchemeIs(extensions::kExtensionScheme) &&
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

  if (contents_delegate_->OnSetFocus(source))
    return;

  if (platform_delegate_)
    platform_delegate_->SetFocus(true);
}

void AlloyBrowserHostImpl::RunFileChooser(
    const CefFileDialogRunner::FileChooserParams& params,
    CefFileDialogRunner::RunFileChooserCallback callback) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(params, std::move(callback));
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
    NOTREACHED() << "Window rendering is not disabled";
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
    NOTREACHED() << "Window rendering is not disabled";
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
    NOTREACHED() << "Window rendering is not disabled";
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
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ImeCancelComposition();
}

void AlloyBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragEnter,
                                 this, drag_data, event, allowed_ops));
    return;
  }

  if (!drag_data.get()) {
    NOTREACHED();
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
    NOTREACHED() << "Window rendering is not disabled";
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
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragTargetDragLeave();
}

void AlloyBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::DragTargetDrop,
                                          this, event));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragTargetDrop(event);
}

void AlloyBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragSourceSystemDragEnded();
}

void AlloyBrowserHostImpl::DragSourceEndedAt(
    int x,
    int y,
    CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragSourceEndedAt, this,
                                 x, y, op));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragSourceEndedAt(x, y, op);
}

void AlloyBrowserHostImpl::SetAudioMuted(bool mute) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetAudioMuted,
                                          this, mute));
    return;
  }
  if (!web_contents())
    return;
  web_contents()->SetAudioMuted(mute);
}

bool AlloyBrowserHostImpl::IsAudioMuted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }
  if (!web_contents())
    return false;
  return web_contents()->IsAudioMuted();
}

void AlloyBrowserHostImpl::SetAudioResumeInterval(int resumeInterval) {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(web_contents());
  if (!mediaSession) {
    LOG(ERROR) << "AlloyBrowserHostImpl::SetAudioResumeInterval get mediaSession failed.";
    return;
  }
  mediaSession->audioResumeInterval_ = resumeInterval;
}

void AlloyBrowserHostImpl::SetAudioExclusive(bool audioExclusive) {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(web_contents());
  if (!mediaSession) {
    LOG(ERROR) << "AlloyBrowserHostImpl::SetAudioExclusive get mediaSession failed.";
    return;
  }
  mediaSession->audioExclusive_ = audioExclusive;
}

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
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  platform_delegate_->AddNewContents(source, std::move(new_contents),
                                     target_url, disposition, initial_rect,
                                     user_gesture, was_blocked);
}

void AlloyBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                               bool should_show_loading_ui) {
  contents_delegate_->LoadingStateChanged(source, should_show_loading_ui);
}

void AlloyBrowserHostImpl::CloseContents(content::WebContents* source) {
  CEF_REQUIRE_UIT();

  if (destruction_state_ == DESTRUCTION_STATE_COMPLETED)
    return;

  bool close_browser = true;

  // If this method is called in response to something other than
  // WindowDestroyed() ask the user if the browser should close.
  if (client_.get() && (IsWindowless() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
#if BUILDFLAG(IS_OHOS)
    // |DoClose| will notify the UI to close, |DESTRUCTION_STATE_NONE| means
    // |CloseBrowser| has not been triggered by UI. We should close browser
    // when received |CloseBrowser| request from UI.
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      close_browser = false;
    }
#endif
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED)
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;

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
        source->GetMainFrame()->GetProcess()->FastShutdownIfPossible(1, false);
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
  contents_delegate_->UpdateTargetURL(source, url);
}

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
    if (handler.get())
      handler->OnTakeFocus(this, !reverse);
  }

  return false;
}

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
  return menu_manager_->CreateContextMenu(params);
}

void AlloyBrowserHostImpl::SetBackgroundColor(int color) {
  if (color == base_background_color_) {
    return;
  }

  base_background_color_ = color;
  OnWebPreferencesChanged();
#if BUILDFLAG(IS_OHOS)
  UpdateBackgroundColor(color);
#endif
}

void AlloyBrowserHostImpl::UpdateBackgroundColor(int color) {
  auto rvh = web_contents()->GetRenderViewHost();

  if (rvh->GetWidget()->GetView()) {
    rvh->GetWidget()->GetView()->SetBackgroundColor(color);
  }
}

void AlloyBrowserHostImpl::UpdateZoomSupportEnabled() {
  auto rvh = web_contents()->GetRenderViewHost();
  CefRenderWidgetHostViewOSR* view =
      static_cast<CefRenderWidgetHostViewOSR*>(rvh->GetWidget()->GetView());

  if (view) {
    view->SetDoubleTapSupportEnabled(settings_.supports_double_tap_zoom);
    view->SetMultiTouchZoomSupportEnabled(settings_.supports_multi_touch_zoom);
  }
}

KeyboardEventProcessingResult AlloyBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return contents_delegate_->PreHandleKeyboardEvent(source, event);
}

bool AlloyBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser)
    return false;

  if (contents_delegate_->HandleKeyboardEvent(source, event))
    return true;

  if (platform_delegate_)
    return platform_delegate_->HandleKeyboardEvent(event);
  return false;
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
  if (client_)
    handler = client_->GetDragHandler();
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
      settings, client, platform_delegate, extra_info);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless(), extra_info);
  CHECK(info.get());
  CHECK(info->is_popup());

  CefRefPtr<AlloyBrowserHostImpl> opener =
      GetBrowserForContents(source_contents);
  if (!opener)
    return;

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

void AlloyBrowserHostImpl::DidNavigatePrimaryMainFramePostCommit(
    content::WebContents* web_contents) {
  contents_delegate_->DidNavigatePrimaryMainFramePostCommit(web_contents);
}

content::JavaScriptDialogManager*
AlloyBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  if (!javascript_dialog_manager_.get() && platform_delegate_) {
    javascript_dialog_manager_.reset(new CefJavaScriptDialogManager(
        this, platform_delegate_->CreateJavaScriptDialogRunner()));
  }
  return javascript_dialog_manager_.get();
}

void AlloyBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(listener, params);
}

#if !BUILDFLAG(IS_OHOS)
bool AlloyBrowserHostImpl::HandleContextMenu(
    content::WebContents* web_contents,
#else
bool AlloyBrowserHostImpl::ShowContextMenu(
#endif
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
  if (platform_delegate_)
    platform_delegate_->SizeTo(pref_size.width(), pref_size.height());
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
  CEF_REQUIRE_UIT();

  blink::MediaStreamDevices devices;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  bool microphone_requested =
      (request.audio_type ==
       blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool webcam_requested = (request.video_type ==
                           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
#if BUILDFLAG(IS_OHOS)
  bool screen_requested =
      (request.video_type ==
       blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE);
  LOG(INFO) << "RequestMediaAccessPermission screen_requested: " << screen_requested;
  AlloyPermissionRequestHandler* permission_handler = GetPermissionRequestHandler();
  if (!permission_handler) {
    // Cancel the request.
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  if (!screen_requested && (microphone_requested || webcam_requested)) {
    permission_handler->SendRequest(new AlloyMediaAccessRequest(request, std::move(callback)));
  } else {
    permission_handler->SendScreenCaptureRequest(new AlloyScreenCaptureAccessRequest(request, std::move(callback)));
  }
#else
  bool screen_requested =
      (request.video_type ==
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  if (microphone_requested || webcam_requested || screen_requested) {
    // Pick the desired device or fall back to the first available of the
    // given type.
    if (microphone_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_audio_device_id, true, false, &devices);
    }
    if (webcam_requested) {
      LOG(INFO) << "RequestMediaAccessPermission requested_video_device_id: " <<
        request.requested_video_device_id;
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_video_device_id, false, true, &devices);
    }
    if (screen_requested) {
      content::DesktopMediaID media_id;
      if (request.requested_video_device_id.empty()) {
        media_id =
            content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                    -1 /* webrtc::kFullDesktopScreenId */);
      } else {
        media_id =
            content::DesktopMediaID::Parse(request.requested_video_device_id);
      }
      devices.push_back(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          media_id.ToString(), "Screen"));
    }
  }

  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<content::MediaStreamUI>());
#endif // BUILDFLAG(IS_OHOS)
}

bool AlloyBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  // Check media access permission without prompting the user.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableMediaStream);
}

bool AlloyBrowserHostImpl::IsNeverComposited(
    content::WebContents* web_contents) {
  return platform_delegate_->IsNeverComposited(web_contents);
}

content::PictureInPictureResult AlloyBrowserHostImpl::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  if (!IsPictureInPictureSupported()) {
    return content::PictureInPictureResult::kNotSupported;
  }

  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void AlloyBrowserHostImpl::ExitPictureInPicture() {
  DCHECK(IsPictureInPictureSupported());
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool AlloyBrowserHostImpl::IsBackForwardCacheSupported() {
  // Disabled due to issue #3237.
  return false;
}

bool AlloyBrowserHostImpl::IsPrerender2Supported() {
  return true;
}

void AlloyBrowserHostImpl::RequestToLockMouse(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  if (contents_delegate_)
    contents_delegate_->RequestToLockMouse(web_contents, user_gesture,
                                           last_unlocked_by_target);
}

void AlloyBrowserHostImpl::LostMouseLock() {
  if (contents_delegate_)
    contents_delegate_->LostMouseLock();
}

#if BUILDFLAG(IS_OHOS)
void AlloyBrowserHostImpl::ShowRepostFormWarningDialog(content::WebContents* source) {
  if (contents_delegate_)
    contents_delegate_->ShowRepostFormWarningDialog(source);
}
#endif

#if defined(OHOS_NWEB_EX)
void AlloyBrowserHostImpl::ShowPasswordDialog(bool is_update,
                                              const std::string& url) {
  if (platform_delegate_) {
    platform_delegate_->ShowPasswordDialog(is_update, url);
  }
}

void AlloyBrowserHostImpl::OnShowAutofillPopup(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions) {
  if (platform_delegate_) {
    platform_delegate_->OnShowAutofillPopup(element_bounds, is_rtl,
                                            suggestions);
  }
}

void AlloyBrowserHostImpl::OnHideAutofillPopup() {
  if (platform_delegate_) {
    platform_delegate_->OnHideAutofillPopup();
  }
}
#endif

// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void AlloyBrowserHostImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() == nullptr) {
    auto render_view_host = render_frame_host->GetRenderViewHost();
    new CefWidgetHostInterceptor(this, render_view_host);
    platform_delegate_->RenderViewCreated(render_view_host);
  }
}

void AlloyBrowserHostImpl::RenderViewReady() {
  platform_delegate_->RenderViewReady();
  UpdateBackgroundColor(base_background_color_);
  UpdateZoomSupportEnabled();

#if BUILDFLAG(IS_OHOS)
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ReportWindowStatus, this, true));
    return;
  }
  ReportWindowStatus(true);
#endif
}

void AlloyBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (web_contents()) {
    auto cef_browser_context =
        static_cast<AlloyBrowserContext*>(web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(
          navigation_handle->GetRedirectChain());
    }
  }
}

void AlloyBrowserHostImpl::OnAudioStateChanged(bool audible) {
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "OnAudioStateChanged: " << audible;
  if (client_.get() && client_->GetMediaHandler().get()) {
    client_->GetMediaHandler()->OnAudioStateChanged(this, audible);
  }
#endif  // BUILDFLAG(IS_OHOS)

  if (audible) {
    if (recently_audible_timer_)
      recently_audible_timer_->Stop();

    StartAudioCapturer();
  } else if (audio_capturer_) {
    if (!recently_audible_timer_)
      recently_audible_timer_ = std::make_unique<base::OneShotTimer>();

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

void AlloyBrowserHostImpl::OnFormEditingStateChanged(bool state) {
  LOG(ERROR) << "AlloyBrowserHostImpl::OnFormEditingStateChanged state: " << state;
  if (client_.get() && client_->GetFormHandler().get()) 
    client_->GetFormHandler()->OnFormEditingStateChanged(this, state);
}

void AlloyBrowserHostImpl::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStartedPlaying, is_video: " << video_type.has_video;

  start_play_ = true;
  cef_media_type_t type = video_type.has_video ? cef_media_type_t::VIDEO : cef_media_type_t::AUDIO;
  if (client_.get() && client_->GetMediaHandler().get()) {
    client_->GetMediaHandler()->OnMediaStateChanged(this, type, cef_media_playing_state_t::PLAYING);
  }
}

void AlloyBrowserHostImpl::MediaStoppedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  LOG(INFO) << "AlloyBrowserHostImpl::MediaStoppedPlaying, is_video: " << video_type.has_video << " stopped reason: " << static_cast<int>(reason);
  
  if (!start_play_) {
    return;
  }

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
}

void AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired() {
  audio_capturer_.reset();
}

void AlloyBrowserHostImpl::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& content_event_bundle) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityEventReceived(content_event_bundle);
  }
}

void AlloyBrowserHostImpl::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityLocationChangesReceived(locData);
  }
}

void AlloyBrowserHostImpl::WebContentsDestroyed() {
  auto wc = web_contents();
  content::WebContentsObserver::Observe(nullptr);
  if (platform_delegate_)
    platform_delegate_->WebContentsDestroyed(wc);
}

void AlloyBrowserHostImpl::StartAudioCapturer() {
  if (!client_.get() || audio_capturer_)
    return;

  CefRefPtr<CefAudioHandler> audio_handler = client_->GetAudioHandler();
  if (!audio_handler.get())
    return;

  CefAudioParameters params;
  params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
  params.sample_rate = media::AudioParameters::kAudioCDSampleRate;
  params.frames_per_buffer = 1024;

  if (!audio_handler->GetAudioParameters(this, params))
    return;

  audio_capturer_.reset(new CefAudioCapturer(params, this, audio_handler));
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
  RenderFrameCreated(web_contents->GetMainFrame());
}

bool AlloyBrowserHostImpl::CreateHostWindow() {
  // |host_window_handle_| will not change after initial host creation for
  // non-views-hosted browsers.
  bool success = true;
  if (!IsWindowless())
    success = platform_delegate_->CreateHostWindow();
  if (success && !is_views_hosted_)
    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  return success;
}

gfx::Point AlloyBrowserHostImpl::GetScreenPoint(const gfx::Point& view) const {
  CEF_REQUIRE_UIT();
  if (platform_delegate_)
    return platform_delegate_->GetScreenPoint(view);
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
                                      item_height, item_font_size, selected_item,
                                      std::move(menu_items), right_aligned,
                                      allow_multiple_selection);
  }
}

void AlloyBrowserHostImpl::UpdateDragCursor(
    ui::mojom::DragOperation operation) {
  if (platform_delegate_)
    platform_delegate_->UpdateDragCursor(operation);
}

void AlloyBrowserHostImpl::EnsureFileDialogManager() {
  CEF_REQUIRE_UIT();
  if (!file_dialog_manager_.get() && platform_delegate_) {
    file_dialog_manager_.reset(new CefFileDialogManager(
        this, platform_delegate_->CreateFileDialogRunner()));
  }
}

void AlloyBrowserHostImpl::AddVisitedLinks(const std::vector<CefString>& urls) {
  std::vector<GURL> urlList = std::vector<GURL>();
  for (auto url : urls) {
    urlList.push_back(url_util::MakeGURL(url, /*fixup=*/false));
  }
  if (web_contents()) {
    auto cef_browser_context =
        static_cast<AlloyBrowserContext*>(web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(urlList);
    }
  }
}

#if BUILDFLAG(IS_OHOS)
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

void AlloyBrowserHostImpl::SetWindowId(int window_id, int nweb_id) {
  window_id_ = window_id;
  nweb_id_ = nweb_id;
};

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

void AlloyBrowserHostImpl::SetToken(void* token) {
  if (platform_delegate_) {
    platform_delegate_->SetToken(token);
  }
};

void AlloyBrowserHostImpl::ContentsZoomChange(bool zoom_in) {
  double curFactor = GetZoomLevel();
  double tempZoomFactor = zoom_in ? curFactor + 2.0 : curFactor - 2.0;
  if (tempZoomFactor > 10.0 || tempZoomFactor < -10.0) {
    LOG(ERROR) << "The mouse wheel event can no longer be zoomed in or out.";
    return;
  }
  SetZoomLevel(tempZoomFactor);
}

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
        date_time_dialog_ptr->dialog_value,
        date_time_dialog_ptr->minimum, date_time_dialog_ptr->maximum,
        date_time_dialog_ptr->step);
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
      handler->OnDateTimeChooserPopup(this, chooser,
        suggestions, callback);
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
        << "AlloyBrowserHostImpl::OnRenderViewReady web_contents is null";
    return;
  }

  if (auto render_view_host = contents->GetRenderViewHost()) {
    auto render_process_host = render_view_host->GetProcess();
    if (!render_process_host) {
      LOG(ERROR) << "AlloyBrowserHostImpl::OnRenderViewReady "
                    "render_process_host is null";
      return;
    }

    ResSchedStatusAdapter status = is_hidden_
                                       ? ResSchedStatusAdapter::WEB_INACTIVE
                                       : ResSchedStatusAdapter::WEB_ACTIVE;
    base::ProcessId process_id = render_process_host->GetProcess().Pid();
    ResSchedClientAdapter::ReportWindowStatus(status, process_id, window_id_,
                                              nweb_id_);
    if (!is_hidden_) {
      ResSchedClientAdapter::ReportScene(ResSchedStatusAdapter::WEB_SCENE_ENTER,
                                         ResSchedSceneAdapter::VISIBLE,
                                         nweb_id_);
    }
  } else {
    LOG(ERROR)
        << "AlloyBrowserHostImpl::OnRenderViewReady render_view_host is null";
    return;
  }
}
#endif
