// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/browser_host_base.h"

#include <tuple>

#include "arkweb/build/features/features.h"
#include "base/logging.h"
#include "cef/libcef/browser/browser_guest_util.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/hang_monitor.h"
#include "cef/libcef/browser/image_impl.h"
#include "cef/libcef/browser/navigation_entry_impl.h"
#include "cef/libcef/browser/printing/print_util.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/url_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/favicon/core/favicon_url.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(ARKWEB_MEDIA_AVSESSION)
#include "content/browser/media/session/media_session_impl.h"
#endif  // ARKWEB_MEDIA_AVSESSION

#if BUILDFLAG(IS_MAC)
#include "components/spellcheck/browser/spellcheck_platform.h"
#endif

#if BUILDFLAG(IS_ARKWEB)
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
#include "cc/input/browser_controls_state.h"
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
#include "content/public/common/referrer.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
#include "content/public/browser/navigation_controller.h"
#endif

#if BUILDFLAG(ARKWEB_USERAGENT)
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#endif

#if BUILDFLAG(ARKWEB_JAVASCRIPT_BRIDGE)
#include "cef/ohos_cef_ext/libcef/browser/javascript/oh_javascript_injector.h"
#endif

#if BUILDFLAG(ARKWEB_PERMISSION)
#include "ohos_cef_ext/libcef/browser/permission/alloy_access_request.h"
#include "ohos_cef_ext/libcef/browser/permission/alloy_geolocation_access.h"
#include "ohos_cef_ext/libcef/browser/permission/alloy_access_query.h"
#endif
namespace {

// Associates a CefBrowserHostBase instance with a WebContents. This object will
// be deleted automatically when the WebContents is destroyed.
class WebContentsUserDataAdapter : public base::SupportsUserData::Data {
 public:
  static void Register(CefRefPtr<CefBrowserHostBase> browser) {
    new WebContentsUserDataAdapter(browser);
  }

  static CefRefPtr<CefBrowserHostBase> Get(
      const content::WebContents* web_contents) {
    WebContentsUserDataAdapter* adapter =
        static_cast<WebContentsUserDataAdapter*>(
            web_contents->GetUserData(UserDataKey()));
    if (adapter) {
      return adapter->browser_;
    }
    return nullptr;
  }

 private:
  explicit WebContentsUserDataAdapter(CefRefPtr<CefBrowserHostBase> browser)
      : browser_(browser) {
    auto web_contents = browser->GetWebContents();
    DCHECK(web_contents);
    web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
  }

  static void* UserDataKey() {
    // We just need a unique constant. Use the address of a static that
    // COMDAT folding won't touch in an optimizing linker.
    static int data_key = 0;
    return reinterpret_cast<void*>(&data_key);
  }

  CefRefPtr<CefBrowserHostBase> browser_;
};

}  // namespace

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::FromBrowser(
    CefRefPtr<CefBrowser> browser) {
  return browser.get()->AsCefBrowserHostBase();
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForHost(
    const content::RenderViewHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents = content::WebContents::FromRenderViewHost(
      const_cast<content::RenderViewHost*>(host));
  if (web_contents) {
    return GetBrowserForContents(web_contents);
  }
  return nullptr;
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          const_cast<content::RenderFrameHost*>(host));
  if (web_contents) {
    return GetBrowserForContents(web_contents);
  }
  return nullptr;
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  CEF_REQUIRE_UIT();
  if (auto browser = WebContentsUserDataAdapter::Get(contents)) {
    return browser;
  }

  // Try the owner WebContents if |contents| originates from an excluded view
  // such as the PDF viewer or Print Preview. This is safe to call even if Alloy
  // extensions are disabled.
#if !BUILDFLAG(ARKWEB_PDF)
  if (auto* owner_contents = GetOwnerForGuestContents(contents)) {
    return WebContentsUserDataAdapter::Get(owner_contents);
  }
#endif

  return nullptr;
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  if (!frame_util::IsValidGlobalId(global_id)) {
    return nullptr;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(global_id);
    if (!render_frame_host) {
      return nullptr;
    }
    return GetBrowserForHost(render_frame_host);
  } else {
    // Use the thread-safe approach.
    auto info = CefBrowserInfoManager::GetInstance()->GetBrowserInfo(global_id);
    if (info) {
      auto browser = info->browser();
      if (!browser) {
        LOG(WARNING) << "Found browser id " << info->browser_id()
                     << " but no browser object matching frame "
                     << frame_util::GetFrameDebugString(global_id);
      }
      return browser;
    }
    return nullptr;
  }
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForGlobalToken(
    const content::GlobalRenderFrameHostToken& global_token) {
  if (!frame_util::IsValidGlobalToken(global_token)) {
    return nullptr;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromFrameToken(global_token);
    if (!render_frame_host) {
      return nullptr;
    }
    return GetBrowserForHost(render_frame_host);
  } else {
    // Use the thread-safe approach.
    auto info =
        CefBrowserInfoManager::GetInstance()->GetBrowserInfo(global_token);
    if (info) {
      auto browser = info->browser();
      if (!browser) {
        LOG(WARNING) << "Found browser id " << info->browser_id()
                     << " but no browser object matching frame "
                     << frame_util::GetFrameDebugString(global_token);
      }
      return browser;
    }
    return nullptr;
  }
}

// static
CefRefPtr<CefBrowserHostBase>
CefBrowserHostBase::GetBrowserForTopLevelNativeWindow(
    gfx::NativeWindow owning_window) {
  DCHECK(owning_window);
  CEF_REQUIRE_UIT();

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    if (auto browser = browser_info->browser()) {
      if (browser->GetTopLevelNativeWindow() == owning_window) {
        return browser;
      }
    }
  }

  return nullptr;
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForBrowserId(
    int browser_id) {
  DCHECK_GT(browser_id, 0);

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    if (auto browser = browser_info->browser()) {
      if (browser->GetIdentifier() == browser_id) {
        return browser;
      }
    }
  }

  return nullptr;
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetLikelyFocusedBrowser() {
  CEF_REQUIRE_UIT();

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    if (auto browser = browser_info->browser()) {
      if (browser->IsFocused()) {
        return browser;
      }
    }
  }

  return nullptr;
}

