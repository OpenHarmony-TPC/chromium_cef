// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_base.h"

#include <tuple>

#include "base/logging.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/favicon/core/favicon_url.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_entry.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/navigation_entry_impl.h"
#include "libcef/browser/printing/print_util.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(IS_MAC)
#include "components/spellcheck/browser/spellcheck_platform.h"
#endif

#if BUILDFLAG(IS_OHOS)
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "cef/include/cef_parser.h"
#include "chrome/browser/browser_process.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "gpu/ipc/common/nweb_native_window_tracker.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/referrer.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/javascript/oh_javascript_injector.h"
#include "libcef/browser/navigation_state_serializer.h"
#include "libcef/browser/permission/alloy_access_request.h"
#include "libcef/browser/permission/alloy_geolocation_access.h"
#include "net/base/mime_util.h"
#include "ohos_adapter_helper.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(OHOS_MEDIA_POLICY)
#include "content/browser/media/session/media_session_impl.h"
#endif // defined(OHOS_MEDIA_POLICY)

#if defined(OHOS_MEDIA_POLICY)
#include "content/browser/media/session/media_session_impl.h"
#endif // defined(OHOS_MEDIA_POLICY)

#if defined(OHOS_EX_UA)
#include "content/public/common/content_switches.h"
#endif

#if defined(OHOS_INPUT_EVENTS)
#include <chrono>
#endif  // defined(OHOS_INPUT_EVENTS)

#if defined(OHOS_PASSWORD_AUTOFILL)
#include "libcef/browser/password/oh_password_manager_client.h"
#endif

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
#include "libcef/browser/autofill/oh_autofill_client.h"
#endif  // defined(OHOS_EX_PASSWORD)

#if defined(OHOS_NO_STATE_PREFETCH)
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#endif  // defined(OHOS_NO_STATE_PREFETCH)

#if defined(OHOS_EX_DOWNLOAD)
#include "components/download/public/common/download_item_impl.h"
#include "libcef/browser/download_item_impl.h"
#include "libcef/browser/received_slice_helper.h"
#endif

#if defined(OHOS_EX_TOPCONTROLS)
#include "cc/input/browser_controls_state.h"
#endif

#if defined(OHOS_EX_NAVIGATION)
#include "content/public/browser/navigation_controller.h"
#endif

#ifdef OHOS_ITP
#include "cef/libcef/browser/anti_tracking/third_party_cookie_access_policy.h"
#endif

#ifdef OHOS_I18N
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/base/ui_base_switches.h"
#endif

#ifdef OHOS_ARKWEB_ADBLOCK
#include "libcef/browser/subresource_filter/adblock_list.h"
#include "cef/libcef/browser/adblock/adblock_config_bridge.h"
#include "components/subresource_filter/content/browser/ohos_adblock_config.h"
#endif

#ifdef OHOS_URL_TRUST_LIST
#include "libcef/browser/ohos_safe_browsing/ohos_url_trust_list_manager.h"
#endif

#include "content/browser/renderer_host/media/media_stream_manager.h"

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "libcef/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#endif

#ifdef OHOS_NOTIFICATION
#include "libcef/browser/permission/alloy_access_query.h"
#endif

namespace {

#if defined(OHOS_INPUT_EVENTS)
static float DEFAULT_MIN_ZOOM_FACTOR = 0.01f;
static float DEFAULT_MAX_ZOOM_FACTOR = 100.0f;

enum class WebScrollType : int {
    UNKNOWN = -1,
    EVENT = 0,
};
#endif  // defined(OHOS_INPUT_EVENTS)

#if defined(OHOS_EX_DOWNLOAD)
const char kNWebId[] = "nweb_id";
#endif

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
  WebContentsUserDataAdapter(CefRefPtr<CefBrowserHostBase> browser)
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

#if BUILDFLAG(IS_OHOS)
const std::string WEB_ARCHIVE_EXTENSION = ".mht";

CefString GenerateArchiveAutoNamePath(const GURL& url,
                                      const CefString& base_name) {
  if (!url.is_empty()) {
    std::string file_name = url.ExtractFileName();
    if (file_name.empty()) {
      file_name = "index";
    }

    std::string test_name =
        base_name.ToString() + file_name + WEB_ARCHIVE_EXTENSION;

    if (!base::PathExists(base::FilePath(test_name))) {
      return test_name;
    }

    for (int i = 0; i < 100; ++i) {
      test_name = base_name.ToString() + file_name + "-" +
                  base::NumberToString(i) + WEB_ARCHIVE_EXTENSION;
      if (!base::PathExists(base::FilePath(test_name))) {
        return test_name;
      }
    }
  }
  return "";
}
#endif  // IS_OHOS

}  // namespace

#if BUILDFLAG(IS_OHOS)
using namespace NWEB;
#endif

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
  return WebContentsUserDataAdapter::Get(contents);
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
    bool is_guest_view = false;
    auto info = CefBrowserInfoManager::GetInstance()->GetBrowserInfo(
        global_id, &is_guest_view);
    if (info && !is_guest_view) {
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
#if BUILDFLAG(IS_OHOS)
      is_views_hosted_(platform_delegate_->IsViewsHosted()),
      weak_ptr_factory_(this) {
#else
      is_views_hosted_(platform_delegate_->IsViewsHosted()) {
#endif
  CEF_REQUIRE_UIT();
  DCHECK(!browser_info_->browser().get());
  browser_info_->SetBrowser(this);
  contents_delegate_ =
      std::make_unique<CefBrowserContentsDelegate>(browser_info_);
  contents_delegate_->AddObserver(this);
#if BUILDFLAG(IS_OHOS)
  contents_delegate_->InitIconHelper();
  if (client) {
    permission_request_handler_.reset(new AlloyPermissionRequestHandler(
      client->GetPermissionRequest(), GetWebContents()));
  }
#endif
}

void CefBrowserHostBase::InitializeBrowser() {
  CEF_REQUIRE_UIT();

  // Associate the WebContents with this browser object.
  DCHECK(GetWebContents());
  WebContentsUserDataAdapter::Register(this);

#if BUILDFLAG(IS_OHOS)
  new OhJavascriptInjector(GetWebContents(), client_);
#endif
}

void CefBrowserHostBase::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  devtools_manager_.reset();
  media_stream_registrar_.reset();

  platform_delegate_.reset();

  contents_delegate_->RemoveObserver(this);
  contents_delegate_->ObserveWebContents(nullptr);

  CefBrowserInfoManager::GetInstance()->RemoveBrowserInfo(browser_info_);
  browser_info_->SetBrowser(nullptr);
}

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::PostTaskToUIThread(CefRefPtr<CefTask> task) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::PostTaskToUIThread, this, task));
    return;
  }
  std::move(task)->Execute();
}
#endif

CefRefPtr<CefBrowser> CefBrowserHostBase::GetBrowser() {
  return this;
}

CefRefPtr<CefClient> CefBrowserHostBase::GetClient() {
  return client_;
}

CefRefPtr<CefRequestContext> CefBrowserHostBase::GetRequestContext() {
  return request_context_;
}

bool CefBrowserHostBase::HasView() {
  return is_views_hosted_;
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

void CefBrowserHostBase::StartDownload(const CefString& url) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostBase::StartDownload, this, url));
    return;
  }

  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid())
    return;

  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return;

  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager)
    return;

  std::unique_ptr<download::DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, gurl, MISSING_TRAFFIC_ANNOTATION));
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      gurl, content::Referrer(web_contents->GetLastCommittedURL(),
                             network::mojom::ReferrerPolicy::kDefault));
  params->set_referrer(referrer.url);
#if defined(OHOS_USERAGENT)
  if (!custom_user_agent_.empty()) {
    params->add_request_header(net::HttpRequestHeaders::kUserAgent, custom_user_agent_);
  }
#endif
  manager->DownloadUrl(std::move(params));
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

void CefBrowserHostBase::ResumeDownload(
    const CefString& url,
    const CefString& full_path,
    int64 received_bytes,
    int64 total_bytes,
    const CefString& etag,
    const CefString& mime_type,
    const CefString& last_modified,
    const CefString& received_slices_string) {
#if defined(OHOS_EX_DOWNLOAD)
  LOG(DEBUG) << "CefBrowserHostBase::ResumeDownload";
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::ResumeDownload, this, url,
                       full_path, received_bytes, total_bytes, etag, mime_type,
                       last_modified, received_slices_string));
    return;
  }

  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid())
    return;

  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return;

  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager)
    return;
  base::FilePath file_full_path = base::FilePath::FromUTF8Unsafe(
      full_path.ToString());  // FILE_PATH_LITERAL()

  std::vector<download::DownloadItem::ReceivedSlice> received_slices =
      received_slice_helper::FromString(received_slices_string.ToString());
  manager->GetNextId(base::BindOnce(
      &CefBrowserHostBase::ResumeDownloadWithId, weak_ptr_factory_.GetWeakPtr(),
      std::move(gurl), std::move(file_full_path), received_bytes, total_bytes,
      etag, mime_type, last_modified, received_slices));
#endif
}

void CefBrowserHostBase::ResumeDownloadWithId(
    const GURL& gurl,
    const base::FilePath& full_path,
    int64 received_bytes,
    int64 total_bytes,
    const std::string& etag,
    const std::string& mime_type,
    const std::string& last_modified,
    std::vector<download::DownloadItem::ReceivedSlice> received_slices,
    uint32_t next_id) {
#if defined(OHOS_EX_DOWNLOAD)
  LOG(DEBUG) << "CefBrowserHostBase::ResumeDownloadWithId url:" << gurl.spec();
  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return;

  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager)
    return;
  std::vector<GURL> url_chain;
  url_chain.push_back(gurl);
  auto download_item = manager->CreateDownloadItem(
      base::GenerateGUID(),                         /*guid*/
      next_id,                                      /*id*/
      full_path,                                    /*current_path*/
      full_path,                                    /*target_path*/
      url_chain,                                    /*url_chain*/
      GURL(),                                       /*referrer_url*/
      content::StoragePartitionConfig(),            /*storage_partition_config*/
      GURL(),                                       /*tab_url*/
      GURL(),                                       /*tab_referrer_url*/
      url::Origin(),                                /*request_initiator*/
      mime_type,                                    /*mime_type*/
      mime_type,                                    /*original_mime_type*/
      base::Time::Now(),                            /*start_time*/
      base::Time::Now(),                            /*end_time*/
      etag,                                         /*etag*/
      last_modified,                                /*last_modified*/
      received_bytes,                               /*received_bytes*/
      total_bytes,                                  /*total_bytes*/
      std::string(),                                /*hash*/
      download::DownloadItem::INTERRUPTED,          /*state*/
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, /*danger_type*/
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, /*interrupt_reason*/
      false,                                              /*opened*/
      base::Time(),                                       /*last_access_time*/
      false,                                              /*transient*/
      received_slices                                     /*received_slices*/
  );
  if (download_item) {
    auto browser = GetBrowser();
    if (browser) {
      int nweb_id = browser->GetNWebId();
      download_item->SetUserData(
          kNWebId,
          std::make_unique<download::DownloadItemImpl::NWebIdData>(nweb_id));
      download_item->Resume(true /*is_user_resume*/);
      return;
    }
  }
#endif  //  OHOS_EX_DOWNLOAD
}

#if defined(OHOS_EX_DOWNLOAD)
CefRefPtr<CefDownloadItem> CefBrowserHostBase::GetDownloadItem(uint32 item_id) {
  LOG(DEBUG) << "CefBrowserHostBase::GetDownloadItem item_id: " << item_id;
  auto web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return nullptr;

  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager)
    return nullptr;

  download::DownloadItem* item = manager->GetDownload(item_id);
  CefRefPtr<CefDownloadItemImpl> download_item(
    new CefDownloadItemImpl(item));
  return download_item.get();
}
#endif

