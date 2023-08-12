// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_host_base.h"

#include <tuple>

#include "cef/include/cef_parser.h"

#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/navigation_entry_impl.h"
#include "libcef/browser/permission/alloy_access_request.h"
#include "libcef/browser/permission/alloy_geolocation_access.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/favicon/core/favicon_url.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#if BUILDFLAG(IS_OHOS)
#include "content/public/browser/message_port_provider.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#endif
#include "content/public/browser/navigation_entry.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gl/nweb_native_window_tracker.h"

#if BUILDFLAG(IS_MAC)
#include "components/spellcheck/browser/spellcheck_platform.h"
#endif

#include "content/browser/web_contents/web_contents_impl.h"

#if BUILDFLAG(IS_OHOS)
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "content/public/common/mhtml_generation_params.h"
#include "libcef/browser/navigation_state_serializer.h"
#include "libcef/browser/javascript/oh_javascript_injector.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(OHOS_NWEB_EX)
#include "content/public/common/content_switches.h"
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
    if (adapter)
      return adapter->browser_;
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

using namespace NWEB;
// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::GetBrowserForHost(
    const content::RenderViewHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents = content::WebContents::FromRenderViewHost(
      const_cast<content::RenderViewHost*>(host));
  if (web_contents)
    return GetBrowserForContents(web_contents);
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
  if (web_contents)
    return GetBrowserForContents(web_contents);
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
    if (!render_frame_host)
      return nullptr;
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
  contents_delegate_->InitIconHelper();
  permission_request_handler_.reset(new AlloyPermissionRequestHandler(
      client_->GetPermissionRequest(), GetWebContents()));
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

  devtools_manager_.reset(nullptr);

  platform_delegate_.reset(nullptr);

  contents_delegate_->RemoveObserver(this);
  contents_delegate_->ObserveWebContents(nullptr);

  CefBrowserInfoManager::GetInstance()->RemoveBrowserInfo(browser_info_);
  browser_info_->SetBrowser(nullptr);
}

void CefBrowserHostBase::PostTaskToUIThread(CefRefPtr<CefTask> task) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::PostTaskToUIThread, this, task));
    return;
  }
  std::move(task)->Execute();
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

bool CefBrowserHostBase::HasView() {
  return is_views_hosted_;
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
  manager->DownloadUrl(std::move(params));
}

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

  if (!callback)
    return;

  GURL gurl = GURL(image_url.ToString());
  if (gurl.is_empty() || !gurl.is_valid())
    return;

  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

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

bool CefBrowserHostBase::SendDevToolsMessage(const void* message,
                                             size_t message_size) {
  if (!message || message_size == 0)
    return false;

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

  if (!EnsureDevToolsManager())
    return false;
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

  if (!EnsureDevToolsManager())
    return 0;
  return devtools_manager_->ExecuteDevToolsMethod(message_id, method, params);
}

CefRefPtr<CefRegistration> CefBrowserHostBase::AddDevToolsMessageObserver(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  if (!observer)
    return nullptr;
  auto registration = CefDevToolsManager::CreateRegistration(observer);
  InitializeDevToolsRegistrationOnUIThread(registration);
  return registration.get();
}

void CefBrowserHostBase::GetNavigationEntries(
    CefRefPtr<CefNavigationEntryVisitor> visitor,
    bool current_only) {
  DCHECK(visitor.get());
  if (!visitor.get())
    return;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostBase::GetNavigationEntries, this,
                                visitor, current_only));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

  content::NavigationController& controller = web_contents->GetController();
  const int total = controller.GetEntryCount();
  const int current = controller.GetCurrentEntryIndex();

  if (current_only) {
    // Visit only the current entry.
    CefRefPtr<CefNavigationEntryImpl> entry =
        new CefNavigationEntryImpl(controller.GetEntryAtIndex(current));
    visitor->Visit(entry.get(), true, current, total);
#if !BUILDFLAG(IS_OHOS)
    std::ignore = entry->Detach(nullptr);
#endif
  } else {
    // Visit all entries.
    bool cont = true;
    for (int i = 0; i < total && cont; ++i) {
      CefRefPtr<CefNavigationEntryImpl> entry =
          new CefNavigationEntryImpl(controller.GetEntryAtIndex(i));
      cont = visitor->Visit(entry.get(), (i == current), i, total);
#if !BUILDFLAG(IS_OHOS)
      std::ignore = entry->Detach(nullptr);
#endif
    }
  }
}