CefBrowserHostBase::CefBrowserHostBase(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefRequestContextImpl> request_context)
    : settings_(settings),
      client_(client),
      platform_delegate_(std::move(platform_delegate)),
      browser_info_(browser_info),
      request_context_(request_context),
      is_views_hosted_(platform_delegate_->IsViewsHosted()),
      contents_delegate_(browser_info_) {
  CEF_REQUIRE_UIT();
  DCHECK(!browser_info_->browser().get());
  browser_info_->SetBrowser(this);

  contents_delegate_.AddObserver(this);
#if BUILDFLAG(ARKWEB_SENSOR) || BUILDFLAG(ARKWEB_PERMISSION)
  if (client) {
    permission_request_handler_.reset(new AlloyPermissionRequestHandler(
        client->AsArkWebClient()->GetPermissionRequest(), GetWebContents()));
  }
#endif
}

void CefBrowserHostBase::InitializeBrowser() {
  CEF_REQUIRE_UIT();

  // Associate the WebContents with this browser object.
  DCHECK(GetWebContents());
  WebContentsUserDataAdapter::Register(this);

#if BUILDFLAG(ARKWEB_FAVICON)
  contents_delegate_.InitIconHelper();
  contents_delegate_.ObserveWebContents(GetWebContents());
#endif

#if BUILDFLAG(ARKWEB_JAVASCRIPT_BRIDGE)
  new NWEB::OhJavascriptInjector(GetWebContents(), client_->AsArkWebClient());
#endif
}

void CefBrowserHostBase::DestroyWebContents(
    content::WebContents* web_contents) {
  CEF_REQUIRE_UIT();

  // GetWebContents() should return nullptr at this point.
  DCHECK(!GetWebContents());

  // Notify that this browser has been destroyed. These must be delivered in
  // the expected order.

  // 1. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserDestroyed().
  platform_delegate_->NotifyBrowserDestroyed();

  // 2. Notify the browser's LifeSpanHandler. This must always be the last
  // notification for this browser.
  OnBeforeClose();

  // Notify any observers that may have state associated with this browser.
  OnBrowserDestroyed();

  // Free objects that may have references to the WebContents.
  devtools_protocol_manager_.reset();
  devtools_window_runner_.reset();
  context_menu_observer_ = nullptr;
  if (javascript_dialog_manager_) {
    javascript_dialog_manager_->Destroy();
    javascript_dialog_manager_.reset();
  }

  browser_info_->WebContentsDestroyed();
  platform_delegate_->WebContentsDestroyed(web_contents);
}

void CefBrowserHostBase::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  // The WebContents should no longer be observed.
  DCHECK(!contents_delegate_.web_contents());

  media_stream_registrar_.reset();

  platform_delegate_.reset();

  contents_delegate_.RemoveObserver(this);

  if (unresponsive_process_callback_) {
    hang_monitor::Detach(unresponsive_process_callback_);
    unresponsive_process_callback_.reset();
  }

  CefBrowserInfoManager::GetInstance()->RemoveBrowserInfo(browser_info_);
  browser_info_->BrowserDestroyed();
}

CefRefPtr<CefBrowser> CefBrowserHostBase::GetBrowser() {
  return this;
}

CefRefPtr<CefClient> CefBrowserHostBase::GetClient() {
  return client_;
}

CefRefPtr<CefRequestContext> CefBrowserHostBase::GetRequestContext() {
  return request_context_;
}

bool CefBrowserHostBase::CanZoom(cef_zoom_command_t command) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (auto web_contents = GetWebContents()) {
    switch (command) {
      case CEF_ZOOM_COMMAND_OUT:
        return chrome::CanZoomOut(web_contents);
      case CEF_ZOOM_COMMAND_RESET:
        return chrome::CanResetZoom(web_contents);
      case CEF_ZOOM_COMMAND_IN:
        return chrome::CanZoomIn(web_contents);
    }
  }

  return false;
}

void CefBrowserHostBase::Zoom(cef_zoom_command_t command) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::Zoom, this, command));
    return;
  }

  if (auto web_contents = GetWebContents()) {
    const content::PageZoom page_zoom = [command]() {
      switch (command) {
        case CEF_ZOOM_COMMAND_OUT:
          return content::PAGE_ZOOM_OUT;
        case CEF_ZOOM_COMMAND_RESET:
          return content::PAGE_ZOOM_RESET;
        case CEF_ZOOM_COMMAND_IN:
          return content::PAGE_ZOOM_IN;
      }
    }();

    // Same implementation as chrome::Zoom(), but explicitly specifying the
    // WebContents.
    zoom::PageZoom::Zoom(web_contents, page_zoom);
  }
}

double CefBrowserHostBase::GetDefaultZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return 0.0;
  }

  if (auto web_contents = GetWebContents()) {
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      return zoom_controller->GetDefaultZoomLevel();
    }
  }

  return 0.0;
}

double CefBrowserHostBase::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return 0.0;
  }

  if (auto web_contents = GetWebContents()) {
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      return zoom_controller->GetZoomLevel();
    }
  }

  return 0.0;
}

void CefBrowserHostBase::SetZoomLevel(double zoomLevel) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::SetZoomLevel,
                                          this, zoomLevel));
    return;
  }

  if (auto web_contents = GetWebContents()) {
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      if (zoomLevel == 0.0) {
        // Same logic as PageZoom::Zoom(PAGE_ZOOM_RESET).
        zoomLevel = zoom_controller->GetDefaultZoomLevel();
        web_contents->SetPageScale(1.f);
      }
      zoom_controller->SetZoomLevel(zoomLevel);
    }
  }
}