void CefBrowserHostBase::DownloadImage(
    const CefString& image_url,
    bool is_favicon,
    uint32 max_image_size,
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

  web_contents->DownloadImage(
      gurl, is_favicon, gfx::Size(max_image_size, max_image_size),
      max_image_size * gfx::ImageSkia::GetMaxSupportedScale(), bypass_cache,
      base::BindOnce(
          [](uint32 max_image_size,
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

void CefBrowserHostBase::CreateToPDF(const CefPdfPrintSettings& settings,
                                     CefRefPtr<CefPdfValueCallback> callback) {
  auto web_contents = GetWebContents();
  if (!web_contents || !callback) {
    LOG(ERROR) << "CefBrowserHostBase::CreateToPDF "
                  "callback is nullptr or web_contents is null";
    return;
  }

  print_util::CreateToPDF(web_contents, settings, callback);
}

void CefBrowserHostBase::StopScreenCapture(int32_t nweb_id, const CefString& session_id) {
#if defined(OHOS_EX_SCREEN_CAPTURE)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  std::string session_id_str = session_id;
  web_contents->StopScreenCapture(nweb_id, session_id_str);
#endif // defined(OHOS_EX_SCREEN_CAPTURE)
}

void CefBrowserHostBase::RegisterScreenCaptureDelegateListener(
    CefRefPtr<CefScreenCaptureCallback> listener) {
#if defined(OHOS_EX_SCREEN_CAPTURE)
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
          [](CefRefPtr<CefScreenCaptureCallback> callback,
             int32_t nweb_id, const char* session_id, int32_t code) {
            callback->OnStateChange(nweb_id, CefString(session_id), code);
          },
          listener));
#endif // defined(OHOS_EX_SCREEN_CAPTURE)
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

  if (!EnsureDevToolsManager()) {
    return false;
  }
  return devtools_manager_->SendDevToolsMessage(message, message_size);
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

  if (!EnsureDevToolsManager()) {
    return 0;
  }
  return devtools_manager_->ExecuteDevToolsMethod(message_id, method, params);
}

CefRefPtr<CefRegistration> CefBrowserHostBase::AddDevToolsMessageObserver(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  if (!observer) {
    return nullptr;
  }
  auto registration = CefDevToolsManager::CreateRegistration(observer);
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
#ifndef OHOS_NAVIGATION
    std::ignore = entry->Detach(nullptr);
#endif
  } else {
    // Visit all entries.
    bool cont = true;
    for (int i = 0; i < total && cont; ++i) {
      CefRefPtr<CefNavigationEntryImpl> entry =
          new CefNavigationEntryImpl(controller.GetEntryAtIndex(i));
      cont = visitor->Visit(entry.get(), (i == current), i, total);
#ifndef OHOS_NAVIGATION
      std::ignore = entry->Detach(nullptr);
#endif
    }
  }
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

#if BUILDFLAG(IS_OHOS)
// copy settings string params
#define SETTINGS_STRING_SET(src, target) \
  cef_string_set(src.str, src.length, &target, true)

void CefBrowserHostBase::UpdateBrowserSettings(
    const CefBrowserSettings& browser_settings) {
  /* font family */
  SETTINGS_STRING_SET(browser_settings.standard_font_family,
                      settings_.standard_font_family);
  SETTINGS_STRING_SET(browser_settings.fixed_font_family,
                      settings_.fixed_font_family);
  SETTINGS_STRING_SET(browser_settings.serif_font_family,
                      settings_.serif_font_family);
  SETTINGS_STRING_SET(browser_settings.sans_serif_font_family,
                      settings_.sans_serif_font_family);
  SETTINGS_STRING_SET(browser_settings.cursive_font_family,
                      settings_.cursive_font_family);
  SETTINGS_STRING_SET(browser_settings.fantasy_font_family,
                      settings_.fantasy_font_family);

  /* font size*/
  settings_.default_font_size = browser_settings.default_font_size;
  settings_.default_fixed_font_size = browser_settings.default_fixed_font_size;
  settings_.minimum_font_size = browser_settings.minimum_font_size;
  settings_.minimum_logical_font_size =
      browser_settings.minimum_logical_font_size;
  SETTINGS_STRING_SET(browser_settings.default_encoding,
                      settings_.default_encoding);
  settings_.javascript = browser_settings.javascript;
  settings_.image_loading = browser_settings.image_loading;
  settings_.local_storage = browser_settings.local_storage;
  settings_.databases = browser_settings.databases;

  /* ohos webview add*/
  settings_.force_dark_mode_enabled = browser_settings.force_dark_mode_enabled;
#if defined(OHOS_DARKMODE)
  settings_.dark_prefer_color_scheme_enabled =
      browser_settings.dark_prefer_color_scheme_enabled;
#endif
  settings_.javascript_can_open_windows_automatically =
      browser_settings.javascript_can_open_windows_automatically;
  settings_.loads_images_automatically =
      browser_settings.loads_images_automatically;
  settings_.text_size_percent = browser_settings.text_size_percent;
  settings_.allow_running_insecure_content =
      browser_settings.allow_running_insecure_content;
  settings_.strict_mixed_content_checking =
      browser_settings.strict_mixed_content_checking;
  settings_.allow_mixed_content_upgrades =
      browser_settings.allow_mixed_content_upgrades;
  settings_.geolocation_enabled = browser_settings.geolocation_enabled;
  settings_.supports_double_tap_zoom =
      browser_settings.supports_double_tap_zoom;
  settings_.supports_multi_touch_zoom =
      browser_settings.supports_multi_touch_zoom;
  settings_.initialize_at_minimum_page_scale =
      browser_settings.initialize_at_minimum_page_scale;
  settings_.viewport_meta_enabled = browser_settings.viewport_meta_enabled;
  settings_.user_gesture_required = browser_settings.user_gesture_required;
  settings_.pinch_smooth_mode = browser_settings.pinch_smooth_mode;
#if defined(OHOS_INPUT_EVENTS)
  settings_.hide_vertical_scrollbars =
      browser_settings.hide_vertical_scrollbars;
  settings_.hide_horizontal_scrollbars =
      browser_settings.hide_horizontal_scrollbars;
  settings_.scroll_enabled = browser_settings.scroll_enabled;
  settings_.blur_enabled = browser_settings.blur_enabled;
#endif  // defined(OHOS_INPUT_EVENTS)
#if BUILDFLAG(IS_OHOS)
  settings_.native_embed_mode_enabled =
      browser_settings.native_embed_mode_enabled;
  settings_.intrinsic_size_enabled =
      browser_settings.intrinsic_size_enabled;
  SETTINGS_STRING_SET(browser_settings.embed_tag, settings_.embed_tag);
  SETTINGS_STRING_SET(browser_settings.embed_tag_type,
                      settings_.embed_tag_type);
  settings_.draw_mode = browser_settings.draw_mode;
  settings_.text_autosizing_enabled = browser_settings.text_autosizing_enabled;
  settings_.force_zero_layout_height = browser_settings.force_zero_layout_height;
#endif  // BUILDFLAG(IS_OHOS)
#ifdef OHOS_SCROLLBAR
  settings_.scrollbar_color = browser_settings.scrollbar_color;
#endif  // OHOS_SCROLLBAR

#ifdef OHOS_EX_FREE_COPY
  settings_.contextmenu_customization_enabled =
      browser_settings.contextmenu_customization_enabled;
#endif

#if defined(OHOS_CLIPBOARD)
  settings_.copy_option = browser_settings.copy_option;
#endif  // defined(OHOS_CLIPBOARD)
#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
  settings_.custom_video_player_enable =
      browser_settings.custom_video_player_enable;
  settings_.custom_video_player_overlay =
      browser_settings.custom_video_player_overlay;
#endif // OHOS_CUSTOM_VIDEO_PLAYER
#if defined(OHOS_MULTI_WINDOW)
  settings_.supports_multiple_windows = browser_settings.supports_multiple_windows;
#endif // OHOS_MULTI_WINDOW

#if defined(OHOS_SOFTWARE_COMPOSITOR)
  settings_.record_whole_document = browser_settings.record_whole_document;
#endif

#ifdef OHOS_NETWORK_LOAD
  settings_.universal_access_from_file_urls = browser_settings.universal_access_from_file_urls;
#endif

#ifdef OHOS_MEDIA_NETWORK_TRAFFIC_PROMPT
  settings_.enable_media_network_traffic_prompt =
      browser_settings.enable_media_network_traffic_prompt;
#endif // OHOS_MEDIA_NETWORK_TRAFFIC_PROMPT
}

void CefBrowserHostBase::SetWebPreferences(
    const CefBrowserSettings& browser_settings) {
  UpdateBrowserSettings(browser_settings);
  GetWebContents()->OnWebPreferencesChanged();
}

void CefBrowserHostBase::OnWebPreferencesChanged() {
  GetWebContents()->OnWebPreferencesChanged();
}

void CefBrowserHostBase::PutUserAgent(const CefString& ua) {
  if (!GetWebContents()) {
    return;
  }

#if defined(OHOS_USERAGENT)
  custom_user_agent_ = ua;
#endif

#if defined(OHOS_EX_UA)
  std::string user_agent = ua;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kForBrowser) &&
      (ua.empty() || ua.length() == 0)) {
    user_agent = DefaultUserAgent();
  }
  GetWebContents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(user_agent), true);
#else
  GetWebContents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(ua), true);
#endif

  content::NavigationController& controller = GetWebContents()->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(true);
  }
}

CefString CefBrowserHostBase::DefaultUserAgent() {
  return embedder_support::GetUserAgent();
}

void CefBrowserHostBase::RegisterNativeJSProxy(
    const CefString& object_name,
    const std::vector<CefString>& method_list,
    const int32_t object_id,
    bool is_async,
    const CefString& permission) {
  OhJavascriptInjector* javascriptInjector =
      OhJavascriptInjector::FromWebContents(GetWebContents());
  if (!javascriptInjector) {
    LOG(ERROR) << "CefBrowserHostBase::RegisterArkJSfunction "
                  "javascriptInjector is null";
    return;
  }
  std::vector<std::string> method_vector;
  for (CefString method : method_list) {
    method_vector.push_back(method.ToString());
  }
  javascriptInjector->AddNativeInterface(object_name.ToString(), method_vector,
                                   object_id, is_async, permission.ToString());
}

void CefBrowserHostBase::RegisterArkJSfunction(
    const CefString& object_name,
    const std::vector<CefString>& method_list,
    const std::vector<CefString>& async_method_list,
    const int32_t object_id,
    const CefString& permission) {
  OhJavascriptInjector* javascriptInjector =
      OhJavascriptInjector::FromWebContents(GetWebContents());
  if (!javascriptInjector) {
    LOG(ERROR) << "CefBrowserHostBase::RegisterArkJSfunction "
                  "javascriptInjector is null";
    return;
  }
  std::vector<std::string> method_vector;
  for (CefString method : method_list) {
    method_vector.push_back(method.ToString());
  }
  std::vector<std::string> async_method_vector;
  for (CefString async_method : async_method_list) {
    async_method_vector.push_back(async_method.ToString());
  }
  javascriptInjector->AddInterface(object_name.ToString(), method_vector,
                                   async_method_vector, object_id, permission.ToString());
}

void CefBrowserHostBase::UnregisterArkJSfunction(
    const CefString& object_name,
    const std::vector<CefString>& method_list) {
  OhJavascriptInjector* javascriptInjector =
      OhJavascriptInjector::FromWebContents(GetWebContents());
  std::vector<std::string> method_vector;
  for (CefString method : method_list) {
    method_vector.push_back(method.ToString());
  }
  javascriptInjector->RemoveInterface(object_name.ToString(), method_vector);
}

#if defined(OHOS_JSPROXY)
js_injection::JsCommunicationHost*
CefBrowserHostBase::GetJsCommunicationHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!js_communication_host_.get()) {
    js_communication_host_ =
        std::make_unique<js_injection::JsCommunicationHost>(GetWebContents());
  }
  return js_communication_host_.get();
}

void CefBrowserHostBase::JavaScriptOnDocumentStart(
    const CefString& script,
    const std::vector<CefString>& script_rules) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    std::string stdScript = script.ToString();
    std::vector<std::string> scriptRules;
    for (CefString rule : script_rules) {
      scriptRules.push_back(rule.ToString());
    }
    js_injection::JsCommunicationHost::AddScriptResult result =
        host->AddDocumentStartJavaScript(script, scriptRules);
    if (result.script_id.has_value()) {
      document_start_script_result_map_[stdScript] = result.script_id.value();
    }
  }
}

void CefBrowserHostBase::RemoveJavaScriptOnDocumentStart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    for (auto iter : document_start_script_result_map_) {
      host->RemoveDocumentStartJavaScript(iter.second);
    }
  }
}

void CefBrowserHostBase::JavaScriptOnDocumentEnd(
    const CefString& script,
    const std::vector<CefString>& script_rules) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    std::string stdScript = script.ToString();
    std::vector<std::string> scriptRules;
    for (CefString rule : script_rules) {
      scriptRules.push_back(rule.ToString());
    }
    js_injection::JsCommunicationHost::AddScriptResult result =
        host->AddDocumentEndJavaScript(script, scriptRules);
    if (result.script_id.has_value()) {
      document_end_script_result_map_[stdScript] = result.script_id.value();
    }
  }
}

void CefBrowserHostBase::RemoveJavaScriptOnDocumentEnd() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    for (auto iter : document_end_script_result_map_) {
      host->RemoveDocumentEndJavaScript(iter.second);
    }
  }
}

void CefBrowserHostBase::JavaScriptOnHeadReady(
    const CefString& script,
    const std::vector<CefString>& script_rules) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    std::string stdScript = script.ToString();
    std::vector<std::string> scriptRules;
    for (CefString rule : script_rules) {
      scriptRules.push_back(rule.ToString());
    }

    js_injection::JsCommunicationHost::AddScriptResult result =
        host->AddHeadReadyJavaScript(script, scriptRules);
    if (result.script_id.has_value()) {
      head_ready_script_result_map_[stdScript] = result.script_id.value();
    }
  }
}

void CefBrowserHostBase::RemoveJavaScriptOnHeadReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host = GetJsCommunicationHost();
  if (host) {
    for (auto iter : head_ready_script_result_map_) {
      host->RemoveHeadReadyJavaScript(iter.second);
    }
  }
}
#endif