CefRefPtr<CefNavigationEntry> CefBrowserHostBase::GetVisibleNavigationEntry() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  content::NavigationEntry* entry = nullptr;
  auto web_contents = GetWebContents();
  if (web_contents)
    entry = web_contents->GetController().GetVisibleEntry();

  if (!entry)
    return nullptr;

  return new CefNavigationEntryImpl(entry);
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
  // TODO(ohos): Fix allow_universal_access_from_file_urls and
  // allow_file_access_from_file_urls
  // settings_.allow_universal_access_from_file_urls =
  //    browser_settings.universal_access_from_file_urls;
  // settings_.allow_file_access_from_file_urls =
  //    browser_settings.file_access_from_file_urls;
  /* ohos webview add*/
  settings_.force_dark_mode_enabled = browser_settings.force_dark_mode_enabled;
  settings_.dark_prefer_color_scheme_enabled = browser_settings.dark_prefer_color_scheme_enabled;
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
#if BUILDFLAG(IS_OHOS)
  settings_.hide_vertical_scrollbars = browser_settings.hide_vertical_scrollbars;
  settings_.hide_horizontal_scrollbars = browser_settings.hide_horizontal_scrollbars;
#endif
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

#if defined(OHOS_NWEB_EX)
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

void CefBrowserHostBase::RegisterArkJSfunction(
    const CefString& object_name,
    const std::vector<CefString>& method_list) {
  OhJavascriptInjector* javascriptInjector =
      OhJavascriptInjector::FromWebContents(GetWebContents());
  std::vector<std::string> method_vector;
  for (CefString method : method_list) {
    method_vector.push_back(method.ToString());
  }
  javascriptInjector->AddInterface(object_name.ToString(), method_vector);
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
#endif

void CefBrowserHostBase::ReplaceMisspelling(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::ReplaceMisspelling, this, word));
    return;
  }

  auto web_contents = GetWebContents();
  if (web_contents)
    web_contents->ReplaceMisspelling(word);
}

void CefBrowserHostBase::AddWordToDictionary(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostBase::AddWordToDictionary, this, word));
    return;
  }

  auto web_contents = GetWebContents();
  if (!web_contents)
    return;

  SpellcheckService* spellcheck = nullptr;
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (browser_context) {
    spellcheck = SpellcheckServiceFactory::GetForContext(browser_context);
    if (spellcheck)
      spellcheck->GetCustomDictionary()->AddWord(word);
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

  if (platform_delegate_)
    platform_delegate_->SendKeyEvent(event);
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
  return browser_info_->browser() == this;
}

CefRefPtr<CefBrowserHost> CefBrowserHostBase::GetHost() {
  return this;
}

bool CefBrowserHostBase::CanGoBack() {
  base::AutoLock lock_scope(state_lock_);
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "getWebContents falied, wc is null";
    return false;
  }
  return wc->GetController().CanGoBack();
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
    wc->GetController().GoBack();
  }
}

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

bool CefBrowserHostBase::CanGoForward() {
  base::AutoLock lock_scope(state_lock_);
  auto wc = GetWebContents();
  if (wc == nullptr) {
    LOG(ERROR) << "getWebContents falied, wc is null";
    return false;
  }
  return wc->GetController().CanGoForward();
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
  wc->GetMainFrame()->AllowInjectingJavaScript();
  std::string jscode(
      "{if(document.fullscreenElement){document.exitFullscreen()}}");
  wc->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(jscode),
                                        base::NullCallback());
}
#endif

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

  if (!callback)
    return;

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

void CefBrowserHostBase::UpdateLocale(const CefString& locale) {
  std::string update_locale = locale.ToString();
  if (!ui::ResourceBundle::HasSharedInstance() ||
      !ui::ResourceBundle::LocaleDataPakExists(update_locale)) {
    LOG(ERROR) << "CefBrowserHostBase::UpdateLocale !HasSharedInstance";
    return;
  }

  std::string origin_locale =
      ui::ResourceBundle::GetSharedInstance().GetLoadedLocaleForTesting();
  if (origin_locale == update_locale) {
    LOG(ERROR) << "CefBrowserHostBase::UpdateLocale no need to update locale";
    return;
  }
  std::string result =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          update_locale);
  if (result.empty()) {
    LOG(ERROR) << "browser host update locale failed";
    return;
  }
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->UpdateLocale(update_locale);
    g_browser_process->SetApplicationLocale(result);
  }
}

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
void CefBrowserHostBase::ScrollPageUpDown(bool is_up,
                                          bool is_half,
                                          float view_height) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->ScrollPageUpDown(is_up, is_half, view_height);
  }
}