bool CefBrowserHostBase::HasView() {
  return is_views_hosted_;
}

bool CefBrowserHostBase::IsReadyToBeClosed() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (auto web_contents = GetWebContents()) {
    return static_cast<content::RenderFrameHostImpl*>(
               web_contents->GetPrimaryMainFrame())
        ->IsPageReadyToBeClosed();
  }
  return true;
}

void CefBrowserHostBase::SetFocus(bool focus) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SetFocus, this, focus));
    return;
  }

  if (focus) {
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  } else if (platform_delegate_) {
    platform_delegate_->SetFocus(false);
  }
}

int CefBrowserHostBase::GetOpenerIdentifier() {
  return opener_id_;
}

void CefBrowserHostBase::RunFileDialog(
    FileDialogMode mode,
    const CefString& title,
    const CefString& default_file_path,
    const std::vector<CefString>& accept_filters,
    CefRefPtr<CefRunFileDialogCallback> callback) {
  DCHECK(callback);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::RunFileDialog,
                                          this, mode, title, default_file_path,
                                          accept_filters, callback));
    return;
  }

  if (!callback || !EnsureFileDialogManager()) {
    LOG(ERROR) << "File dialog canceled due to invalid state.";
    if (callback) {
      callback->OnFileDialogDismissed({});
    }
    return;
  }

  file_dialog_manager_->RunFileDialog(mode, title, default_file_path,
                                      accept_filters, callback);
}

void CefBrowserHostBase::StartDownload(const CefString& url) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostBase::StartDownload, this, url));
    return;
  }

  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid()) {
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager) {
    return;
  }

  std::unique_ptr<download::DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, gurl, MISSING_TRAFFIC_ANNOTATION));
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      gurl, content::Referrer(web_contents->GetLastCommittedURL(),
                              network::mojom::ReferrerPolicy::kDefault));
  params->set_referrer(referrer.url);
#endif
#if BUILDFLAG(ARKWEB_USERAGENT)
  CefString custom_user_agent;
  if (AsAlloyBrowserHostImpl()) {
    custom_user_agent = AsAlloyBrowserHostImpl()->GetCustomUserAgent();
  }
  if (!custom_user_agent.empty()) {
    params->add_request_header(net::HttpRequestHeaders::kUserAgent,
                               custom_user_agent);
  }
#endif
  manager->DownloadUrl(std::move(params));
}

void CefBrowserHostBase::DownloadImage(
    const CefString& image_url,
    bool is_favicon,
    uint32_t max_image_size,
    bool bypass_cache,
    CefRefPtr<CefDownloadImageCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::DownloadImage, this, image_url,
                       is_favicon, max_image_size, bypass_cache, callback));
    return;
  }

  if (!callback) {
    return;
  }

  GURL gurl = GURL(image_url.ToString());
  if (gurl.is_empty() || !gurl.is_valid()) {
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  const float scale = ui::GetScaleForMaxSupportedResourceScaleFactor();
  web_contents->DownloadImage(
      gurl, is_favicon, gfx::Size(max_image_size, max_image_size),
      max_image_size * scale, bypass_cache,
      base::BindOnce(
          [](uint32_t max_image_size,
             CefRefPtr<CefDownloadImageCallback> callback, int id,
             int http_status_code, const GURL& image_url,
             const std::vector<SkBitmap>& bitmaps,
             const std::vector<gfx::Size>& sizes) {
            CEF_REQUIRE_UIT();

            CefRefPtr<CefImageImpl> image_impl;

            if (!bitmaps.empty()) {
              image_impl = new CefImageImpl();
              image_impl->AddBitmaps(max_image_size, bitmaps);
            }

            callback->OnDownloadImageFinished(
                image_url.spec(), http_status_code, image_impl.get());
          },
          max_image_size, callback));
}

void CefBrowserHostBase::Print() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::Print, this));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  const bool print_preview_disabled =
      !platform_delegate_ || !platform_delegate_->IsPrintPreviewSupported();
  print_util::Print(web_contents, print_preview_disabled);
}

void CefBrowserHostBase::PrintToPDF(const CefString& path,
                                    const CefPdfPrintSettings& settings,
                                    CefRefPtr<CefPdfPrintCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::PrintToPDF, this,
                                          path, settings, callback));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  print_util::PrintToPDF(web_contents, path, settings, callback);
}

void CefBrowserHostBase::ShowDevTools(const CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient> client,
                                      const CefBrowserSettings& settings,
                                      const CefPoint& inspect_element_at) {
  auto params = std::make_unique<CefShowDevToolsParams>(
      windowInfo, client, settings, inspect_element_at);

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::ShowDevToolsOnUIThread,
                                 this, std::move(params)));
  } else {
    ShowDevToolsOnUIThread(std::move(params));
  }
}

void CefBrowserHostBase::CloseDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::CloseDevTools, this));
    return;
  }

  if (devtools_window_runner_) {
    devtools_window_runner_->CloseDevTools();
  }
}

bool CefBrowserHostBase::HasDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (devtools_window_runner_) {
    return devtools_window_runner_->HasDevTools();
  }
  return false;
}

void CefBrowserHostBase::StopScreenCapture(int32_t nweb_id,
                                           const CefString& session_id) {
#if BUILDFLAG(ARKWEB_EX_SCREEN_CAPTURE)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  std::string session_id_str = session_id;
  web_contents->StopScreenCapture(nweb_id, session_id_str);
#endif  // defined(ARKWEB_EX_SCREEN_CAPTURE)
}