void CefBrowserHostBase::CallH5Function(
    int32_t routing_id,
    int32_t h5_object_id,
    const CefString& h5_method_name,
    const std::vector<CefRefPtr<CefValue>>& args) {
  OhJavascriptInjector* javascriptInjector =
      OhJavascriptInjector::FromWebContents(GetWebContents());
  if (!javascriptInjector || h5_object_id <= 0) {
    LOG(ERROR) << "CefBrowserHostBase::GetH5ObjectMethods "
                  "javascriptInjector is null or h5_object_id = "
               << h5_object_id;
    return;
  }

  std::string name = h5_method_name.ToString();
  javascriptInjector->DoCallH5Function(routing_id, h5_object_id, name, args);
}
#endif

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

#if defined(OHOS_INPUT_EVENTS)
  if (mouseUp)
    return;

  float ratio = GetVirtualPixelRatio();
  if (ratio <= 0) {
    LOG(ERROR) << "get ratio invalid: " << ratio;
    return;
  }
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->SendHitEvent(event.x * ratio, event.y * ratio, 0, 0);
  }
#endif  // defined(OHOS_INPUT_EVENTS)
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

void CefBrowserHostBase::SendTouchpadFlingEvent(const CefMouseEvent& event, double vx, double vy) {
  if (vx == 0 && vy == 0) {
    // Nothing to do.
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::SendTouchpadFlingEvent, this,
                                 event, vx, vy));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendTouchpadFlingEvent(event, vx, vy);
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
  return browser_info_->browser() == this;
}

CefRefPtr<CefBrowserHost> CefBrowserHostBase::GetHost() {
  return this;
}

bool CefBrowserHostBase::CanGoBack() {
  base::AutoLock lock_scope(state_lock_);
#if BUILDFLAG(IS_OHOS)
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
#if defined(OHOS_CLIPBOARD)
    wc->CollapseAllFramesSelection();
#endif  // defined(OHOS_CLIPBOARD)
    wc->GetController().GoBack();
  }
}

#if BUILDFLAG(IS_OHOS)
bool CefBrowserHostBase::TerminateRenderProcess() {
  bool result = false;
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->TerminateRenderProcess(result);
    LOG(INFO) << "TerminateRenderProcess result:" << result;
  }
  return result;
}
#endif

#ifdef OHOS_NETWORK_CONNINFO
CefString CefBrowserHostBase::GetOriginalUrl() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetController().GetOriginalUrl();
  }
  return base::EmptyString();
}

void CefBrowserHostBase::PutNetworkAvailable(bool available) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->SetJsOnlineProperty(available);
  }
}
#endif

bool CefBrowserHostBase::CanGoForward() {
  base::AutoLock lock_scope(state_lock_);
#if BUILDFLAG(IS_OHOS)
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

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::ExitFullScreen() {
  base::AutoLock lock_scope(state_lock_);
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "getWebContents falied, wc is null";
    return;
  }

  if (!is_fullscreen_) {
    LOG(ERROR) << "not full-screen state";
    return;
  }
  wc->GetPrimaryMainFrame()->AllowInjectingJavaScript();
  std::string jscode(
      "{if(document.fullscreenElement){document.exitFullscreen()}}");
  wc->GetPrimaryMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(jscode),
                                               base::NullCallback());
}

bool CefBrowserHostBase::CanGoBackOrForward(int num_steps) {
  auto wc = GetWebContents();
  if (wc != nullptr) {
    return wc->GetController().CanGoToOffset(num_steps);
  }
  return false;
}

void CefBrowserHostBase::GoBackOrForward(int num_steps) {
  auto callback =
      base::BindOnce(&CefBrowserHostBase::GoBackOrForward, this, num_steps);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc && wc->GetController().CanGoToOffset(num_steps)) {
    wc->GetController().GoToOffset(num_steps);
  }
}

void CefBrowserHostBase::DeleteHistory() {
  auto callback = base::BindOnce(&CefBrowserHostBase::DeleteHistory, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }
  auto wc = GetWebContents();
  if (wc && wc->GetController().CanPruneAllButLastCommitted()) {
    wc->GetController().PruneAllButLastCommitted();
    base::AutoLock lock_scope(state_lock_);
  }
}
#endif  // BUILDFLAG(IS_OHOS)

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

#if BUILDFLAG(IS_OHOS)
bool CefBrowserHostBase::CanStoreWebArchive() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    return false;
  }

  auto web_contents = GetWebContents();
  std::string mime_type = web_contents->GetContentsMimeType();
  GURL url = web_contents->GetURL();
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && (entry->GetPageType() != content::PAGE_TYPE_NORMAL ||
                entry->GetBaseURLForDataURL().spec() ==
                    content::kUnreachableWebDataURL)) {
    return false;
  }

  return url.SchemeIsHTTPOrHTTPS() &&
         (net::MatchesMimeType(mime_type, "text/html") ||
          net::MatchesMimeType(mime_type, "application/xhtml+xml"));
}

void CefBrowserHostBase::ReloadOriginalUrl() {
  auto callback = base::BindOnce(&CefBrowserHostBase::ReloadOriginalUrl, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = GetWebContents();
  if (wc) {
    wc->GetController().Reload(content::ReloadType::ORIGINAL_REQUEST_URL, true);
  }
}

void CefBrowserHostBase::SetFocusOnWeb() {
  if (settings_.blur_enabled) {
    auto web_contents = GetWebContents();
    if (web_contents) {
      LOG(INFO) << "CefBrowserHostBase::SetFocusOnWeb: ClearFocusedElement";
      web_contents->ClearFocusedElement();
    }
  }
}

void CefBrowserHostBase::StoreWebArchiveInternal(
    CefRefPtr<CefStoreWebArchiveResultCallback> callback,
    const CefString& path) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    callback->OnStoreWebArchiveDone("");
    return;
  }

  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(base::FilePath(path)),
      base::BindOnce(
          [](const CefString& path,
             CefRefPtr<CefStoreWebArchiveResultCallback> callback,
             int64 file_size) {
            CEF_REQUIRE_UIT();
            callback->OnStoreWebArchiveDone(file_size < 0 ? "" : path);
          },
          path, callback));
}

void CefBrowserHostBase::StoreWebArchive(
    const CefString& base_name,
    bool auto_name,
    CefRefPtr<CefStoreWebArchiveResultCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::StoreWebArchive, this,
                                 base_name, auto_name, callback));
    return;
  }

  if (!callback) {
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents) {
    callback->OnStoreWebArchiveDone("");
    return;
  }

  if (auto_name) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&GenerateArchiveAutoNamePath, web_contents->GetURL(),
                       base_name),
        base::BindOnce(&CefBrowserHostBase::StoreWebArchiveInternal,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    StoreWebArchiveInternal(std::move(callback), base_name);
  }
}

void CefBrowserHostBase::SetBrowserUserAgentString(
    const CefString& user_agent) {
  auto callback = base::BindOnce(&CefBrowserHostBase::ReloadOriginalUrl, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
    return;
  }
  PutUserAgent(user_agent);
}

#if defined(OHOS_I18N)
void CefBrowserHostBase::UpdateLocale(const CefString& locale) {
  std::string update_locale = locale.ToString();
  
  // need to notify renderer preference to change accepted_language.
  if (!GetWebContents()) {
    return;
  }

  auto prefs = GetWebContents()->GetMutableRendererPrefs();
  if (prefs->accept_languages.compare(update_locale)) {
    prefs->accept_languages = update_locale;
    GetWebContents()->SyncRendererPrefs();
  }

  if (!ui::ResourceBundle::HasSharedInstance() ||
      !ui::ResourceBundle::LocaleDataPakExists(update_locale)) {
    LOG(ERROR) << "CefBrowserHostBase::UpdateLocale !HasSharedInstance";
    return;
  }

  std::string origin_locale =
      ui::ResourceBundle::GetSharedInstance().GetLoadedLocaleForTesting();
  LOG(DEBUG) << "UpdateLocale origin locale:" << origin_locale
             << ", update locale:" << update_locale;
  if (origin_locale == update_locale) {
    LOG(WARNING) << "CefBrowserHostBase::UpdateLocale no need to update locale";
    return;
  }

  // each render process may need to update locale.
  content::RenderProcessHost* host =
      GetWebContents()->GetPrimaryMainFrame()->GetProcess();
  if (host) {
    host->OnLocaleChangedToRenderer(update_locale);
  }

  const std::string result =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          update_locale);

  if (result.empty()) {
    LOG(ERROR) << "browser host update locale failed";
    return;
  }
  g_browser_process->SetApplicationLocale(result);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kLang)) {
    return;
  }

  std::string current_lang =
      command_line->GetSwitchValueASCII(::switches::kLang);
  if (current_lang != result) {
    command_line->AppendSwitchASCII(switches::kLang, result);
  }
}
#endif

void CefBrowserHostBase::RemoveCache(bool include_disk_files) {
  auto frame = GetMainFrame();
  if (!frame) {
    LOG(ERROR) << "browser host remove cache failed, frame is null";
    return;
  }
  if (frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->RemoveCache(include_disk_files);
  } else {
    LOG(ERROR) << "browser host remove cache failed, frame is not valid";
    return;
  }
}

void CefBrowserHostBase::UpdateBrowserControlsState(int constraints,
                                                    int current,
                                                    bool animate) {
#if defined(OHOS_EX_TOPCONTROLS)
  if (!GetWebContents()) {
    return;
  }

  cc::BrowserControlsState constraints_state =
      static_cast<cc::BrowserControlsState>(constraints);
  cc::BrowserControlsState current_state =
      static_cast<cc::BrowserControlsState>(current);

  GetWebContents()->UpdateBrowserControlsState(constraints_state, current_state,
                                               animate);
#endif
}

void CefBrowserHostBase::UpdateBrowserControlsHeight(int height, bool animate) {
#if defined(OHOS_EX_TOPCONTROLS)
  if (!GetWebContents()) {
    return;
  }

  GetWebContents()->UpdateBrowserControlsHeight(height, animate);
#endif
}

void CefBrowserHostBase::GetRootBrowserAccessibilityManager(
    void** manager) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::SetVirtualKeyBoardArg(int32_t width,
                                               int32_t height,
                                               double keyboard) {
  // todo(ohos):impl this function then remove todo
}

bool CefBrowserHostBase::ShouldVirtualKeyboardOverlay() {
  // todo(ohos):impl this function then remove todo
  return false;
}

void CefBrowserHostBase::SetDrawRect(int x, int y, int width, int height) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::SetNativeInnerWeb(bool isInnerWeb) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::SetDrawMode(int mode) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::SetFitContentMode(int mode) {
  // todo(ohos):impl this function then remove todo
}

bool CefBrowserHostBase::GetPendingSizeStatus() {
  // todo(ohos):impl this function then remove todo
  return false;
}

void CefBrowserHostBase::SetZoomLevel(double zoomLevel) {
  // todo(ohos):impl this function then remove todo
}
double CefBrowserHostBase::GetZoomLevel() {
  // todo(ohos):impl this function then remove todo
  return 0;
}

void CefBrowserHostBase::SetBrowserZoomLevel(double zoom_factor) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::AdvanceFocusForIME(int focusType) {
  // todo(ohos):impl this function then remove todo
}

void CefBrowserHostBase::SetNativeEmbedMode(bool flag) {
  // todo(ohos):impl this function then remove todo
}
#endif  // IS_OHOS

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
  auto impl = static_cast<CefBrowserHostBase*>(that.get());
  return (impl == this);
}

bool CefBrowserHostBase::HasDocument() {
  base::AutoLock lock_scope(state_lock_);
  return has_document_;
}