void CefBrowserHostBase::ScrollTo(float x,
                                  float y) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->ScrollTo(x, y);
  }
}

void CefBrowserHostBase::ScrollBy(float delta_x,
                                  float delta_y) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->ScrollBy(delta_x, delta_y);
  }
}

void CefBrowserHostBase::SlideScroll(float vx,
                                     float vy) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
       ->SlideScroll(vx, vy);
  }
}

CefRefPtr<CefBinaryValue> CefBrowserHostBase::GetWebState() {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return NavigationStateSerializer::WriteNavigationStatus(*web_contents);
}

bool CefBrowserHostBase::RestoreWebState(const CefRefPtr<CefBinaryValue> state) {
  auto web_contents = GetWebContents();
  if (!web_contents || !state) {
    return false;
  }
  return NavigationStateSerializer::RestoreNavigationStatus(*web_contents, state);
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
    if (frame->GetName() == name)
      return frame;
  }
  return nullptr;
}

size_t CefBrowserHostBase::GetFrameCount() {
  return browser_info_->GetAllFrames().size();
}

void CefBrowserHostBase::GetFrameIdentifiers(std::vector<int64>& identifiers) {
  if (identifiers.size() > 0)
    identifiers.clear();

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty())
    return;

  identifiers.reserve(frames.size());
  for (const auto& frame : frames) {
    identifiers.push_back(frame->GetIdentifier());
  }
}

void CefBrowserHostBase::GetFrameNames(std::vector<CefString>& names) {
  if (names.size() > 0)
    names.clear();

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty())
    return;

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
  if (unhandled_geolocation_prompts_.empty())
    return;
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

CefRefPtr<CefFrame> CefBrowserHostBase::GetFrameForHost(
    const content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();
  if (!host)
    return nullptr;

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
    if (!url_util::FixupGURL(gurl))
      return false;

    web_contents->GetController().LoadURL(
        gurl, params.referrer, params.transition, params.extra_headers);
    return true;
  }
  return false;
}

void CefBrowserHostBase::OnDidFinishLoad(CefRefPtr<CefFrameHostImpl> frame,
                                         const GURL& validated_url,
                                         int http_status_code) {
  frame->RefreshAttributes();

  contents_delegate_->OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserHostBase::OnUpdateHitData(const int type,
                                         const CefString extra_data) {
  cef_hit_data_.type = type;
  cef_hit_data_.extra_data = extra_data;
}

void CefBrowserHostBase::ViewText(const std::string& text) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostBase::ViewText, this, text));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ViewText(text);
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
  for (auto& observer : observers_)
    observer.OnBrowserDestroyed(this);
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
  if (web_contents)
    return web_contents->GetBrowserContext();
  return nullptr;
}

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_OHOS)
views::Widget* CefBrowserHostBase::GetWindowWidget() const {
  CEF_REQUIRE_UIT();
  if (!platform_delegate_)
    return nullptr;
  return platform_delegate_->GetWindowWidget();
}

CefRefPtr<CefBrowserView> CefBrowserHostBase::GetBrowserView() const {
  CEF_REQUIRE_UIT();
  if (is_views_hosted_ && platform_delegate_)
    return platform_delegate_->GetBrowserView();
  return nullptr;
}
#endif  // defined(TOOLKIT_VIEWS)

bool CefBrowserHostBase::EnsureDevToolsManager() {
  CEF_REQUIRE_UIT();
  if (!contents_delegate_->web_contents())
    return false;

  if (!devtools_manager_) {
    devtools_manager_.reset(new CefDevToolsManager(this));
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

  if (!EnsureDevToolsManager())
    return;
  devtools_manager_->InitializeRegistrationOnUIThread(registration);
}

CefString CefBrowserHostBase::Title() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetTitle();
  }
  return "";
}

// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::CreateWebMessagePorts(std::vector<CefString>& ports) {
  auto web_contents = GetWebContents();
  if (web_contents) {
    std::vector<blink::WebMessagePort> portArr;
    web_contents->CreateWebMessagePorts(portArr);
    if (portArr.size() != 2) {
      LOG(ERROR) << "CreateWebMessagePorts size wrong";
      return;
    }
    uint64_t pointer0 = reinterpret_cast<uint64_t>(&portArr[0]);
    uint64_t pointer1 = reinterpret_cast<uint64_t>(&portArr[1]);
    portMap_[std::make_pair(pointer0, pointer1)] =
        std::make_pair(std::move(portArr[0]), std::move(portArr[1]));
    ports.emplace_back(std::to_string(pointer0));
    ports.emplace_back(std::to_string(pointer1));
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
    LOG(INFO) << "PostWebMessage port:" << port.ToString();
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
void CefBrowserHostBase::ClosePort(CefString& portHandle) {
  LOG(INFO) << "ClosePort ClosePort";
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "ClosePort GetWebContents null";
  }

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

  for (auto iter = runnerMap_.begin(); iter != runnerMap_.end(); ++iter) {
    if (portHandle.ToString().compare(iter->first) == 0) {
      runnerMap_.erase(iter);
      break;
    }
  }
  postedPorts_.erase(portHandle.ToString());
  LOG(INFO) << "ClosePort end";
}

void CefBrowserHostBase::PostPortMessage(CefString& portHandle,
                                         CefRefPtr<CefValue> data) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }

  blink::WebMessagePort::Message message;
  if (data->GetType() == VTYPE_STRING) {
    message = blink::WebMessagePort::Message(base::UTF8ToUTF16(data->GetString().ToString()));
  } else if (data->GetType() == VTYPE_BINARY) {
    CefRefPtr<CefBinaryValue> binValue = data->GetBinary();
    size_t len = binValue->GetSize();
    std::vector<uint8_t> arr(len);
    binValue->GetData(&arr[0], len, 0);
    message = blink::WebMessagePort::Message(std::move(arr));
  } else {
    LOG(ERROR) << "CefBrowserHostBase::PostPortMessage not support type";
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

// in the std::map<std::pair<uint64_t, uint64_t>, std::pair<WebMessagePort,
// WebMessagePort>> portMap_; first is the paif of port_handles. second is the
// WebMessagePort of the pipe.
void CefBrowserHostBase::SetPortMessageCallback(
    CefString& portHandle,
    CefRefPtr<CefWebMessageReceiver> callback) {
  auto web_contents = GetWebContents();
  if (!web_contents) {
    LOG(ERROR) << "GetWebContents null";
    return;
  }

  // get sequenced task runner
  std::string pointer0 = portHandle.ToString();
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  auto runner_it = runnerMap_.find(pointer0);
  if (runner_it != runnerMap_.end()) {
    sequenced_task_runner_ = runner_it->second;
  } else {
    sequenced_task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  }

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
  runnerMap_[pointer0] = sequenced_task_runner_;
  receiverMap_[pointer0] = webMsgReceiver;
}

void CefBrowserHostBase::DestroyAllWebMessagePorts() {
  LOG(INFO) << "clear all message ports";
  portMap_.clear();
  runnerMap_.clear();
  receiverMap_.clear();
  postedPorts_.clear();
}

WebMessageReceiverImpl::~WebMessageReceiverImpl() {
  LOG(INFO) << "WebMessageReceiverImpl destrory";
}

void WebMessageReceiverImpl::SetOnMessageCallback(
    CefRefPtr<CefWebMessageReceiver> callback) {
  LOG(INFO) << "WebMessageReceiverImpl::SetOnMessageCallback ";
  callback_ = callback;
}

// this will receive message from html5
bool WebMessageReceiverImpl::OnMessage(blink::WebMessagePort::Message message) {
  LOG(INFO) << "OnMessage start";
  // Pass the message on to the receiver.
  if (callback_) {
    CefRefPtr<CefValue> data = CefValue::Create();
    if (!message.data.empty()) {
      data->SetString(base::UTF16ToUTF8(message.data));
    } else {
      std::vector<uint8_t> vecBinary = message.array_buffer;
      CefRefPtr<CefBinaryValue> value =
        CefBinaryValue::Create(vecBinary.data(), vecBinary.size());
      data->SetBinary(value);
    }

    callback_->OnMessage(data);
  } else {
    LOG(ERROR) << "u should set callback to receive message";
  }
  return true;
}
#endif

void CefBrowserHostBase::GetHitData(int& type, CefString& extra_data) {
  type = cef_hit_data_.type;
  extra_data = cef_hit_data_.extra_data;
}

void CefBrowserHostBase::SetInitialScale(float scale) {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())
        ->SetInitialScale(scale / (100 / virtual_pixel_ratio_));
  }
}