void CefBrowserHostBase::RegisterScreenCaptureDelegateListener(
    CefRefPtr<CefScreenCaptureCallback> listener) {
#if BUILDFLAG(ARKWEB_EX_SCREEN_CAPTURE)
  if (!listener) {
    LOG(ERROR) << "CefBrowserHostBase::RegisterScreenCaptureDelegateListener "
                  "listener is nullptr";
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }

  content::MediaStreamManager::SetScreenCaptureDelegateCallback(
      base::BindRepeating(
          [](CefRefPtr<CefScreenCaptureCallback> callback, int32_t nweb_id,
             const char* session_id, int32_t code) {
            callback->OnStateChange(nweb_id, CefString(session_id), code);
          },
          listener));
#endif  // defined(ARKWEB_EX_SCREEN_CAPTURE)
}

bool CefBrowserHostBase::SendDevToolsMessage(const void* message,
                                             size_t message_size) {
  if (!message || message_size == 0) {
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    std::string message_str(static_cast<const char*>(message), message_size);
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            [](CefRefPtr<CefBrowserHostBase> self, std::string message_str) {
              self->SendDevToolsMessage(message_str.data(), message_str.size());
            },
            CefRefPtr<CefBrowserHostBase>(this), std::move(message_str)));
    return false;
  }

  if (!EnsureDevToolsProtocolManager()) {
    return false;
  }
  return devtools_protocol_manager_->SendDevToolsMessage(message, message_size);
}

int CefBrowserHostBase::ExecuteDevToolsMethod(
    int message_id,
    const CefString& method,
    CefRefPtr<CefDictionaryValue> params) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(base::IgnoreResult(
                                    &CefBrowserHostBase::ExecuteDevToolsMethod),
                                this, message_id, method, params));
    return 0;
  }

  if (!EnsureDevToolsProtocolManager()) {
    return 0;
  }
  return devtools_protocol_manager_->ExecuteDevToolsMethod(message_id, method,
                                                           params);
}

CefRefPtr<CefRegistration> CefBrowserHostBase::AddDevToolsMessageObserver(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  if (!observer) {
    return nullptr;
  }
  auto registration = CefDevToolsProtocolManager::CreateRegistration(observer);
  InitializeDevToolsRegistrationOnUIThread(registration);
  return registration.get();
}

void CefBrowserHostBase::GetNavigationEntries(
    CefRefPtr<CefNavigationEntryVisitor> visitor,
    bool current_only) {
  DCHECK(visitor.get());
  if (!visitor.get()) {
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostBase::GetNavigationEntries, this,
                                visitor, current_only));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  content::NavigationController& controller = web_contents->GetController();
  const int total = controller.GetEntryCount();
  const int current = controller.GetCurrentEntryIndex();

  if (current_only) {
    // Visit only the current entry.
    CefRefPtr<CefNavigationEntryImpl> entry =
        new CefNavigationEntryImpl(controller.GetEntryAtIndex(current));
    visitor->Visit(entry.get(), true, current, total);
#if !BUILDFLAG(ARKWEB_NAVIGATION)
    std::ignore = entry->Detach(nullptr);
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)
  } else {
    // Visit all entries.
    bool cont = true;
    for (int i = 0; i < total && cont; ++i) {
      CefRefPtr<CefNavigationEntryImpl> entry =
          new CefNavigationEntryImpl(controller.GetEntryAtIndex(i));
      cont = visitor->Visit(entry.get(), (i == current), i, total);
#if !BUILDFLAG(ARKWEB_NAVIGATION)
      std::ignore = entry->Detach(nullptr);
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)
    }
  }
}

void CefBrowserHostBase::EnableVideoAssistant(bool enable) {
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  if (!GetWebContents()) {
    LOG(ERROR) << "failed to get content when enable video assistant";
    return;
  }

  GetWebContents()->EnableVideoAssistant(enable);
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
}

void CefBrowserHostBase::ExecuteVideoAssistantFunction(const CefString& cmdId) {
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  if (!GetWebContents()) {
    LOG(ERROR) << "failed to get content when execute video assistant function";
    return;
  }

  GetWebContents()->ExecuteVideoAssistantFunction(cmdId.ToString());
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
}

CefRefPtr<CefNavigationEntry> CefBrowserHostBase::GetVisibleNavigationEntry() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return nullptr;
  }

  content::NavigationEntry* entry = nullptr;
  auto web_contents = GetWebContents();
  if (web_contents) {
    entry = web_contents->GetController().GetVisibleEntry();
  }

  if (!entry) {
    return nullptr;
  }

  return new CefNavigationEntryImpl(entry);
}

void CefBrowserHostBase::NotifyMoveOrResizeStarted() {
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::NotifyMoveOrResizeStarted, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->NotifyMoveOrResizeStarted();
  }
#endif
}

bool CefBrowserHostBase::IsFullscreen() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (auto web_contents = GetWebContents()) {
    return web_contents->IsFullscreen();
  }
  return false;
}

void CefBrowserHostBase::ExitFullscreen(bool will_cause_resize) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::ExitFullscreen,
                                          this, will_cause_resize));
    return;
  }

  auto web_contents = GetWebContents();
  if (web_contents && web_contents->IsFullscreen()) {
    web_contents->ExitFullscreen(will_cause_resize);
  }
}

bool CefBrowserHostBase::IsRenderProcessUnresponsive() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (auto* web_contents = GetWebContents()) {
    if (auto* rvh = web_contents->GetRenderViewHost()) {
      if (auto* rwh = rvh->GetWidget()) {
        return rwh->IsCurrentlyUnresponsive();
      }
    }
  }
  return false;
}

cef_runtime_style_t CefBrowserHostBase::GetRuntimeStyle() {
  return IsAlloyStyle() ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME;
}

void CefBrowserHostBase::ReplaceMisspelling(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::ReplaceMisspelling, this, word));
    return;
  }

  auto web_contents = GetWebContents();
  if (web_contents) {
    web_contents->ReplaceMisspelling(word);
  }
}