bool CefBrowserHostBase::IsPopup() {
  return browser_info_->is_popup();
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetMainFrame() {
  return GetFrame(CefFrameHostImpl::kMainFrameId);
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFocusedFrame() {
  return GetFrame(CefFrameHostImpl::kFocusedFrameId);
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrame(int64 identifier) {
  if (identifier == CefFrameHostImpl::kInvalidFrameId) {
    return nullptr;
  } else if (identifier == CefFrameHostImpl::kMainFrameId) {
    return browser_info_->GetMainFrame();
  } else if (identifier == CefFrameHostImpl::kFocusedFrameId) {
    base::AutoLock lock_scope(state_lock_);
    if (!focused_frame_) {
      // The main frame is focused by default.
      return browser_info_->GetMainFrame();
    }
    return focused_frame_;
  }

  return browser_info_->GetFrameForGlobalId(
      frame_util::MakeGlobalId(identifier));
}

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrame(const CefString& name) {
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

void CefBrowserHostBase::GetFrameIdentifiers(std::vector<int64>& identifiers) {
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

void CefBrowserHostBase::OnStateChanged(CefBrowserContentsState state_changed) {
  // Make sure that CefBrowser state is consistent before the associated
  // CefClient callback is executed.
  base::AutoLock lock_scope(state_lock_);
  if ((state_changed & CefBrowserContentsState::kNavigation) ==
      CefBrowserContentsState::kNavigation) {
    is_loading_ = contents_delegate_->is_loading();
#if !BUILDFLAG(IS_OHOS)
    can_go_back_ = contents_delegate_->can_go_back();
    can_go_forward_ = contents_delegate_->can_go_forward();
#endif
  }
  if ((state_changed & CefBrowserContentsState::kDocument) ==
      CefBrowserContentsState::kDocument) {
    has_document_ = contents_delegate_->has_document();
  }
  if ((state_changed & CefBrowserContentsState::kFullscreen) ==
      CefBrowserContentsState::kFullscreen) {
    is_fullscreen_ = contents_delegate_->is_fullscreen();
  }
  if ((state_changed & CefBrowserContentsState::kFocusedFrame) ==
      CefBrowserContentsState::kFocusedFrame) {
    focused_frame_ = contents_delegate_->focused_frame();
  }
}

void CefBrowserHostBase::OnWebContentsDestroyed(
    content::WebContents* web_contents) {}

#if BUILDFLAG(IS_OHOS)
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

  GetClient()->GetPermissionRequest()->OnGeolocationShow(origin);
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
    GetClient()->GetPermissionRequest()->OnGeolocationHide();
    if (!unhandled_geolocation_prompts_.empty()) {
      OnGeolocationShow(unhandled_geolocation_prompts_.front().first);
    }
  }
}

#if defined(OHOS_SENSOR)
void CefBrowserHostBase::AskSensorsPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  if (permission_request_handler_) {
    permission_request_handler_->SendSensorRequest(new AlloySensorAccessRequest(
        this, origin, std::move(callback)));
  } else {
    LOG(ERROR) << "AskSensorsPermission, handler is null.";
  }
}

void CefBrowserHostBase::AbortAskSensorsPermission(
    const CefString& origin) {
  if (permission_request_handler_) {
    permission_request_handler_->CancelRequest(
        origin, AlloyAccessRequest::Resources::SENSORS);
  } else {
    LOG(ERROR) << "AbortAskSensorsPermission, handler is null.";
  }
}
#endif // defined(OHOS_SENSOR)

#ifdef OHOS_NOTIFICATION
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
#endif // OHOS_NOTIFICATION

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::MaximizeResize() {}
#endif

void CefBrowserHostBase::AskProtectedMediaIdentifierPermission(
    const CefString& origin,
    cef_permission_callback_t callback) {
  permission_request_handler_->SendRequest(new AlloyAccessRequest(
      origin, AlloyAccessRequest::Resources::PROTECTED_MEDIA_ID,
      std::move(callback)));
}

void CefBrowserHostBase::AbortAskProtectedMediaIdentifierPermission(
    const CefString& origin) {
  permission_request_handler_->CancelRequest(
      origin, AlloyAccessRequest::Resources::PROTECTED_MEDIA_ID);
}

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
#endif

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
  return browser_info_->GetFrameForGlobalId(global_id, nullptr);
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

#ifdef OHOS_POST_URL
    if (params.post_data) {
      content::NavigationController::LoadURLParams LoadURLParams(params);
      web_contents->GetController().LoadURLWithParams(LoadURLParams);
    } else {
      web_contents->GetController().LoadURL(
          gurl, params.referrer, params.transition, params.extra_headers
#ifdef OHOS_NETWORK_LOAD
          ,
          params.user_gesture
#endif
          );
    }
#else
    web_contents->GetController().LoadURL(
        gurl, params.referrer, params.transition, params.extra_headers
#ifdef OHOS_NETWORK_LOAD
        ,
        params.user_gesture
#endif
      );
#endif
    return true;
  }
  return false;
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
    gfx::NativeWindow owning_window,
    void* params) {
  if (!EnsureFileDialogManager()) {
    LOG(ERROR) << "File dialog canceled due to invalid state.";
    listener->FileSelectionCanceled(params);
    return;
  }
  file_dialog_manager_->RunSelectFile(listener, std::move(policy), type, title,
                                      default_path, file_types, file_type_index,
                                      default_extension, owning_window, params);
}

void CefBrowserHostBase::SelectFileListenerDestroyed(
    ui::SelectFileDialog::Listener* listener) {
  if (file_dialog_manager_) {
    file_dialog_manager_->SelectFileListenerDestroyed(listener);
  }
}

bool CefBrowserHostBase::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    bool is_guest_view,
    const content::OpenURLParams& params) {
  return true;
}

void CefBrowserHostBase::OnAfterCreated() {
  CEF_REQUIRE_UIT();
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

bool CefBrowserHostBase::IsWindowless() const {
  return false;
}

content::WebContents* CefBrowserHostBase::GetWebContents() const {
  CEF_REQUIRE_UIT();
  return contents_delegate_->web_contents();
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

bool CefBrowserHostBase::EnsureDevToolsManager() {
  CEF_REQUIRE_UIT();
  if (!contents_delegate_->web_contents()) {
    return false;
  }

  if (!devtools_manager_) {
    devtools_manager_ = std::make_unique<CefDevToolsManager>(this);
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

  if (!EnsureDevToolsManager()) {
    return;
  }
  devtools_manager_->InitializeRegistrationOnUIThread(registration);
}

bool CefBrowserHostBase::EnsureFileDialogManager() {
  CEF_REQUIRE_UIT();
  if (!contents_delegate_->web_contents()) {
    return false;
  }

  if (!file_dialog_manager_) {
    file_dialog_manager_ = std::make_unique<CefFileDialogManager>(this);
  }
  return true;
}

#if BUILDFLAG(IS_OHOS)
CefString CefBrowserHostBase::Title() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetTitle();
  }
  return "";
}

#if defined(OHOS_MSGPORT)
// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
void CefBrowserHostBase::CreateWebMessagePorts(std::vector<CefString>& ports) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CHECK(false) << "called on invalid thread";
    return;
  }
  base::AutoLock msg_lock(web_message_lock_);
  auto web_contents = GetWebContents();
  if (web_contents) {
    int retry_times = 0;
    constexpr int MAX_RETRY_TIMES = 5;
    do {
      std::vector<blink::WebMessagePort> portArr;
      web_contents->CreateWebMessagePorts(portArr);
      if (portArr.size() != 2) {
        LOG(ERROR) << "CreateWebMessagePorts size wrong";
        return;
      }
      uint64_t pointer0 = base::RandUint64();
      if (pointer0 == ULLONG_MAX || (pointer0 + 1) == ULLONG_MAX) {
        retry_times++;
        continue;
      }
      pointer0 = ((pointer0 % 2) == 0) ? (pointer0 + 1) : pointer0;
      uint64_t pointer1 = pointer0 + 1;
      auto iter = portMap_.find(std::make_pair(pointer0, pointer1));
      if (iter == portMap_.end()) {
        portMap_[std::make_pair(pointer0, pointer1)] =
            std::make_pair(std::move(portArr[0]), std::move(portArr[1]));
        ports.emplace_back(std::to_string(pointer0));
        ports.emplace_back(std::to_string(pointer1));
        return;
      }
      retry_times++;
    } while (retry_times < MAX_RETRY_TIMES);
  } else {
    LOG(ERROR) << "CreateWebMessagePorts web_contents its null";
  }
}

// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
void CefBrowserHostBase::PostWebMessage(CefString& message,
                                        std::vector<CefString>& port_handles,
                                        CefString& targetUri) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CHECK(false) << "called on invalid thread";
    return;
  }
  base::AutoLock msg_lock(web_message_lock_);
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "PostWebMessage web_contents its null";
    return;
  }

  std::string msg = message.empty() ? "" : message.ToString();
  std::string uri = targetUri.empty() ? "" : targetUri.ToString();
  // check whether the port is already send to html5.
  for (CefString port : port_handles) {
    auto found = postedPorts_.find(port.ToString());
    if (found != postedPorts_.end()) {
      LOG(ERROR) << "This port has alrady send to h5, can not post again.";
      return;
    }
  }

  // find the WebMessagePort by port_handle, and send to html5
  std::vector<blink::WebMessagePort> sendPorts;
  for (CefString port : port_handles) {
    LOG(DEBUG) << "PostWebMessage port:" << port.ToString();
    for (auto iter = portMap_.begin(); iter != portMap_.end(); ++iter) {
      if (port.ToString().compare(std::to_string(iter->first.first)) == 0) {
        postedPorts_.insert(port.ToString());
        sendPorts.emplace_back(std::move(iter->second.first));
      } else if (port.ToString().compare(std::to_string(iter->first.second)) ==
                 0) {
        postedPorts_.insert(port.ToString());
        sendPorts.emplace_back(std::move(iter->second.second));
      }
    }
  }

  // send to html5 main frame
  if (sendPorts.size() >= 1) {
    web_contents->PostWebMessage(msg, sendPorts, uri);
  }
}

// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
void CefBrowserHostBase::ClosePortInternal(const CefString& portHandle) {
  base::AutoLock msg_lock(web_message_lock_);
  LOG(DEBUG) << "ClosePort Start";

  // find port and close, then erase the item in map
  blink::WebMessagePort port;
  for (auto iter = portMap_.begin(); iter != portMap_.end(); ++iter) {
    if (portHandle.ToString().compare(std::to_string(iter->first.first)) == 0) {
      port = std::move(iter->second.first);
      port.Close();
      portMap_.erase(iter);
      break;
    } else if (portHandle.ToString().compare(
                   std::to_string(iter->first.second)) == 0) {
      port = std::move(iter->second.second);
      port.Close();
      portMap_.erase(iter);
      break;
    }
  }

  for (auto iter = receiverMap_.begin(); iter != receiverMap_.end(); ++iter) {
    if (portHandle.ToString().compare(iter->first) == 0) {
      receiverMap_.erase(iter);
      break;
    }
  }

  postedPorts_.erase(portHandle.ToString());
  LOG(DEBUG) << "ClosePort end";
}

void CefBrowserHostBase::ClosePort(const CefString& portHandle) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CefBrowserHostBase::ClosePortInternal,
                                  this, portHandle));
}

bool CefBrowserHostBase::ConvertCefValueToBlinkMsg(
    CefRefPtr<CefValue>& original,
    blink::WebMessagePort::Message& message) {
  LOG(DEBUG) << "ConvertCefValueToBlinkMsg type:" << (int)original->GetType();
  switch (original->GetType()) {
    case VTYPE_STRING: {
      message = blink::WebMessagePort::Message(
          base::UTF8ToUTF16(original->GetString().ToString()));
      message.type_ = blink::WebMessagePort::Message::MessageType::STRING;
      break;
    }
    case VTYPE_BINARY: {
      CefRefPtr<CefBinaryValue> binValue = original->GetBinary();
      size_t len = binValue->GetSize();
      std::vector<uint8_t> arr(len);
      binValue->GetData(&arr[0], len, 0);
      message = blink::WebMessagePort::Message(std::move(arr));
      message.type_ = blink::WebMessagePort::Message::MessageType::BINARY;
      break;
    }
    case VTYPE_BOOL: {
      message.bool_value_ = original->GetBool();
      message.type_ = blink::WebMessagePort::Message::MessageType::BOOLEAN;
      break;
    }
    case VTYPE_INT: {
      message.int64_value_ = original->GetInt();
      message.type_ = blink::WebMessagePort::Message::MessageType::INTEGER;
      break;
    }
    case VTYPE_DOUBLE: {
      message.double_value_ = original->GetDouble();
      message.type_ = blink::WebMessagePort::Message::MessageType::DOUBLE;
      break;
    }
    case VTYPE_LIST: {
      CefRefPtr<CefListValue> value = original->GetList();
      CefValueType type = value->GetType(0);
      size_t len = value->GetSize();
      switch (type) {
        case VTYPE_STRING: {
          std::vector<std::u16string> msg_arr;
          for (size_t i = 0; i < len; i++) {
            msg_arr.push_back(
                base::UTF8ToUTF16(value->GetString(i).ToString()));
          }
          message.string_arr_ = std::move(msg_arr);
          message.type_ =
              blink::WebMessagePort::Message::MessageType::STRINGARRAY;
          break;
        }
        case VTYPE_BOOL: {
          std::vector<bool> msg_arr;
          for (size_t i = 0; i < len; i++) {
            msg_arr.push_back(value->GetBool(i));
          }
          message.bool_arr_ = std::move(msg_arr);
          message.type_ =
              blink::WebMessagePort::Message::MessageType::BOOLEANARRAY;
          break;
        }
        case VTYPE_INT: {
          std::vector<int64_t> msg_arr;
          for (size_t i = 0; i < len; i++) {
            msg_arr.push_back(value->GetInt(i));
          }
          message.int64_arr_ = std::move(msg_arr);
          message.type_ =
              blink::WebMessagePort::Message::MessageType::INT64ARRAY;
          break;
        }
        case VTYPE_DOUBLE: {
          std::vector<double> msg_arr;
          for (size_t i = 0; i < len; i++) {
            msg_arr.push_back(value->GetDouble(i));
          }
          message.double_arr_ = std::move(msg_arr);
          message.type_ =
              blink::WebMessagePort::Message::MessageType::DOUBLEARRAY;
          break;
        }
        default:
          LOG(ERROR) << "Only support string, bool, int, double";
          break;
      }
      break;
    }
    case VTYPE_DICTIONARY: {
      CefRefPtr<CefDictionaryValue> dict = original->GetDictionary();
      std::u16string err_name = u"";
      std::u16string err_msg = u"";
      if (dict->HasKey("Error.name")) {
        err_name = base::UTF8ToUTF16(dict->GetString("Error.name").ToString());
      }
      if (dict->HasKey("Error.message")) {
        err_msg =
            base::UTF8ToUTF16(dict->GetString("Error.message").ToString());
      }
      message.err_name_ = std::move(err_name);
      message.err_msg_ = std::move(err_msg);
      message.type_ = blink::WebMessagePort::Message::MessageType::ERROR;
      break;
    }
    default: {
      LOG(ERROR) << "Not support type:" << (int)original->GetType();
      break;
    }
  }
  return true;
}