void CefBrowserHostBase::SetVirtualPixelRatio(float ratio) {
  virtual_pixel_ratio_ = ratio;
}

float CefBrowserHostBase::GetVirtualPixelRatio() const {
  return virtual_pixel_ratio_;
}

float CefBrowserHostBase::Scale() {
  auto web_contents = GetWebContents();
  if (web_contents) {
    return static_cast<content::WebContentsImpl*>(web_contents)
        ->GetPrimaryPage()
        .page_scale_factor();
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
  dataBase = GetDataURI(dataBase);
  buildData.append(dataBase);
  GURL data_url = GURL(buildData);
  content::NavigationController::LoadURLParams loadUrlParams(data_url);

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
    LOG(INFO) << "load data with BaseUrl";
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
  buildData.append(dataBase);
  GURL data_url = GURL(buildData);
  content::NavigationController::LoadURLParams loadUrlParams(data_url);

  auto web_contents = GetWebContents();
  if (web_contents) {
    LOG(INFO) << "load data";
    web_contents->GetController().LoadURLWithParams(loadUrlParams);
  }
}

void CefBrowserHostBase::ExecuteJavaScript(
    const CefString& code,
    CefRefPtr<CefJavaScriptResultCallback> callback) {
  auto web_contents = GetWebContents();
  // enable inject javaScript
  web_contents->GetMainFrame()->AllowInjectingJavaScript();
  if (web_contents) {
    LOG(INFO) << "ExecuteJavaScript with callback";
    web_contents->GetMainFrame()->ExecuteJavaScript(
        code.ToString16(),
        base::BindOnce(
            [](CefRefPtr<CefJavaScriptResultCallback> callback,
               base::Value result) {
              LOG(INFO) << "javascript result callback enter";
              std::string json;
              base::JSONWriter::Write(result, &json);
              if (callback != nullptr) {
                callback->OnJavaScriptExeResult(json);
              }
            },
            callback));
  }
}

void CefBrowserHostBase::SetNativeWindow(cef_native_window_t window) {
  widget_ = NWebNativeWindowTracker::Instance().AddNativeWindow(window);
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

#if BUILDFLAG(IS_OHOS)
void CefBrowserHostBase::SetFileAccess(bool flag) {
  base::AutoLock lock_scope(state_lock_);
  if (file_access_ == flag) {
    return;
  }
  file_access_ = flag;
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
#endif

void CefBrowserHostBase::GetImageForContextNode() {
  auto frame = GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->GetImageForContextNode();
  }
}

void CefBrowserHostBase::GetImageFromCache(const CefString& url) {
  LOG(ERROR) << "CefBrowserHostBase::GetImageFromCache";
  auto web_contents = GetWebContents();
  auto frame = GetMainFrame();
  if (web_contents && frame && frame->IsValid()) {
    content::RenderFrameHost* rfh = web_contents->GetMainFrame();
    if (rfh) {
      LOG(ERROR) << "CefBrowserHostBase::GetImageFromCache";
      rfh->GetImageFromCache(
          url.ToString(),
          base::BindOnce(&CefFrameHostImpl::OnGetImageFromCache,
                         static_cast<CefFrameHostImpl*>(frame.get()),
                         url.ToString()));
    }
  }
}

#if BUILDFLAG(IS_OHOS)
bool CefBrowserHostBase::ShouldShowLoadingUI() {
  auto wc = GetWebContents();
  if (wc) {
    return wc->ShouldShowLoadingUI();
  }
  return false;
}

void CefBrowserHostBase::SetForceEnableZoom(bool forceEnableZoom) {
#if defined (OHOS_NWEB_EX)
  if (!GetWebContents()) {
    return;
  }
  GetWebContents()->SetForceEnableZoom(forceEnableZoom);
#endif
}

bool CefBrowserHostBase::GetForceEnableZoom() {
#if defined (OHOS_NWEB_EX)
  if (!GetWebContents()) {
    return false;
  }
  return GetWebContents()->GetForceEnableZoom();
#else
  return false;
#endif
}
#endif