void CefBrowserHostBase::AddWordToDictionary(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::AddWordToDictionary, this, word));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  SpellcheckService* spellcheck = nullptr;
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (browser_context) {
    spellcheck = SpellcheckServiceFactory::GetForContext(browser_context);
    if (spellcheck) {
      spellcheck->GetCustomDictionary()->AddWord(word);
    }
  }
#if BUILDFLAG(IS_MAC)
  if (spellcheck && spellcheck::UseBrowserSpellChecker()) {
    spellcheck_platform::AddWord(spellcheck->platform_spell_checker(), word);
  }
#endif
}

void CefBrowserHostBase::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::SendKeyEvent,
                                          this, event));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendKeyEvent(event);
  }
}

void CefBrowserHostBase::SendMouseClickEvent(const CefMouseEvent& event,
                                             MouseButtonType type,
                                             bool mouseUp,
                                             int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SendMouseClickEvent, this,
                                 event, type, mouseUp, clickCount));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseClickEvent(event, type, mouseUp, clickCount);
  }
}

void CefBrowserHostBase::SendMouseMoveEvent(const CefMouseEvent& event,
                                            bool mouseLeave) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SendMouseMoveEvent, this,
                                 event, mouseLeave));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseMoveEvent(event, mouseLeave);
  }
}

void CefBrowserHostBase::SendMouseWheelEvent(const CefMouseEvent& event,
                                             int deltaX,
                                             int deltaY) {
  if (deltaX == 0 && deltaY == 0) {
    // Nothing to do.
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SendMouseWheelEvent, this,
                                 event, deltaX, deltaY));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseWheelEvent(event, deltaX, deltaY);
  }
}

bool CefBrowserHostBase::IsValid() {
  return browser_info_->IsValid();
}

bool CefBrowserHostBase::CanGoBack() {
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "getWebContents falied, wc is null";
    return false;
  }
  return wc->GetController().CanGoBack();
#else
  return can_go_back_;
#endif
}

void CefBrowserHostBase::GoBack() {
  auto callback = base::BindOnce(&CefBrowserHostBase::GoBack, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc && wc->GetController().CanGoBack()) {
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    wc->CollapseAllFramesSelection();
#endif  // defined(ARKWEB_CLIPBOARD)
    wc->GetController().GoBack();
  }
}

bool CefBrowserHostBase::CanGoForward() {
  base::AutoLock lock_scope(state_lock_);
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "getWebContents falied, wc is null";
    return false;
  }
  return wc->GetController().CanGoForward();
#else
  return can_go_forward_;
#endif
}

void CefBrowserHostBase::GoForward() {
  auto callback = base::BindOnce(&CefBrowserHostBase::GoForward, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc && wc->GetController().CanGoForward()) {
    wc->GetController().GoForward();
  }
}

bool CefBrowserHostBase::IsLoading() {
  base::AutoLock lock_scope(state_lock_);
  return is_loading_;
}

void CefBrowserHostBase::Reload() {
  auto callback = base::BindOnce(&CefBrowserHostBase::Reload, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc) {
    wc->GetController().Reload(content::ReloadType::NORMAL, true);
  }
}

void CefBrowserHostBase::ReloadIgnoreCache() {
  auto callback = base::BindOnce(&CefBrowserHostBase::ReloadIgnoreCache, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc) {
    wc->GetController().Reload(content::ReloadType::BYPASSING_CACHE, true);
  }
}

void CefBrowserHostBase::StopLoad() {
  auto callback = base::BindOnce(&CefBrowserHostBase::StopLoad, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc) {
    wc->Stop();
  }
}

int CefBrowserHostBase::GetIdentifier() {
  return browser_id();
}

bool CefBrowserHostBase::IsSame(CefRefPtr<CefBrowser> that) {
  auto impl = FromBrowser(that);
  return (impl.get() == this);
}

bool CefBrowserHostBase::HasDocument() {
  base::AutoLock lock_scope(state_lock_);
  return has_document_;
}

bool CefBrowserHostBase::IsPopup() {
  return browser_info_->is_popup();
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetMainFrame() {
  return browser_info_->GetMainFrame();
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFocusedFrame() {
  {
    base::AutoLock lock_scope(state_lock_);
    if (focused_frame_) {
      return focused_frame_;
    }
  }

  // The main frame is focused by default.
  return browser_info_->GetMainFrame();
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameByIdentifier(
    const CefString& identifier) {
  const auto& global_token = frame_util::ParseFrameIdentifier(identifier);
  if (!global_token) {
    return nullptr;
  }

  return browser_info_->GetFrameForGlobalToken(*global_token);
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameByName(const CefString& name) {
  for (const auto& frame : browser_info_->GetAllFrames()) {
    if (frame->GetName() == name) {
      return frame;
    }
  }
  return nullptr;
}

size_t CefBrowserHostBase::GetFrameCount() {
  return browser_info_->GetAllFrames().size();
}

void CefBrowserHostBase::GetFrameIdentifiers(
    std::vector<CefString>& identifiers) {
  if (identifiers.size() > 0) {
    identifiers.clear();
  }

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty()) {
    return;
  }

  identifiers.reserve(frames.size());
  for (const auto& frame : frames) {
    identifiers.push_back(frame->GetIdentifier());
  }
}

void CefBrowserHostBase::GetFrameNames(std::vector<CefString>& names) {
  if (names.size() > 0) {
    names.clear();
  }

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty()) {
    return;
  }

  names.reserve(frames.size());
  for (const auto& frame : frames) {
    names.push_back(frame->GetName());
  }
}

void CefBrowserHostBase::SetAccessibilityState(
    cef_state_t accessibility_state) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SetAccessibilityState,
                                 this, accessibility_state));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAccessibilityState(accessibility_state);
  }
}