void CefBrowserHostBase::PostPortMessageInternal(const CefString& portHandle,
                                         CefRefPtr<CefValue> data) {
  base::ScopedAllowBlocking scoped_allow_blocking;
  base::AutoLock msg_lock(web_message_lock_);
  // construct blink message
  blink::WebMessagePort::Message message;
  if (!ConvertCefValueToBlinkMsg(data, message)) {
    LOG(ERROR) << "Post meessage: convert cef value to blink message failed";
    return;
  }

  // find the WebMessagePort in map
  for (auto iter = portMap_.begin(); iter != portMap_.end(); ++iter) {
    if (portHandle.ToString().compare(std::to_string(iter->first.first)) == 0) {
      if (iter->second.first.CanPostMessage()) {
        iter->second.first.PostMessage(std::move(message));
      } else {
        LOG(ERROR) << "port can not post messsage";
      }
      break;
    } else if (portHandle.ToString().compare(
                   std::to_string(iter->first.second)) == 0) {
      if (iter->second.second.CanPostMessage()) {
        iter->second.second.PostMessage(std::move(message));
      } else {
        LOG(ERROR) << "port can not post messsage";
      }
      break;
    }
  }
}

void CefBrowserHostBase::PostPortMessage(const CefString& portHandle,
                                         CefRefPtr<CefValue> data) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CefBrowserHostBase::PostPortMessageInternal,
                                  this, portHandle, data));
}

void CefBrowserHostBase::SetPortMessageCallbackInternal(
    const CefString& portHandle,
    CefRefPtr<CefWebMessageReceiver> callback) {
  base::ScopedAllowBlocking scoped_allow_blocking;
  base::AutoLock msg_lock(web_message_lock_);
  std::string pointer0 = portHandle.ToString();

  // get web message receiver instance
  std::shared_ptr<WebMessageReceiverImpl> webMsgReceiver;
  auto receive_it = receiverMap_.find(pointer0);
  if (receive_it != receiverMap_.end()) {
    webMsgReceiver = receive_it->second;
  } else {
    webMsgReceiver = std::make_shared<WebMessageReceiverImpl>();
  }

  webMsgReceiver->SetOnMessageCallback(callback);

  // find the port set message callback
  for (auto iter = portMap_.begin(); iter != portMap_.end(); ++iter) {
    if (portHandle.ToString().compare(std::to_string(iter->first.first)) == 0) {
      if (iter->second.first.HasReceiver()) {
        iter->second.first.ClearReceiver();
      }
      iter->second.first.SetReceiver(webMsgReceiver.get(),
                                     sequenced_task_runner_);
      break;
    } else if (portHandle.ToString().compare(
                   std::to_string(iter->first.second)) == 0) {
      if (iter->second.second.HasReceiver()) {
        iter->second.second.ClearReceiver();
      }
      iter->second.second.SetReceiver(webMsgReceiver.get(),
                                      sequenced_task_runner_);
      break;
    }
  }

  // save in map
  receiverMap_[pointer0] = webMsgReceiver;
}

// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
void CefBrowserHostBase::SetPortMessageCallback(
    const CefString& portHandle,
    CefRefPtr<CefWebMessageReceiver> callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CefBrowserHostBase::SetPortMessageCallbackInternal,
                                this, portHandle, callback));
}

void CefBrowserHostBase::DestroyAllWebMessagePorts() {
  base::AutoLock msg_lock(web_message_lock_);
  LOG(DEBUG) << "clear all message ports";
  portMap_.clear();
  receiverMap_.clear();
  postedPorts_.clear();
}
#endif  // defined(OHOS_MSGPORT)

WebMessageReceiverImpl::~WebMessageReceiverImpl() {
  LOG(DEBUG) << "WebMessageReceiverImpl destrory";
}

void WebMessageReceiverImpl::SetOnMessageCallback(
    CefRefPtr<CefWebMessageReceiver> callback) {
  LOG(DEBUG) << "WebMessageReceiverImpl::SetOnMessageCallback ";
  callback_ = callback;
}

void WebMessageReceiverImpl::ConvertBlinkMsgToCefValue(
    blink::WebMessagePort::Message& message,
    CefRefPtr<CefValue> data) {
  switch (message.type_) {
    case blink::WebMessagePort::Message::MessageType::STRING: {
      data->SetString(base::UTF16ToUTF8(message.data));
      break;
    }
    case blink::WebMessagePort::Message::MessageType::BINARY: {
      std::vector<uint8_t> vecBinary = message.array_buffer;
      CefRefPtr<CefBinaryValue> value =
          CefBinaryValue::Create(vecBinary.data(), vecBinary.size());
      data->SetBinary(value);
      break;
    }
    case blink::WebMessagePort::Message::MessageType::DOUBLE: {
      data->SetDouble((message.double_value_));
      break;
    }
    case blink::WebMessagePort::Message::MessageType::BOOLEAN: {
      data->SetBool((message.bool_value_));
      break;
    }
    case blink::WebMessagePort::Message::MessageType::INTEGER: {
      data->SetInt((message.int64_value_));
      break;
    }
    case blink::WebMessagePort::Message::MessageType::STRINGARRAY: {
      CefRefPtr<CefListValue> value = CefListValue::Create();
      for (size_t i = 0; i < message.string_arr_.size(); i++) {
        value->SetString(i, base::UTF16ToUTF8(message.string_arr_[i]));
      }
      data->SetList(value);
      break;
    }
    case blink::WebMessagePort::Message::MessageType::BOOLEANARRAY: {
      CefRefPtr<CefListValue> value = CefListValue::Create();
      for (size_t i = 0; i < message.bool_arr_.size(); i++) {
        value->SetBool(i, message.bool_arr_[i]);
      }
      data->SetList(value);
      break;
    }
    case blink::WebMessagePort::Message::MessageType::DOUBLEARRAY: {
      CefRefPtr<CefListValue> value = CefListValue::Create();
      for (size_t i = 0; i < message.double_arr_.size(); i++) {
        value->SetDouble(i, message.double_arr_[i]);
      }
      data->SetList(value);
      break;
    }
    case blink::WebMessagePort::Message::MessageType::INT64ARRAY: {
      CefRefPtr<CefListValue> value = CefListValue::Create();
      for (size_t i = 0; i < message.int64_arr_.size(); i++) {
        value->SetInt(i, message.int64_arr_[i]);
      }
      data->SetList(value);
      break;
    }
    case blink::WebMessagePort::Message::MessageType::ERROR: {
      std::u16string err_name = message.err_name_;
      std::u16string err_msg = message.err_msg_;
      CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
      dict->SetString("Error.name", err_name);
      dict->SetString("Error.message", err_msg);
      data->SetDictionary(dict);
      break;
    }
    default:
      break;
  }
}

// this will receive message from html5
bool WebMessageReceiverImpl::OnMessage(blink::WebMessagePort::Message message) {
  LOG(DEBUG) << "OnMessage start";
  // Pass the message on to the receiver.
  if (callback_) {
    CefRefPtr<CefValue> data = CefValue::Create();
    ConvertBlinkMsgToCefValue(message, data);
    callback_->OnMessage(data);
  } else {
    LOG(ERROR) << "u should set callback to receive message";
  }
  return true;
}

void CefBrowserHostBase::SetInitialScale(float scale) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->SetInitialScale(scale / (100 / virtual_pixel_ratio_));
  }
}

void CefBrowserHostBase::SetVirtualPixelRatio(float ratio) {
#ifdef OHOS_SCROLLBAR
  if (std::fabs(ratio - virtual_pixel_ratio_) > std::numeric_limits<float>::epsilon()) {
    UpdatePixelRatio(ratio);
  }
#endif
  virtual_pixel_ratio_ = ratio;
}

float CefBrowserHostBase::GetVirtualPixelRatio() {
  return virtual_pixel_ratio_;
}

#ifdef OHOS_SCROLLBAR
void CefBrowserHostBase::UpdatePixelRatio(float ratio) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->UpdatePixelRatio(ratio);
  } else {
    LOG(ERROR) << "main frame is invalid";
  }
}
#endif

float CefBrowserHostBase::Scale() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    return static_cast<content::WebContentsImpl*>(web_contents)
        ->GetPrimaryPage()
        .GetPageScaleFactor();
  }
  return 1.0;
}

bool CefBrowserHostBase::IsBase64Encoded(std::string encoding) {
  return "base64" == encoding;
}

std::string CefBrowserHostBase::GetDataURI(const std::string& data) {
  return CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
      .ToString();
}

int CefBrowserHostBase::PageLoadProgress() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    if (!web_contents->IsLoading()) {
      return 100;
    }
    return round(100 * web_contents->GetLoadProgress());
  }
  return 0;
}

void CefBrowserHostBase::LoadWithDataAndBaseUrl(const CefString& baseUrl,
                                                const CefString& data,
                                                const CefString& mimeType,
                                                const CefString& encoding,
                                                const CefString& historyUrl) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::LoadWithDataAndBaseUrl, this,
                       baseUrl, data, mimeType, encoding, historyUrl));
    return;
  }
  std::string dataBase = data.empty() ? "" : data;
  std::string mimeTypeBase = mimeType.empty() ? "text/html" : mimeType;
  std::string url = baseUrl.empty() ? "about:blank" : baseUrl;
  std::string historyUrlBase = historyUrl.empty() ? "about:blank" : historyUrl;

  std::string buildData = "data:";
  buildData.append(mimeTypeBase);
  if (!encoding.empty()) {
    buildData.append(";charset=");
    buildData.append(encoding);
  }
  buildData.append(";base64");
  buildData.append(",");
  std::string emtry_data_url = buildData;
  dataBase = GetDataURI(dataBase);
  buildData.append(dataBase);
  GURL data_url = GURL(buildData);
  content::NavigationController::LoadURLParams loadUrlParams(data_url);
  if (data_url.spec().size() > url::kMaxURLChars) {
    loadUrlParams.url = GURL(emtry_data_url);
    loadUrlParams.data_url_as_string =
      base::MakeRefCounted<base::RefCountedString>(std::move(buildData));
  }

  if (!(url.find("data:") == 0)) {
    loadUrlParams.virtual_url_for_data_url = GURL(historyUrlBase);
    loadUrlParams.base_url_for_data_url = GURL(url);
  }

  loadUrlParams.load_type = content::NavigationController::LOAD_TYPE_DATA;
  loadUrlParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  loadUrlParams.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;
  loadUrlParams.can_load_local_resources = true;
  auto web_contents = GetWebContents();
  if (web_contents) {
    LOG(DEBUG) << "load data with BaseUrl";
    web_contents->GetController().LoadURLWithParams(loadUrlParams);
  }
}

void CefBrowserHostBase::LoadWithData(const CefString& data,
                                      const CefString& mimeType,
                                      const CefString& encoding) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostBase::LoadWithData,
                                          this, data, mimeType, encoding));
    return;
  }
  std::string dataBase = data.empty() ? "" : data;
  std::string mimeTypeBase = mimeType.empty() ? "text/html" : mimeType;

  std::string buildData = "data:";
  buildData.append(mimeTypeBase);
  if (encoding.ToString() == "base64") {
    buildData.append(";base64");
  }
  buildData.append(",");
  std::string emtry_data_url = buildData;
  buildData.append(dataBase);
  GURL data_url = GURL(buildData);
  content::NavigationController::LoadURLParams loadUrlParams(data_url);
  if (data_url.spec().size() > url::kMaxURLChars) {
    loadUrlParams.url = GURL(emtry_data_url);
    loadUrlParams.data_url_as_string =
      base::MakeRefCounted<base::RefCountedString>(std::move(buildData));
  }

  auto web_contents = GetWebContents();
  if (web_contents) {
    LOG(DEBUG) << "load data";
    web_contents->GetController().LoadURLWithParams(loadUrlParams);
  }
}

// static
bool ValidateResultType(base::Value::Type type) {
  if (type == base::Value::Type::STRING || type == base::Value::Type::DOUBLE ||
      type == base::Value::Type::INTEGER ||
      type == base::Value::Type::BOOLEAN) {
    return true;
  }
  return false;
}

void CefBrowserHostBase::ExecuteJSCallback(
    CefRefPtr<CefJavaScriptResultCallback> callback,
    base::Value result) {
  LOG(DEBUG) << "javascript result callback enter";
  std::string json;
  base::JSONWriter::Write(result, &json);
  if (callback != nullptr) {
    CefRefPtr<CefValue> data = CefValue::Create();
    data->SetString(json);
    callback->OnJavaScriptExeResult(data);
  }
}

void CefBrowserHostBase::ExecuteExtensionJSCallback(
    CefRefPtr<CefJavaScriptResultCallback> callback,
    base::Value result) {
  LOG(DEBUG) << "javascript result callback enter, type:"
    << result.GetTypeName(result.type());
  std::string json;
  base::JSONWriter::Write(result, &json);
  CefRefPtr<CefValue> data = CefValue::Create();
  switch (result.type()) {
    case base::Value::Type::STRING: {
      data->SetString(json);
      break;
    }
    case base::Value::Type::DOUBLE: {
      data->SetDouble(result.GetDouble());
      break;
    }
    case base::Value::Type::INTEGER: {
      data->SetDouble(result.GetInt());
      break;
    }
    case base::Value::Type::BOOLEAN: {
      data->SetBool(result.GetBool());
      break;
    }
    case base::Value::Type::BINARY: {
      std::vector<uint8_t> vec = result.GetBlob();
      CefRefPtr<CefBinaryValue> value =
        CefBinaryValue::Create(vec.data(), vec.size());
      data->SetBinary(value);
      break;
    }
    case base::Value::Type::LIST: {
      int len = result.GetList().size();
      CefRefPtr<CefListValue> value = CefListValue::Create();
      base::Value::Type typeFirst = base::Value::Type::NONE;
      base::Value::Type typeCur = base::Value::Type::NONE;
      bool support = true;
      for (int i = 0; i < len; i++) {
        base::Value list_ele = std::move(result.GetList()[i]);
        typeCur = list_ele.type();
        if (!ValidateResultType(typeCur)) {
          data->SetString(
            "This type not support, only string/number/boolean "
            "is supported for array elements");
          support = false;
          break;
        }
        if (i == 0) {
          typeFirst = typeCur;
        }
        if (typeCur != typeFirst) {
          support = false;
          data->SetString(
            "This type not support, The elements in the array "
            "must be the same.");
          break;
        }
        switch (list_ele.type()) {
          case base::Value::Type::STRING: {
            CefString msgCef;
            msgCef.FromString(list_ele.GetString());
            value->SetString(i, msgCef);
            break;
          }
          case base::Value::Type::DOUBLE: {
            value->SetDouble(i, list_ele.GetDouble());
            break;
          }
          case base::Value::Type::INTEGER: {
            value->SetInt(i, list_ele.GetInt());
            break;
          }
          case base::Value::Type::BOOLEAN: {
            value->SetBool(i, list_ele.GetBool());
            break;
          }
          default: {
            LOG(ERROR) << "Not support type";
            support = false;
            data->SetString(
              "This type not support, only "
              "string/number/boolean is supported for array "
              "elements");
            break;
          }
        }
      }
      if (support) {
        data->SetList(value);
      }
      break;
    }
    default: {
      LOG(ERROR)
        << "base::Value not support type:" << result.type();
      data->SetString(
        "This type not support, only "
        "string/number/boolean/arraybuffer/array is supported");
      break;
    }
  }

  if (callback != nullptr) {
    callback->OnJavaScriptExeResult(data);
  }
}

void CefBrowserHostBase::ExecuteJavaScript(
    const std::string& code,
    CefRefPtr<CefJavaScriptResultCallback> callback,
    bool extention) {
  auto web_contents = GetWebContents();
  // enable inject javaScript
  LOG(DEBUG) << "ExecuteJavaScript with callback enter";
  if (web_contents && web_contents->GetPrimaryMainFrame()) {
    LOG(DEBUG) << "ExecuteJavaScript with callback";
    web_contents->GetPrimaryMainFrame()->AllowInjectingJavaScript();
    if (!extention) {
      web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
          base::UTF8ToUTF16(code),
          base::BindOnce(&CefBrowserHostBase::ExecuteJSCallback, this, callback));
    } else {
      web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
          base::UTF8ToUTF16(code),
          base::BindOnce(&CefBrowserHostBase::ExecuteExtensionJSCallback, this, callback));
    }
  }
}

void CefBrowserHostBase::ExecuteJavaScriptExt(
    const int fd,
    const uint64 scriptLength,
    CefRefPtr<CefJavaScriptResultCallback> callback,
    bool extention) {
  auto web_contents = GetWebContents();
  // enable inject javaScript
  LOG(DEBUG) << "ExecuteJavaScriptExt with callback enter";
  if (web_contents && web_contents->GetPrimaryMainFrame()) {
    LOG(DEBUG) << "ExecuteJavaScriptExt with callback";
    web_contents->GetPrimaryMainFrame()->AllowInjectingJavaScript();
    if (!extention) {
      web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptExt(
          fd, scriptLength,
          base::BindOnce(&CefBrowserHostBase::ExecuteJSCallback, this, callback));
    } else {
      web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptExt(
          fd, scriptLength,
          base::BindOnce(&CefBrowserHostBase::ExecuteExtensionJSCallback, this, callback));
    }
  }
}

void CefBrowserHostBase::SetNativeWindow(cef_native_window_t window) {
  widget_ = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(window);
}

cef_accelerated_widget_t CefBrowserHostBase::GetAcceleratedWidget() {
  return widget_;
}

void CefBrowserHostBase::SetWebDebuggingAccess(bool isEnableDebug) {
  base::AutoLock lock_scope(state_lock_);
  if (is_web_debugging_access_ == isEnableDebug) {
    return;
  }

  if (isEnableDebug) {
    CefDevToolsManagerDelegate::StartHttpHandler(GetBrowserContext());
  } else {
    CefDevToolsManagerDelegate::StopHttpHandler();
  }
  is_web_debugging_access_ = isEnableDebug;
}

bool CefBrowserHostBase::GetWebDebuggingAccess() {
  base::AutoLock lock_scope(state_lock_);
  return is_web_debugging_access_;
}

void CefBrowserHostBase::GetImageForContextNode(int command_id) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->GetImageForContextNode(command_id);
  }
}

void CefBrowserHostBase::GetImageFromCacheEx(const CefString& url) {
#ifdef OHOS_NWEB_EX
  LOG(INFO) << "CefBrowserHostBase::GetImageFromCacheEx";
  auto web_contents = GetWebContents();
  auto frame = GetMainFrame();
  if (web_contents && frame && frame->IsValid()) {
    content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
      rfh->GetImageFromCache(
          url.ToString(),
          base::BindOnce(&CefFrameHostImpl::OnGetImageFromCacheEx,
                         static_cast<CefFrameHostImpl*>(frame.get()), url.ToString()));
    return;
  }
#endif
}

void CefBrowserHostBase::GetImageFromCache(const CefString& url, int command_id) {
  LOG(INFO) << "CefBrowserHostBase::GetImageFromCache";
  auto web_contents = GetWebContents();
  auto frame = GetMainFrame();
  if (web_contents && frame && frame->IsValid()) {
    content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
    if (rfh) {
      LOG(INFO) << "CefBrowserHostBase::GetImageFromCache";
      rfh->GetImageFromCache(
          url.ToString(),
          base::BindOnce(&CefFrameHostImpl::OnGetImageFromCache,
                         static_cast<CefFrameHostImpl*>(frame.get()),
                         url.ToString(), command_id));
    }
  }
}

void CefBrowserHostBase::SetAudioResumeInterval(int resumeInterval) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::SetAudioExclusive(bool audioExclusive) {
  // TODO(ohos): please impl the function and remove this comment.
}

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::WasOccluded(bool occluded) {
  // TODO(ohos): please impl the function and remove this comment.
}

 // TODO(ohos): please impl the function and remove this comment.
void CefBrowserHostBase::OnWindowShow() {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::OnWindowHide() {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::OnOnlineRenderToForeground() {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::SendTouchEventList(const std::vector<CefTouchEvent>& event_list) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::WasKeyboardResized() {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::SetWindowId(int window_id, int nweb_id) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::SetWakeLockHandler(int32_t windowId, CefRefPtr<CefSetLockCallback> callback) {
  // TODO(ohos): please impl the function and remove this comment.
}

#if defined(OHOS_PRINT)
void CefBrowserHostBase::SetToken(void* token) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::CreateWebPrintDocumentAdapter(
    const CefString& jobName,
    void** webPrintDocumentAdapter) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::SetPrintBackground(bool enabled) {
  // TODO(ohos): please impl the function and remove this comment.
}

bool CefBrowserHostBase::GetPrintBackground() {
  // TODO(ohos): please impl the function and remove this comment.
  return false;
}
#endif // defined(OHOS_PRINT)

void CefBrowserHostBase::SetEnableLowerFrameRate(bool enabled) {
  // TODO(ohos): please impl the function and remove this comment.
}
#endif

#ifdef OHOS_PAGE_UP_DOWN
void CefBrowserHostBase::ScrollPageUpDown(bool is_up,
                                          bool is_half,
                                          float view_height) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->ScrollPageUpDown(is_up, is_half, view_height);
  }
}

#ifdef OHOS_GET_SCROLL_OFFSET
void CefBrowserHostBase::GetScrollOffset(float* offset_x,
                                         float* offset_y) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->GetScrollOffset(offset_x, offset_y);
  }
}
#endif
#endif  // #ifdef OHOS_PAGE_UP_DOWN

CefRefPtr<CefBinaryValue> CefBrowserHostBase::GetWebState() {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return NavigationStateSerializer::WriteNavigationStatus(*web_contents);
}

bool CefBrowserHostBase::RestoreWebState(
    const CefRefPtr<CefBinaryValue> state) {
  auto web_contents = GetWebContents();
  if (!web_contents || !state) {
    return false;
  }
  return NavigationStateSerializer::RestoreNavigationStatus(*web_contents,
                                                            state);
}

#if defined(OHOS_INPUT_EVENTS)
void CefBrowserHostBase::ScrollTo(float x, float y) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->ScrollTo(x, y);
  }
}

void CefBrowserHostBase::ScrollBy(float delta_x, float delta_y) {
  // By calling cc interface SetSynchronousInputHandlerRootScrollOffset,
  // sliding can be realized without waiting for rendering to be completed.
  if (!scrollable_ && scrollType_ != static_cast<int>(WebScrollType::EVENT)) {
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ScrollBy(delta_x, delta_y);
  }
}

void CefBrowserHostBase::SlideScroll(float vx, float vy) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->SlideScroll(vx, vy);
  }
}

void CefBrowserHostBase::ZoomBy(float delta, float width, float height) {
  if (delta < DEFAULT_MIN_ZOOM_FACTOR || delta > DEFAULT_MAX_ZOOM_FACTOR) {
    LOG(ERROR) << "invalid zommby delta";
    return;
  }
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->ZoomBy(delta, width, height);
  }
}

void CefBrowserHostBase::SetOverscrollMode(int overscrollMode) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->SetOverscrollMode(overscrollMode);
  }
}

void CefBrowserHostBase::SetScrollable(bool enable, int scrollType) {
  LOG(DEBUG) << "set scrollable: " << enable << ", scrollType = " << scrollType;
  scrollable_ = enable;
  scrollType_ = scrollType;

  if (platform_delegate_) {
    platform_delegate_->SetScrollable(enable);
  }

  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    if (scrollType == static_cast<int>(WebScrollType::UNKNOWN)) {
      static_cast<CefFrameHostImpl*>(frame.get())->SetScrollable(enable);
    } else if (scrollType == static_cast<int>(WebScrollType::EVENT)) {
      static_cast<CefFrameHostImpl*>(frame.get())->SetScrollable(true);
    }
  }
}

void CefBrowserHostBase::GetHitData(int& type, CefString& extra_data) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->GetHitData(type, extra_data);
  }
}

uint64_t CefBrowserHostBase::GetCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

void CefBrowserHostBase::UpdateDrawRect() {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->UpdateDrawRect();
  }
}

#if defined(OHOS_GET_SCROLL_OFFSET)
void CefBrowserHostBase::GetOverScrollOffset(float* offset_x,
                                             float* offset_y) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->GetOverScrollOffset(offset_x, offset_y);
  }
}
#endif

void CefBrowserHostBase::ScrollToWithAnime(float x, float y, int32_t duration) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->ScrollToWithAnime(x, y, duration);
  }
}

void CefBrowserHostBase::ScrollByWithAnime(float delta_x, float delta_y, int32_t duration) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->ScrollByWithAnime(delta_x, delta_y, duration);
  }
}
#endif  // defined(OHOS_INPUT_EVENTS)

#ifdef OHOS_NETWORK_CONNINFO
void CefBrowserHostBase::SetFileAccess(bool flag) {
  base::AutoLock lock_scope(state_lock_);
  if (file_access_ == flag) {
    return;
  }
  file_access_ = flag;
}

void CefBrowserHostBase::SetDisallowSandboxFileAccessFromFileUrl(bool disallow) {
  base::AutoLock lock_scope(state_lock_);
  if (dis_allow_sandbox_file_access_from_file_url_ == disallow) {
    return;
  }
  dis_allow_sandbox_file_access_from_file_url_  = disallow;
}

void CefBrowserHostBase::SetBlockNetwork(bool flag) {
  base::AutoLock lock_scope(state_lock_);
  if (network_blocked_ == flag) {
    return;
  }
  network_blocked_ = flag;
}

void CefBrowserHostBase::SetCacheMode(int flag) {
  base::AutoLock lock_scope(state_lock_);
  if (cache_mode_ == flag) {
    return;
  }
  cache_mode_ = flag;
}

void CefBrowserHostBase::SetGrantFileAccessDirs(
    const std::vector<CefString>& dir_list) {
  base::AutoLock lock_scope(state_lock_);
  file_access_dirs_list_ = dir_list;
}

bool CefBrowserHostBase::GetFileAccess() {
  base::AutoLock lock_scope(state_lock_);
  return file_access_;
}