void CefBrowserHostBase::OnStateChanged(CefBrowserContentsState state_changed) {
  // Make sure that CefBrowser state is consistent before the associated
  // CefClient callback is executed.
  base::AutoLock lock_scope(state_lock_);
  if ((state_changed & CefBrowserContentsState::kNavigation) ==
      CefBrowserContentsState::kNavigation) {
    is_loading_ = contents_delegate_.is_loading();
#if !BUILDFLAG(ARKWEB_NETWORK_BASE)
    can_go_back_ = contents_delegate_.can_go_back();
    can_go_forward_ = contents_delegate_.can_go_forward();
#endif
  }
  if ((state_changed & CefBrowserContentsState::kDocument) ==
      CefBrowserContentsState::kDocument) {
    has_document_ = contents_delegate_.has_document();
  }
  if ((state_changed & CefBrowserContentsState::kFullscreen) ==
      CefBrowserContentsState::kFullscreen) {
    is_fullscreen_ = contents_delegate_.is_fullscreen();
  }
  if ((state_changed & CefBrowserContentsState::kFocusedFrame) ==
      CefBrowserContentsState::kFocusedFrame) {
    focused_frame_ = contents_delegate_.focused_frame();
  }
}

void CefBrowserHostBase::OnWebContentsDestroyed(
    content::WebContents* web_contents) {}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameForHost(
    const content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();
  if (!host) {
    return nullptr;
  }

  return browser_info_->GetFrameForHost(host);
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  return browser_info_->GetFrameForGlobalId(global_id);
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameForGlobalToken(
    const content::GlobalRenderFrameHostToken& global_token) {
  return browser_info_->GetFrameForGlobalToken(global_token);
}

void CefBrowserHostBase::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefBrowserHostBase::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

bool CefBrowserHostBase::HasObserver(Observer* observer) const {
  CEF_REQUIRE_UIT();
  return observers_.HasObserver(observer);
}

void CefBrowserHostBase::LoadMainFrameURL(
    const content::OpenURLParams& params) {
  auto callback =
      base::BindOnce(&CefBrowserHostBase::LoadMainFrameURL, this, params);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (Navigate(params)) {
    OnSetFocus(FOCUS_SOURCE_NAVIGATION);
  }
}

bool CefBrowserHostBase::Navigate(const content::OpenURLParams& params) {
  CEF_REQUIRE_UIT();
  auto web_contents = GetWebContents();
  if (web_contents) {
    GURL gurl = params.url;
    if (!url_util::FixupGURL(gurl)) {
      return false;
    }

#if BUILDFLAG(ARKWEB_POST_URL)
    if (params.post_data) {
      content::NavigationController::LoadURLParams LoadURLParams(params);
      web_contents->GetController().LoadURLWithParams(LoadURLParams);
    } else {
      web_contents->GetController().LoadURL(
          gurl, params.referrer, params.transition, params.extra_headers);
    }
#else
    web_contents->GetController().LoadURL(
        gurl, params.referrer, params.transition, params.extra_headers);
#endif
    return true;
  }
  return false;
}

void CefBrowserHostBase::ShowDevToolsOnUIThread(
    std::unique_ptr<CefShowDevToolsParams> params) {
  CEF_REQUIRE_UIT();
  GetDevToolsWindowRunner()->ShowDevTools(this, std::move(params));
}

void CefBrowserHostBase::ViewText(const std::string& text) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::ViewText, this, text));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ViewText(text);
  }
}

void CefBrowserHostBase::RunFileChooserForBrowser(
    const blink::mojom::FileChooserParams& params,
    CefFileDialogManager::RunFileChooserCallback callback) {
  if (!EnsureFileDialogManager()) {
    LOG(ERROR) << "File dialog canceled due to invalid state.";
    std::move(callback).Run({});
    return;
  }
  file_dialog_manager_->RunFileChooser(params, std::move(callback));
}

void CefBrowserHostBase::RunSelectFile(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
#if BUILDFLAG(ARKWEB_FILE_UPLOAD)
    gfx::NativeWindow owning_window,
    std::vector<std::u16string> accept_types,
    bool use_media_capture) {
#else
    gfx::NativeWindow owning_window) {
#endif
  if (!EnsureFileDialogManager()) {
    LOG(ERROR) << "File dialog canceled due to invalid state.";
    listener->FileSelectionCanceled();
    return;
  }
  file_dialog_manager_->RunSelectFile(listener, std::move(policy), type, title,
                                      default_path, file_types, file_type_index,
#if BUILDFLAG(ARKWEB_FILE_UPLOAD)
                                      default_extension, owning_window,
                                      accept_types, use_media_capture);
#else
                                      default_extension, owning_window);
#endif
}

void CefBrowserHostBase::SelectFileListenerDestroyed(
    ui::SelectFileDialog::Listener* listener) {
  if (file_dialog_manager_) {
    file_dialog_manager_->SelectFileListenerDestroyed(listener);
  }
}

content::JavaScriptDialogManager*
CefBrowserHostBase::GetJavaScriptDialogManager() {
  if (!javascript_dialog_manager_) {
    javascript_dialog_manager_ =
        std::make_unique<CefJavaScriptDialogManager>(this);
  }
  return javascript_dialog_manager_.get();
}

bool CefBrowserHostBase::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    const content::OpenURLParams& params) {
  return true;
}

void CefBrowserHostBase::OnAfterCreated() {
  CEF_REQUIRE_UIT();
  LOG(INFO) << "CefBrowserHostBase::OnAfterCreated, client_:" << client_;
  if (client_) {
    if (auto handler = client_->GetLifeSpanHandler()) {
      handler->OnAfterCreated(this);
    }
  }
}

void CefBrowserHostBase::OnBeforeClose() {
  CEF_REQUIRE_UIT();
  if (client_) {
    if (auto handler = client_->GetLifeSpanHandler()) {
      handler->OnBeforeClose(this);
    }
  }
  browser_info_->SetClosing();
}

void CefBrowserHostBase::OnBrowserDestroyed() {
  CEF_REQUIRE_UIT();

  // Destroy any platform constructs.
  if (file_dialog_manager_) {
    file_dialog_manager_->Destroy();
    file_dialog_manager_.reset();
  }

  for (auto& observer : observers_) {
    observer.OnBrowserDestroyed(this);
  }
}

int CefBrowserHostBase::browser_id() const {
  return browser_info_->browser_id();
}

SkColor CefBrowserHostBase::GetBackgroundColor() const {
  // Don't use |platform_delegate_| because it's not thread-safe.
  return CefContext::Get()->GetBackgroundColor(
      &settings_, IsWindowless() ? STATE_ENABLED : STATE_DISABLED);
}

content::WebContents* CefBrowserHostBase::GetWebContents() const {
  CEF_REQUIRE_UIT();
  return contents_delegate_.web_contents();
}

content::BrowserContext* CefBrowserHostBase::GetBrowserContext() const {
  CEF_REQUIRE_UIT();
  auto web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetBrowserContext();
  }
  return nullptr;
}

CefMediaStreamRegistrar* CefBrowserHostBase::GetMediaStreamRegistrar() {
  CEF_REQUIRE_UIT();
  if (!media_stream_registrar_) {
    media_stream_registrar_ = std::make_unique<CefMediaStreamRegistrar>(this);
  }
  return media_stream_registrar_.get();
}

CefDevToolsWindowRunner* CefBrowserHostBase::GetDevToolsWindowRunner() {
  if (!devtools_window_runner_) {
    devtools_window_runner_ = std::make_unique<CefDevToolsWindowRunner>();
  }
  return devtools_window_runner_.get();
}

views::Widget* CefBrowserHostBase::GetWindowWidget() const {
  CEF_REQUIRE_UIT();
  if (!platform_delegate_) {
    return nullptr;
  }
  return platform_delegate_->GetWindowWidget();
}

CefRefPtr<CefBrowserView> CefBrowserHostBase::GetBrowserView() const {
  CEF_REQUIRE_UIT();
  if (is_views_hosted_ && platform_delegate_) {
    return platform_delegate_->GetBrowserView();
  }
  return nullptr;
}

gfx::NativeWindow CefBrowserHostBase::GetTopLevelNativeWindow() const {
  CEF_REQUIRE_UIT();
  // Windowless browsers always return nullptr from GetTopLevelNativeWindow().
  if (!IsWindowless()) {
    auto web_contents = GetWebContents();
    if (web_contents) {
      return web_contents->GetTopLevelNativeWindow();
    }
  }
  return gfx::NativeWindow();
}

bool CefBrowserHostBase::IsFocused() const {
  CEF_REQUIRE_UIT();
  auto web_contents = GetWebContents();
  if (web_contents) {
    return static_cast<content::RenderFrameHostImpl*>(
               web_contents->GetPrimaryMainFrame())
        ->IsFocused();
  }
  return false;
}

bool CefBrowserHostBase::IsVisible() const {
  CEF_REQUIRE_UIT();
  // Windowless browsers always return nullptr from GetNativeView().
  if (!IsWindowless()) {
    auto web_contents = GetWebContents();
    if (web_contents) {
      return platform_util::IsVisible(web_contents->GetNativeView());
    }
  }
  return false;
}

int CefBrowserHostBase::GetNextPopupId() {
  CEF_REQUIRE_UIT();
  return next_popup_id_++;
}

bool CefBrowserHostBase::EnsureDevToolsProtocolManager() {
  CEF_REQUIRE_UIT();
  if (!contents_delegate_.web_contents()) {
    return false;
  }

  if (!devtools_protocol_manager_) {
    devtools_protocol_manager_ =
        std::make_unique<CefDevToolsProtocolManager>(this);
  }
  return true;
}

void CefBrowserHostBase::InitializeDevToolsRegistrationOnUIThread(
    CefRefPtr<CefRegistration> registration) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefBrowserHostBase::InitializeDevToolsRegistrationOnUIThread, this,
            registration));
    return;
  }

  if (!EnsureDevToolsProtocolManager()) {
    return;
  }
  devtools_protocol_manager_->InitializeRegistrationOnUIThread(registration);
}

bool CefBrowserHostBase::EnsureFileDialogManager() {
  CEF_REQUIRE_UIT();
  if (!contents_delegate_.web_contents()) {
    return false;
  }

  if (!file_dialog_manager_) {
    file_dialog_manager_ = std::make_unique<CefFileDialogManager>(this);
  }
  return true;
}

#if BUILDFLAG(ARKWEB_PERMISSION)

CefRefPtr<CefBrowserPermissionRequestDelegate>
CefBrowserHostBase::GetPermissionRequestDelegate() {
  return this;
}

CefRefPtr<CefGeolocationAcess> CefBrowserHostBase::GetGeolocationPermissions() {
  if (geolocation_permissions_ == nullptr) {
    geolocation_permissions_ = new AlloyGeolocationAccess();
  }
  return geolocation_permissions_;
}

bool CefBrowserHostBase::UseLegacyGeolocationPermissionAPI() {
  return true;
}

void CefBrowserHostBase::AskGeolocationPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  if (UseLegacyGeolocationPermissionAPI()) {
    PopupGeolocationPrompt(origin, std::move(callback));
    return;
  }
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::GEOLOCATION, std::move(callback)));
}

void CefBrowserHostBase::AbortAskGeolocationPermission(
    const CefString& origin) {
  RemoveGeolocationPrompt(origin);
  return;
}

void CefBrowserHostBase::PopupGeolocationPrompt(
    std::string origin,
    cef_permission_callback_t callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool show_prompt = unhandled_geolocation_prompts_.empty();
  unhandled_geolocation_prompts_.emplace_back(origin, std::move(callback));
  if (show_prompt) {
    OnGeolocationShow(origin);
  }
}