bool CefBrowserHostBase::GetBlockNetwork() {
  base::AutoLock lock_scope(state_lock_);
  return network_blocked_;
}

int CefBrowserHostBase::GetCacheMode() {
  base::AutoLock lock_scope(state_lock_);
  return cache_mode_;
}

std::vector<std::string> CefBrowserHostBase::GetGrantFileAccessDirs() {
  base::AutoLock lock_scope(state_lock_);
  std::vector<std::string> dir_list;
  for (auto& dir: file_access_dirs_list_) {
    dir_list.emplace_back(dir.ToString());
  }
  return dir_list;
}
#endif

void CefBrowserHostBase::SetShouldFrameSubmissionBeforeDraw(bool should) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::ShowFreeCopyMenu() {
#if defined(OHOS_EX_FREE_COPY)
  if (!GetWebContents()) {
    return;
  }
  LOG(DEBUG) << "select and copy invoke";
  GetWebContents()->ShowFreeCopyMenu();
#endif
}

bool CefBrowserHostBase::ShouldShowFreeCopyMenu() {
#if defined(OHOS_EX_FREE_COPY)
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->ShouldShowFreeCopyMenu();
#else
  return false;
#endif
}

#if defined(OHOS_NO_STATE_PREFETCH)
void CefBrowserHostBase::PrefetchPage(CefString& url,
                                      CefString& additionalHttpHeaders) {
  if (!GetWebContents()) {
    return;
  }

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          GetWebContents()->GetBrowserContext());
  if (!no_state_prefetch_manager) {
    return;
  }
  content::SessionStorageNamespace* session_storage_namespace =
      GetWebContents()->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size size = GetWebContents()->GetContainerBounds().size();

  std::string prefetch_url = url;
  std::string additional_http_headers = additionalHttpHeaders;
  no_state_prefetch_manager->StartOhPrefetchingFromOmnibox(
      GURL(prefetch_url), session_storage_namespace, size, nullptr,
      additional_http_headers);
}
#endif  // defined(OHOS_NO_STATE_PREFETCH)

int CefBrowserHostBase::GetNWebId() {
#ifdef OHOS_EX_DOWNLOAD
  if (browser_info() && browser_info()->extra_info()) {
    auto nweb_id_value = browser_info()->extra_info()->GetValue(kNWebId);
    if (nweb_id_value) {
      return nweb_id_value->GetInt();
    }
  }
  return -1;
#else
  return -1;
#endif
}

#endif  // IS_OHOS

void CefBrowserHostBase::EnableVideoAssistant(bool enable) {
#if defined(OHOS_VIDEO_ASSISTANT)
  if (!GetWebContents()) {
    LOG(ERROR) << "failed to get content when enable video assistant";
    return;
  }

  GetWebContents()->EnableVideoAssistant(enable);
#endif  // defined(OHOS_VIDEO_ASSISTANT)
}

void CefBrowserHostBase::ExecuteVideoAssistantFunction(const CefString& cmdId) {
#if defined(OHOS_VIDEO_ASSISTANT)
  if (!GetWebContents()) {
    LOG(ERROR) << "failed to get content when execute video assistant function";
    return;
  }

  GetWebContents()->ExecuteVideoAssistantFunction(cmdId.ToString());
#endif  // defined(OHOS_VIDEO_ASSISTANT)
}

void CefBrowserHostBase::CustomWebMediaPlayer(bool enable) {
#if defined(OHOS_VIDEO_ASSISTANT)
  if (!GetWebContents()) {
    LOG(ERROR) << "failed to get content when enable custom web media player";
    return;
  }

  GetWebContents()->CustomWebMediaPlayer(enable);
#endif  // defined(OHOS_VIDEO_ASSISTANT)
}

#if defined(OHOS_MEDIA_POLICY)
void CefBrowserHostBase::CloseMedia() {
  if (is_fullscreen_) {
    ExitFullScreen();
    StopMedia();
  }
}

void CefBrowserHostBase::ResumeMedia() {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(GetWebContents());
  if (!mediaSession || !GetWebContents()) {
    LOG(ERROR) << "CefBrowserHostBase::ResumeMedia get mediaSession or webContents failed.";
    return;
  }
  mediaSession->Resume(content::MediaSession::SuspendType::kSystem);
  GetWebContents()->SetHtmlPlayEnabled(true);
}

void CefBrowserHostBase::PauseMedia() {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(GetWebContents());
  if (!mediaSession || !GetWebContents()) {
    LOG(ERROR) << "CefBrowserHostBase::PauseMedia get mediaSession or webContents failed.";
    return;
  }
  mediaSession->Suspend(content::MediaSession::SuspendType::kSystem);
  GetWebContents()->SetHtmlPlayEnabled(false);
}

void CefBrowserHostBase::StopMedia() {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(GetWebContents());
  if (!mediaSession) {
    LOG(ERROR) << "CefBrowserHostBase::StopMedia get mediaSession failed.";
    return;
  }
  mediaSession->Stop(content::MediaSession::SuspendType::kSystem);
}

int CefBrowserHostBase::GetMediaPlaybackState() {
  content::MediaSessionImpl* mediaSession = content::MediaSessionImpl::Get(GetWebContents());
  if (!mediaSession || !GetWebContents()) {
    LOG(ERROR) << "CefBrowserHostBase::GetMediaPlaybackState get mediaSession or webContents failed.";
    return static_cast<int>(content::MediaSessionImpl::NWebPlaybackState::NONE);
  }
  if (!GetWebContents()->IsHtmlPlayEnabled()) {
    return static_cast<int>(content::MediaSessionImpl::NWebPlaybackState::PAUSED);
  }
  content::MediaSessionImpl::NWebPlaybackState playbackState = mediaSession->NWebGetState();
  return static_cast<int>(playbackState);
}
#endif // defined(OHOS_MEDIA_POLICY)

// #if defined(OHOS_NWEB_EX)
// NOTE: Keep the previous line commented, add NWebEx APIs below.
bool CefBrowserHostBase::ShouldShowLoadingUI() {
  auto wc = GetWebContents();
  if (wc) {
    return wc->ShouldShowLoadingUI();
  }
  return false;
}

void CefBrowserHostBase::SetForceEnableZoom(bool forceEnableZoom) {
#if defined(OHOS_EX_FORCE_ZOOM)
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->SetForceEnableZoom(forceEnableZoom);
#endif
}

bool CefBrowserHostBase::GetForceEnableZoom() {
#if defined(OHOS_EX_FORCE_ZOOM)
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->GetForceEnableZoom();
#else
  return false;
#endif  // #if defined (OHOS_EX_FORCE_ZOOM)
}

void CefBrowserHostBase::SaveOrUpdatePassword(bool is_update) {
#if defined(OHOS_EX_PASSWORD)
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->SaveOrUpdatePassword(is_update);
#endif
}

#ifdef OHOS_ARKWEB_ADBLOCK
void CefBrowserHostBase::UpdateAdblockEasyListRules(
    long adBlockEasyListVersion) {
  adblock::UpdateAdblockEasyListRules(adBlockEasyListVersion);
}

void CefBrowserHostBase::EnableAdsBlock(bool enable) {
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->EnableAdsBlock(enable);

  LOG(INFO) << "web adblock enabled : " << enable;

  OHOS::adblock::AdBlockConfig::GetInstance()->ReadFromPrefService();
  if (!OHOS::adblock::AdBlockConfig::GetInstance()
           ->GetUserEasylistReplaceSwitch() &&
      enable) {
    LOG(INFO) << "[Adblock] enable cloud control for easylist";
    ohos_adblock::AdblockConfigBridge::GetInstance()->EnableAdsBlock(
        GetBrowserContext(), true);
  } else {
    ohos_adblock::AdblockConfigBridge::GetInstance()->EnableAdsBlock(
        GetBrowserContext(), false);
  }
}

bool CefBrowserHostBase::IsAdsBlockEnabled() {
  if (!GetWebContents()) {
    return false;
  }

  return GetWebContents()->IsAdsBlockEnabled();
}

bool CefBrowserHostBase::IsAdsBlockEnabledForCurPage() {
  if (!GetWebContents()) {
    return false;
  }

  return GetWebContents()->IsAdsBlockEnabledForCurPage();
}
#endif

void CefBrowserHostBase::SetSavePasswordAutomatically(bool enable) {
#if defined(OHOS_EX_PASSWORD)
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->SetSavePasswordAutomatically(enable);
#endif
}

bool CefBrowserHostBase::GetSavePasswordAutomatically() {
#if defined(OHOS_EX_PASSWORD)
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->GetSavePasswordAutomatically();
#else
  return false;
#endif
}

void CefBrowserHostBase::SetSavePassword(bool enable) {
#if defined(OHOS_EX_PASSWORD)
  if (!GetWebContents()) {
    return;
  }
  LOG(DEBUG) << "set save password " << enable;
  GetWebContents()->SetSavePassword(enable);
#endif
}

void CefBrowserHostBase::PasswordSuggestionSelected(int list_index) {
#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
  if (!GetWebContents()) {
    return;
  }
  autofill::OhAutofillClient* autofill_client =
      autofill::OhAutofillClient::FromWebContents(GetWebContents());
  if (autofill_client) {
    autofill_client->SuggestionSelected(list_index);
  }
#endif
}

#if BUILDFLAG(IS_OHOS)
  void CefBrowserHostBase::SetAutofillCallback(CefRefPtr<CefWebMessageReceiver> callback) {
    auto web_contents = GetWebContents();
    if (!web_contents) {
      LOG(ERROR) << "GetWebContents null";
      return;
    }

    autofill::OhAutofillClient* autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents);
    if (autofill_client) {
      autofill_client->SetOnMessageCallback(callback);
    }
  }

  void CefBrowserHostBase::FillAutofillData(CefRefPtr<CefValue> message) {
    auto web_contents = GetWebContents();
    if (!web_contents) {
      LOG(ERROR) << "GetWebContents null";
      return;
    }

    autofill::OhAutofillClient* autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents);
    if (autofill_client) {
      autofill_client->FillData(message);
    }
  }

  bool CefBrowserHostBase::IsSafeBrowsingEnabled() {
    return settings_.is_safe_browsing_enable;
  }

  void CefBrowserHostBase::EnableSafeBrowsing(bool enable) {
    LOG(INFO) << "enable safe browsing" << enable;
    if (settings_.is_safe_browsing_enable != enable) {
      settings_.is_safe_browsing_enable = enable;
    }
  }

  void CefBrowserHostBase::EnableSafeBrowsingDetection(bool enable,
                                                       bool strictMode) {
    if (!GetWebContents()) {
      return;
    }
    GetWebContents()->EnableSafeBrowsingDetection(enable, strictMode);
  }

  bool CefBrowserHostBase::IsSafeBrowsingDetectionEnabled() const {
    if (!GetWebContents()) {
      return false;
    }

    return GetWebContents()->IsSafeBrowsingDetectionEnabled();
  }
#endif

#if defined(OHOS_PASSWORD_AUTOFILL)
void CefBrowserHostBase::ProcessAutofillCancel(
    const std::string& fillContent) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }

  OhPasswordManagerClient* password_manager_client =
      OhPasswordManagerClient::FromWebContents(web_contents);
  if (password_manager_client) {
    password_manager_client->ProcessAutofillCancel(fillContent);
  }
}

void CefBrowserHostBase::AutoFillWithIMFEvent(bool is_username,
                                              bool is_other_account,
                                              bool is_new_password,
                                              const std::string& content) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }

  OhPasswordManagerClient* password_manager_client =
      OhPasswordManagerClient::FromWebContents(web_contents);
  if (password_manager_client) {
    password_manager_client->AutoFillWithIMFEvent(
        is_username, is_other_account, is_new_password, content);
  }
}
#endif

bool CefBrowserHostBase::GetSavePassword() {
#if defined(OHOS_EX_PASSWORD)
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->GetSavePassword();
#else
  return false;
#endif
}
// #endif // defined(OHOS_NWEB_EX)

int CefBrowserHostBase::GetTopControlsOffset() {
#if defined(OHOS_EX_TOPCONTROLS)
  if (platform_delegate_) {
    return platform_delegate_->GetTopControlsOffset();
  }
  return 0;
#else
  return 0;
#endif
}

int CefBrowserHostBase::GetShrinkViewportHeight() {
#if defined(OHOS_EX_TOPCONTROLS)
  if (platform_delegate_) {
    return platform_delegate_->GetShrinkViewportHeight();
  }
  return 0;
#else
  return 0;
#endif
}

#if BUILDFLAG(IS_OHOS)
int CefBrowserHostBase::GetSecurityLevel() {
  if (!GetWebContents()) {
    return static_cast<int>(security_state::SecurityLevel::NONE);
  }

  auto web_contents = GetWebContents();
  std::unique_ptr<security_state::VisibleSecurityState> state =
      security_state::GetVisibleSecurityState(web_contents);
  DCHECK(state);

  security_state::SecurityLevel securityValue =
      security_state::GetSecurityLevel(*state, false);
  return static_cast<int>(securityValue);
}
#endif  // BUILDFLAG(IS_OHOS)

#if defined(OHOS_EX_NAVIGATION)
int CefBrowserHostBase::InsertBackForwardEntry(int index,
                                               const CefString& url) {
  if (!GetWebContents()) {
    return static_cast<int>(
        content::NavigationController::NavigationEntryUpdateError::ERR_OTHER);
  }
  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid()) {
    return static_cast<int>(
        content::NavigationController::NavigationEntryUpdateError::ERR_OTHER);
  }
  return static_cast<int>(
      GetWebContents()->GetController().InsertBackForwardEntry(index, gurl));
}

int CefBrowserHostBase::UpdateNavigationEntryUrl(int index,
                                                 const CefString& url) {
  if (!GetWebContents()) {
    return static_cast<int>(
        content::NavigationController::NavigationEntryUpdateError::ERR_OTHER);
  }
  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid()) {
    return static_cast<int>(
        content::NavigationController::NavigationEntryUpdateError::ERR_OTHER);
  }
  return static_cast<int>(
      GetWebContents()->GetController().UpdateNavigationEntryUrl(index, gurl));
}
void CefBrowserHostBase::ClearForwardList() {
  if (!GetWebContents()) {
    return;
  }

  GetWebContents()->GetController().PruneForwardEntries();
}
#endif

#ifdef OHOS_ITP
void CefBrowserHostBase::EnableIntelligentTrackingPrevention(bool enable) {
  {
    base::AutoLock locker(lock_);
    intelligent_tracking_prevention_cookies_enabled_ = enable;
  }
  LOG(INFO) << "Intelligent tracking prevention cookies enabled " << enable;
  ohos_anti_tracking::ThirdPartyCookieAccessPolicy::GetInstance()->
      EnableIntelligentTrackingPrevention(GetBrowserContext(), enable);
}

bool CefBrowserHostBase::IsIntelligentTrackingPreventionEnabled() {
  base::AutoLock locker(lock_);
  return intelligent_tracking_prevention_cookies_enabled_;
}

GURL CefBrowserHostBase::GetLastCommittedURL() {
  GURL url;
  if (!GetWebContents()) {
    return url;
  }
  url = GetWebContents()->GetLastCommittedURL();
  return url;
}
#endif

#if defined(OHOS_SECURE_JAVASCRIPT_PROXY)
CefString CefBrowserHostBase::GetLastJavascriptProxyCallingFrameUrl() {
  return base::EmptyString();
}
#endif

void CefBrowserHostBase::StartCamera() {
#if defined(OHOS_WEBRTC)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  web_contents->StartCamera(web_contents->GetNWebId());
#endif  // defined(OHOS_WEBRTC)
}

void CefBrowserHostBase::StopCamera() {
#if defined(OHOS_WEBRTC)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  web_contents->StopCamera(web_contents->GetNWebId());
#endif  // defined(OHOS_WEBRTC)
}

void CefBrowserHostBase::CloseCamera() {
#if defined(OHOS_WEBRTC)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  web_contents->CloseCamera(web_contents->GetNWebId());
#endif  // defined(OHOS_WEBRTC)
}

void CefBrowserHostBase::SetNWebId(int NWebID) {
#if defined(OHOS_WEBRTC)
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }
  web_contents->SetNWebId(NWebID);
#endif  // defined(OHOS_WEBRTC)
}

#ifdef OHOS_RENDER_PROCESS_MODE
void CefBrowserHostBase::SetNeedsReload(bool needs_reload) {}

bool CefBrowserHostBase::NeedsReload() {
  return false;
}
#endif

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::PrecompileJavaScript(const std::string& url,
                                              const std::string& script,
                                              CefRefPtr<CefCacheOptions> cacheOptions,
                                              CefRefPtr<CefPrecompileCallback> callback) {
  std::map<std::string, std::string> responseHeaders;

  cef_string_map_t cefResponseHeaders = cacheOptions->GetResponseHeaders();
  CefString key;
  CefString value;
  size_t size = cef_string_map_size(cefResponseHeaders);
  for (size_t i = 0 ; i < size ; i ++) {
    cef_string_map_key(cefResponseHeaders, i, key.GetWritableStruct());
    cef_string_map_value(cefResponseHeaders, i, value.GetWritableStruct());
    responseHeaders.emplace(key.ToString(), value.ToString());
  }

  auto options = std::make_shared<oh_code_cache::CacheOptions>(responseHeaders);

  oh_code_cache::TaskRunner::GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CefBrowserHostBase::WriteResponseCache, this, url, script, options),
      base::BindOnce(&CefBrowserHostBase::OnDidWriteResponseCache,
          this, url, script, options, std::move(callback)));
}

oh_code_cache::NextOp CefBrowserHostBase::WriteResponseCache(
    const std::string& url,
    const std::string& script,
    std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions) {
  auto response_cache = oh_code_cache::ResponseCache::CreateResponseCache(url);

  if (!response_cache) {
    LOG(DEBUG) << "Create response cache error.";
    return oh_code_cache::NextOp::THROW_ERROR;
  }

  return response_cache->Write(cacheOptions->response_headers_, script);
}

void CefBrowserHostBase::OnDidWriteResponseCache(const std::string& url,
                                                 const std::string& script,
                                                 std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions,
                                                 CefRefPtr<CefPrecompileCallback> callback,
                                                 oh_code_cache::NextOp nextOp) {
  switch (nextOp) {
    case oh_code_cache::NextOp::WRITE_CODE_CACHE:
      GenerateCodeCache(url, script, cacheOptions, std::move(callback));
      break;
    case oh_code_cache::NextOp::THROW_ERROR:
      callback->OnPrecompileFinished(static_cast<int32_t>(oh_code_cache::CacheError::INTERNAL_ERROR));
      break;
    default:
      callback->OnPrecompileFinished(static_cast<int32_t>(oh_code_cache::CacheError::NO_ERROR));
      break;
  }
}

void CefBrowserHostBase::GenerateCodeCache(const std::string& url,
                                           const std::string& script,
                                           std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions,
                                           CefRefPtr<CefPrecompileCallback> callback) {
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "WebContents has not initialized.";
    callback->OnPrecompileFinished(
        static_cast<int32_t>(oh_code_cache::CacheError::INTERNAL_ERROR));
    return;
  }

  wc->GetPrimaryMainFrame()->GenerateCodeCache(url, script, cacheOptions,
      base::BindOnce(&CefBrowserHostBase::OnDidGenerateCodeCache,
      this, std::move(callback)));
}

void CefBrowserHostBase::OnDidGenerateCodeCache(CefRefPtr<CefPrecompileCallback> callback, int32_t result) {
  LOG(DEBUG) << "Get generate code cache result: " << result;
  callback->OnPrecompileFinished(result);
}
#endif

#ifdef OHOS_AI
void CefBrowserHostBase::OnTextSelected(bool flag) {
  // TODO(ohos): please impl the function and remove this comment.
}

void CefBrowserHostBase::OnDestroyImageAnalyzerOverlay() {
  // TODO(ohos): please impl the function and remove this comment.
}

float CefBrowserHostBase::GetPageScaleFactor() {
  if (platform_delegate_) {
    return platform_delegate_->GetPageScaleFactor();
  }
  return 1;
}
#endif

#ifdef OHOS_DISPLAY_CUTOUT
void CefBrowserHostBase::OnSafeInsetsChange(int left,
                                            int top,
                                            int right,
                                            int bottom) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::OnSafeInsetsChange, this,
                                 left, top, right, bottom));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->OnSafeInsetsChange(left, top, right, bottom);
  }
}

void CefBrowserHostBase::NotifyForNextTouchEvent() {
  // TODO(ohos): please impl the function and remove this comment.
}
#endif

#ifdef OHOS_AI
void CefBrowserHostBase::OnFoldStatusChanged(uint32_t foldstatus) {
  // TODO(ohos): please impl the function and remove this comment.
}
#endif

#if defined(OHOS_SOFTWARE_COMPOSITOR)
bool CefBrowserHostBase::WebPageSnapshot(
    const char* id,
    int width,
    int height,
    cef_web_snapshot_callback_t callback) {
  if (platform_delegate_) {
    return platform_delegate_->WebPageSnapshot(id, width, height, std::move(callback));
  }
  return false;
}
#endif

#if OHOS_URL_TRUST_LIST
int CefBrowserHostBase::SetUrlTrustListWithErrMsg(
  const CefString& urlTrustList, CefString& detailErrMsg) {
  std::string urlTrustListUpdated = urlTrustList.ToString();
  content::WebContents* webContents = GetWebContents();
  std::string detailErrMsgUpdated;
  if (!webContents) {
    LOG(ERROR) << "SetUrlTrustListWithErrMsg failed, web contents is error.";
    return static_cast<int>(ohos_safe_browsing::UrlListSetResult::INIT_ERROR);
  }
  ohos_safe_browsing::UrlTrustListManager* manager =
    reinterpret_cast<ohos_safe_browsing::UrlTrustListManager *>(
    webContents->GetUserData(
      &ohos_safe_browsing::UrlTrustListInterface::interfaceKey));
  if (!manager) {
    manager = new ohos_safe_browsing::UrlTrustListManager();
    if (!manager) {
      LOG(ERROR) << "SetUrlTrustListWithErrMsg failed, new UrlTrustListManager failed.";
      return static_cast<int>(ohos_safe_browsing::UrlListSetResult::INIT_ERROR);
    }
    webContents->SetUserData(
      &ohos_safe_browsing::UrlTrustListInterface::interfaceKey,
      std::unique_ptr<base::SupportsUserData::Data>(manager));
  }
  int res = static_cast<int>(manager->SetUrlTrustListWithErrMsg(
    urlTrustListUpdated, detailErrMsgUpdated));
  detailErrMsg.FromString(detailErrMsgUpdated);
  return res;
}
#endif

#if defined(OHOS_ARKWEB_EXTENSIONS)
void CefBrowserHostBase::SetTabId(int tab_id) {}
int CefBrowserHostBase::GetTabId() { return -1; }

void CefBrowserHostBase::WebExtensionTabUpdated(
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
      [&changed_properties] (const CefString& name) {
    changed_properties.emplace_back(name.ToString());
  });

  extensions::cef::TabsWindowsAPI::Get(browser_context)->TabUpdated(
      tab_id, web_contents, changed_properties, url.ToString());
}

void CefBrowserHostBase::WebExtensionTabUpdated(
    int tab_id,
    const std::vector<CefString>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "TabUpdated get contents failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "TabUpdated get contents failed.";
#endif

    return;
  }

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "TabUpdated get browser context failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "TabUpdated get browser contents failed.";
#endif

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

void CefBrowserHostBase::WebExtensionTabActivated(int tab_id, int window_id) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "TabActivated get contents failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "TabActivated get contents failed.";
#endif

    return;
  }
  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "TabActivated get browser context failed.";

#ifdef OHOS_LOGGER_REPORT
    LOG_FEEDBACK(ERROR) << "TabActivated get browser contents failed.";
#endif

    return;
  }
  extensions::cef::TabsWindowsAPI::Get(browser_context)
      ->TabActived(tab_id, window_id, web_contents);
}

void CefBrowserHostBase::WebExtensionActionClicked(
    std::string extensionId,
    const NWebExtensionTab* tab) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    LOG(ERROR) << "WebExtensionActionClicked get web_contents failed.";

  auto browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    LOG(ERROR) << "WebExtensionActionClicked get browser_context failed.";

  extensions::ExtensionActionAPI::Get(browser_context)
      ->DispatchExtensionActionClickedWithCustomArgs(web_contents, extensionId,
                                                     tab);
}
#endif

#ifdef OHOS_BFCACHE
void CefBrowserHostBase::SetBackForwardCacheOptions(int32_t size, int32_t timeToLive) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "SetBackForwardCacheOptions failed to get web contents in CefBrowserHostBase.";
    return;
  }

  LOG(INFO) << "SetBackForwardCacheOptions size: " << size
            << " timeToLive: " << timeToLive;
  content::NavigationController& controller = web_contents->GetController();
  controller.GetBackForwardCache().SetCacheSize(size);
  controller.GetBackForwardCache().SetTimeToLive(timeToLive);
}
#endif

#if defined(OHOS_INPUT_EVENTS)
void CefBrowserHostBase::ScrollFocusedEditableNodeIntoView() {
  // TODO(ohos): please impl the function and remove this comment.
}
#endif

#if defined(OHOS_USERAGENT)
std::string CefBrowserHostBase::GetCustomUserAgent() {
  return custom_user_agent_;
}
#endif

void CefBrowserHostBase::ScaleGestureChangeV2(int type, float scale, float originScale, float centerX, float centerY)
{
  if (platform_delegate_)
  {
    platform_delegate_->ScaleGestureChangeV2(type, scale, originScale, centerX, centerY);
  }
}

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::SetOptimizeParserBudgetEnabled(bool enable) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->SetOptimizeParserBudgetEnabled(enable);
  }
}
#endif

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
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
#endif // OHOS_DISPATCH_BEFORE_UNLOAD

#ifdef OHOS_NOTIFICATION
void CefBrowserHostBase::GetPermissionStatusAsync(
    const CefString& origin,
    cef_permission_status_query_callback_t callback) {
  CefPermissionQuery::GetPermissionStatusAsync(
      new AlloyAccessQuery(origin, AlloyAccessRequest::Resources::NOTIFICATION,
                           std::move(callback)));
}
#endif // OHOS_NOTIFICATION