void CefBrowserHostBase::OnGeolocationShow(std::string origin) {
  // Reject if geoloaction is disabled, or the origin has a retained deny
  if (!settings_.geolocation_enabled) {
    NotifyGeolocationPermission(false, origin);
    return;
  }

  auto permissions = GetGeolocationPermissions();
  if (permissions->ContainOrigin(origin)) {
    NotifyGeolocationPermission(permissions->IsOriginAccessEnabled(origin),
                                origin);
    return;
  }

  GetClient()->AsArkWebClient()->GetPermissionRequest()->OnGeolocationShow(
      origin);
}

void CefBrowserHostBase::NotifyGeolocationPermission(bool value,
                                                     const CefString& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (unhandled_geolocation_prompts_.empty()) {
    return;
  }
  if (origin == unhandled_geolocation_prompts_.front().first) {
    std::move(unhandled_geolocation_prompts_.front().second).Run(value);
    unhandled_geolocation_prompts_.pop_front();
    if (!unhandled_geolocation_prompts_.empty()) {
      OnGeolocationShow(unhandled_geolocation_prompts_.front().first);
    }
  }
}

void CefBrowserHostBase::RemoveGeolocationPrompt(std::string origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool removed_current_outstanding_callback = false;
  std::list<OriginCallback>::iterator it =
      unhandled_geolocation_prompts_.begin();
  while (it != unhandled_geolocation_prompts_.end()) {
    if ((*it).first == origin) {
      if (it == unhandled_geolocation_prompts_.begin()) {
        removed_current_outstanding_callback = true;
      }
      it = unhandled_geolocation_prompts_.erase(it);
    } else {
      ++it;
    }
  }

  if (removed_current_outstanding_callback) {
    GetClient()->AsArkWebClient()->GetPermissionRequest()->OnGeolocationHide();
    if (!unhandled_geolocation_prompts_.empty()) {
      OnGeolocationShow(unhandled_geolocation_prompts_.front().first);
    }
  }
}
#if BUILDFLAG(ARKWEB_SENSOR)
void CefBrowserHostBase::AskSensorsPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  if (permission_request_handler_) {
    permission_request_handler_->SendSensorRequest(
        new AlloySensorAccessRequest(this, origin, std::move(callback)));
  } else {
    LOG(ERROR) << "AskSensorsPermission, handler is null.";
  }
}

void CefBrowserHostBase::AbortAskSensorsPermission(const CefString& origin) {
  if (permission_request_handler_) {
    permission_request_handler_->CancelRequest(
        origin, AlloyAccessRequest::Resources::SENSORS);
  } else {
    LOG(ERROR) << "AbortAskSensorsPermission, handler is null.";
  }
}
#endif  // #if BUILDFLAG(ARKWEB_SENSOR)

#if BUILDFLAG(ARKWEB_NOTIFICATION)
void CefBrowserHostBase::AskNotificationPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  if (permission_request_handler_) {
    permission_request_handler_->SendRequest(new AlloyAccessRequest(
        origin, AlloyAccessRequest::Resources::NOTIFICATION,
        std::move(callback)));
  } else {
    LOG(ERROR) << "AskNotificationPermission, handler is null.";
  }
}
 
void CefBrowserHostBase::AbortAskNotificationPermission(
    const CefString& origin) {
  if (permission_request_handler_) {
    permission_request_handler_->CancelRequest(
        origin, AlloyAccessRequest::Resources::NOTIFICATION);
  } else {
    LOG(ERROR) << "AbortAskNotificationPermission, handler is null.";
  }
}

void CefBrowserHostBase::GetPermissionStatusAsync(
    const CefString& origin,
    cef_permission_status_query_callback_t callback) {
  CefPermissionQuery::GetPermissionStatusAsync(
      new AlloyAccessQuery(origin, AlloyAccessRequest::Resources::NOTIFICATION,
                           std::move(callback)));
}

#endif // #if BUILDFLAG(ARKWEB_NOTIFICATION)

void CefBrowserHostBase::AskMIDISysexPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::MIDI_SYSEX, std::move(callback)));
}

void CefBrowserHostBase::AbortAskMIDISysexPermission(const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::MIDI_SYSEX);
}

void CefBrowserHostBase::AskClipboardReadWritePermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::CLIPBOARD_READ_WRITE,
      std::move(callback)));
}

void CefBrowserHostBase::AbortAskClipboardReadWritePermission(
    const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::CLIPBOARD_READ_WRITE);
}

void CefBrowserHostBase::AskClipboardSanitizedWritePermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::CLIPBOARD_SANITIZED_WRITE,
      std::move(callback)));
}

void CefBrowserHostBase::AbortAskClipboardSanitizedWritePermission(
    const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::CLIPBOARD_SANITIZED_WRITE);
}

void CefBrowserHostBase::AskAudioCapturePermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::AUDIO_CAPTURE,
      std::move(callback)));
}

void CefBrowserHostBase::AbortAskAudioCapturePermission(
    const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::AUDIO_CAPTURE);
}

void CefBrowserHostBase::AskVideoCapturePermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::VIDEO_CAPTURE,
      std::move(callback)));
}

void CefBrowserHostBase::AbortAskVideoCapturePermission(
    const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::VIDEO_CAPTURE);
}
#endif  // BUILDFLAG(ARKWEB_PERMISSION)

#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
bool CefBrowserHostBase::NeedToFireBeforeUnloadOrUnloadEvents() {
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->NeedToFireBeforeUnloadOrUnloadEvents();
}

void CefBrowserHostBase::DispatchBeforeUnload() {
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->DispatchBeforeUnload(false);
}
#endif  // ARKWEB_DISATCH_BEFORE_UNLOAD
