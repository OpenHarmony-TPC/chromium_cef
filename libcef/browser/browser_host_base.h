// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#pragma once
#include <map>
#include <set>
#include <unordered_map>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_image.h"
#include "include/cef_permission_request.h"
#include "include/cef_task.h"
#include "include/views/cef_browser_view.h"
#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/devtools/devtools_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/permission/alloy_permission_request_handler.h"
#include "libcef/browser/request_context_impl.h"

#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "components/download/public/common/download_item.h"
#if BUILDFLAG(IS_OHOS)
#include "components/js_injection/browser/js_communication_host.h"
#endif //IS_OHOS
namespace extensions {
class Extension;
}

class AlloyPermissionRequestHandler;

// Parameters that are passed to the runtime-specific Create methods.
struct CefBrowserCreateParams {
  CefBrowserCreateParams() {}

  // Copy constructor used with the chrome runtime only.
  CefBrowserCreateParams(const CefBrowserCreateParams& that) {
    operator=(that);
  }
  CefBrowserCreateParams& operator=(const CefBrowserCreateParams& that) {
    // Not all parameters can be copied.
    client = that.client;
    url = that.url;
    settings = that.settings;
    request_context = that.request_context;
    extra_info = that.extra_info;
#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_OHOS)
    browser_view = that.browser_view;
#endif
    return *this;
  }

  // Platform-specific window creation info. Will be nullptr when creating a
  // views-hosted browser. Currently used with the alloy runtime only.
  std::unique_ptr<CefWindowInfo> window_info;

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_OHOS)
  // The BrowserView that will own a Views-hosted browser. Will be nullptr for
  // popup browsers.
  CefRefPtr<CefBrowserView> browser_view;

  // True if this browser is a popup and has a Views-hosted opener, in which
  // case the BrowserView for this browser will be created later (from
  // PopupWebContentsCreated).
  bool popup_with_views_hosted_opener = false;
#endif

  // Client implementation. May be nullptr.
  CefRefPtr<CefClient> client;

  // Initial URL to load. May be empty. If this is a valid extension URL then
  // the browser will be created as an app view extension host.
  CefString url;

  // Browser settings.
  CefBrowserSettings settings;

  // Other browser that opened this DevTools browser. Will be nullptr for non-
  // DevTools browsers. Currently used with the alloy runtime only.
  CefRefPtr<CefBrowserHostBase> devtools_opener;

  // Request context to use when creating the browser. If nullptr the global
  // request context will be used.
  CefRefPtr<CefRequestContext> request_context;

  // Extra information that will be passed to
  // CefRenderProcessHandler::OnBrowserCreated.
  CefRefPtr<CefDictionaryValue> extra_info;

  // Used when explicitly creating the browser as an extension host via
  // ProcessManager::CreateBackgroundHost. Currently used with the alloy
  // runtime only.
  const extensions::Extension* extension = nullptr;
  extensions::mojom::ViewType extension_host_type =
      extensions::mojom::ViewType::kInvalid;
};

class WebMessageReceiverImpl : public blink::WebMessagePort::MessageReceiver {
 public:
  WebMessageReceiverImpl() = default;
  ~WebMessageReceiverImpl();

  // WebMessagePort::MessageReceiver implementation:
  bool OnMessage(blink::WebMessagePort::Message message) override;

  void SetOnMessageCallback(CefRefPtr<CefWebMessageReceiver> callback);
  void ConvertBlinkMsgToCefValue(blink::WebMessagePort::Message& message, CefRefPtr<CefValue> data);

 private:
  CefRefPtr<CefWebMessageReceiver> callback_;
};

// Base class for CefBrowserHost implementations. Includes functionality that is
// shared by the alloy and chrome runtimes. All methods are thread-safe unless
// otherwise indicated.
class CefBrowserHostBase : public CefBrowserHost,
                           public CefBrowser,
                           public CefBrowserContentsDelegate::Observer,
                           public CefBrowserPermissionRequestDelegate {
 public:
  // Interface to implement for observers that wish to be informed of changes
  // to the CefBrowserHostBase. All methods will be called on the UI thread.
  class Observer : public base::CheckedObserver {
   public:
    // Called before |browser| is destroyed. Any references to |browser| should
    // be cleared when this method is called.
    virtual void OnBrowserDestroyed(CefBrowserHostBase* browser) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Create a new CefBrowserHost instance of the current runtime type with
  // owned WebContents.
  static CefRefPtr<CefBrowserHostBase> Create(
      CefBrowserCreateParams& create_params);

  // Returns the browser associated with the specified RenderViewHost.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForHost(
      const content::RenderViewHost* host);
  // Returns the browser associated with the specified RenderFrameHost.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForHost(
      const content::RenderFrameHost* host);
  // Returns the browser associated with the specified WebContents.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForContents(
      const content::WebContents* contents);
  // Returns the browser associated with the specified global ID.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);

  CefBrowserHostBase(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefRequestContextImpl> request_context);

  CefBrowserHostBase(const CefBrowserHostBase&) = delete;
  CefBrowserHostBase& operator=(const CefBrowserHostBase&) = delete;

  // Called on the UI thread after the associated WebContents is created.
  virtual void InitializeBrowser();

  // Called on the UI thread when the OS window hosting the browser is
  // destroyed.
  virtual void WindowDestroyed() = 0;

  // Called on the UI thread after the associated WebContents is destroyed.
  // Also called from CefBrowserInfoManager::DestroyAllBrowsers if the browser
  // was not properly shut down.
  virtual void DestroyBrowser();

  void PostTaskToUIThread(CefRefPtr<CefTask> task) override;

  // CefBrowserHost methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefClient> GetClient() override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  bool HasView() override;
  void StartDownload(const CefString& url) override;
  void ResumeDownload(const CefString& url,
                      const CefString& full_path,
                      int64 received_bytes,
                      int64 total_bytes,
                      const CefString& etag,
                      const CefString& mime_type,
                      const CefString& last_modified,
                      const CefString& received_slices_string) override;

  void DownloadImage(const CefString& image_url,
                     bool is_favicon,
                     uint32 max_image_size,
                     bool bypass_cache,
                     CefRefPtr<CefDownloadImageCallback> callback) override;
  void ReplaceMisspelling(const CefString& word) override;
  void AddWordToDictionary(const CefString& word) override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  bool SendDevToolsMessage(const void* message, size_t message_size) override;
  int ExecuteDevToolsMethod(int message_id,
                            const CefString& method,
                            CefRefPtr<CefDictionaryValue> params) override;
  CefRefPtr<CefRegistration> AddDevToolsMessageObserver(
      CefRefPtr<CefDevToolsMessageObserver> observer) override;
  void GetNavigationEntries(CefRefPtr<CefNavigationEntryVisitor> visitor,
                            bool current_only) override;
  CefRefPtr<CefNavigationEntry> GetVisibleNavigationEntry() override;
  void PrefetchPage(CefString& url, CefString& additionalHttpHeaders) override;
#if BUILDFLAG(IS_OHOS)
  /* ohos webview begin */
  void SetWebPreferences(const CefBrowserSettings& browser_settings) override;
  void PutUserAgent(const CefString& ua) override;
  CefString DefaultUserAgent() override;
  void UpdateBrowserSettings(const CefBrowserSettings& browser_settings);
  void RegisterArkJSfunction(
      const CefString& object_name,
      const std::vector<CefString>& method_list) override;
  void UnregisterArkJSfunction(
      const CefString& object_name,
      const std::vector<CefString>& method_list) override;
  void JavaScriptOnDocumentStart(
      const CefString& script,
      const std::vector<CefString>& script_rules) override;
  void RemoveJavaScriptOnDocumentStart() override;
  void OnWebPreferencesChanged();
  void ReloadOriginalUrl() override;
  void StoreWebArchive(
      const CefString& base_name,
      bool auto_name,
      CefRefPtr<CefStoreWebArchiveResultCallback> callback) override;
  void GetImageForContextNode() override;
  void GetImageFromCache(const CefString& url) override;
  void SetBrowserUserAgentString(const CefString& user_agent) override;
  void ExitFullScreen() override;
  void UpdateLocale(const CefString& locale) override;
  CefString GetOriginalUrl() override;
  void PutNetworkAvailable(bool available) override;
  void RemoveCache(bool include_disk_files) override;
  CefRefPtr<CefBinaryValue> GetWebState() override;
  bool RestoreWebState(const CefRefPtr<CefBinaryValue> state) override;
  void ScrollPageUpDown(bool is_up, bool is_half, float view_height) override;
  void ScrollTo(float x, float y) override;
  void ScrollBy(float delta_x, float delta_y) override;
  void SlideScroll(float vx, float vy) override;
  void ZoomBy(float delta, float width, float height) override;
  void SetForceEnableZoom(bool forceEnableZoom) override;
  bool GetForceEnableZoom() override;
  void SetSavePasswordAutomatically(bool enable) override;
  bool GetSavePasswordAutomatically() override;
  void SetSavePassword(bool enable) override;
  void SaveOrUpdatePassword(bool is_udpate) override;
  void PasswordSuggestionSelected(int list_index) override;
  bool GetSavePassword() override;
  void SelectAndCopy() override;
  bool ShouldShowFreeCopy() override;
  int GetNWebId() override;
  void SetEnableBlankTargetPopupIntercept(bool enableBlankTargetPopup) override;
  void SetOverscrollMode(int overScrollMode) override;
  void UpdateBrowserControlsState(int constraints,
                                  int current,
                                  bool animate) override;
  void UpdateBrowserControlsHeight(int height, bool animate) override;

  /* ohos webview end */
#endif

  // CefBrowser methods:
  bool IsValid() override;
  CefRefPtr<CefBrowserHost> GetHost() override;
  bool CanGoBack() override;
  void GoBack() override;
  bool CanGoForward() override;
  void GoForward() override;
  bool CanGoBackOrForward(int num_steps) override;
  void GoBackOrForward(int num_steps) override;
  void DeleteHistory() override;
  bool IsLoading() override;
  void Reload() override;
  void ReloadIgnoreCache() override;
  void StopLoad() override;
  int GetIdentifier() override;
  bool IsSame(CefRefPtr<CefBrowser> that) override;
  bool HasDocument() override;
  bool IsPopup() override;
  CefRefPtr<CefFrame> GetMainFrame() override;
  CefRefPtr<CefFrame> GetFocusedFrame() override;
  CefRefPtr<CefFrame> GetFrame(int64 identifier) override;
  CefRefPtr<CefFrame> GetFrame(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<int64>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;
  CefRefPtr<CefBrowserPermissionRequestDelegate> GetPermissionRequestDelegate()
      override;
  CefRefPtr<CefGeolocationAcess> GetGeolocationPermissions() override;
#if BUILDFLAG(IS_OHOS)
  void CreateWebMessagePorts(std::vector<CefString>& ports) override;
  void PostWebMessage(CefString& message,
                      std::vector<CefString>& ports,
                      CefString& targetUri) override;
  void ClosePort(CefString& port_handle) override;
  void PostPortMessage(CefString& port_handle, CefRefPtr<CefValue> message) override;
  void SetPortMessageCallback(
      CefString& port_handle,
      CefRefPtr<CefWebMessageReceiver> callback) override;
  void DestroyAllWebMessagePorts() override;
#endif
  CefString Title() override;
  void GetHitData(int& type, CefString& extra_data) override;
  void SetInitialScale(float scale) override;
  void SetVirtualPixelRatio(float ratio) override;
  float GetVirtualPixelRatio() const;
  int PageLoadProgress() override;
  float Scale() override;
  void LoadWithDataAndBaseUrl(const CefString& baseUrl,
                              const CefString& data,
                              const CefString& mimeType,
                              const CefString& encoding,
                              const CefString& historyUrl) override;

  void LoadWithData(const CefString& data,
                    const CefString& mimeType,
                    const CefString& encoding) override;

  void ExecuteJavaScript(
      const CefString& code,
      CefRefPtr<CefJavaScriptResultCallback> callback,
      bool extention) override;

  // CefBrowserContentsDelegate::Observer methods:
  void OnStateChanged(CefBrowserContentsState state_changed) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  AlloyPermissionRequestHandler* GetPermissionRequestHandler() {
    return permission_request_handler_.get();
  }

  // CefBrowserPermissionRequestDelegate methods:
  void AskGeolocationPermission(const CefString& origin,
                                cef_permission_callback_t callback) override;
  void AbortAskGeolocationPermission(const CefString& origin) override;
  void NotifyGeolocationPermission(bool value,
                                   const CefString& origin) override;
  void AskProtectedMediaIdentifierPermission(
      const CefString& origin,
      cef_permission_callback_t callback) override;
  void AbortAskProtectedMediaIdentifierPermission(
      const CefString& origin) override;
  void AskMIDISysexPermission(const CefString& origin,
                              cef_permission_callback_t callback) override;
  void AbortAskMIDISysexPermission(const CefString& origin) override;

  // Geolocation API support
  void PopupGeolocationPrompt(std::string origin,
                              cef_permission_callback_t callback);
  void RemoveGeolocationPrompt(std::string origin);
  void OnGeolocationShow(std::string origin);

  // Returns the frame associated with the specified RenderFrameHost.
  CefRefPtr<CefFrame> GetFrameForHost(const content::RenderFrameHost* host);

  // Returns the frame associated with the specified global ID. See
  // documentation on RenderFrameHost::GetFrameTreeNodeId() for why the global
  // ID is preferred.
  CefRefPtr<CefFrame> GetFrameForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);

  // Manage observer objects. The observer must either outlive this object or
  // be removed before destruction. Must be called on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  // Methods called from CefFrameHostImpl.
  void LoadMainFrameURL(const content::OpenURLParams& params);
  void OnDidFinishLoad(CefRefPtr<CefFrameHostImpl> frame,
                       const GURL& validated_url,
                       int http_status_code);
  virtual void OnSetFocus(cef_focus_source_t source) = 0;
  void ViewText(const std::string& text);

  // Called from CefBrowserInfoManager::MaybeAllowNavigation.
  virtual bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                                    bool is_guest_view,
                                    const content::OpenURLParams& params);

  // Helpers for executing client callbacks. Must be called on the UI thread.
  void OnAfterCreated();
  void OnBeforeClose();
  void OnBrowserDestroyed();

  bool IsBase64Encoded(std::string encoding);
  std::string GetDataURI(const std::string& data);

  // Thread-safe accessors.
  const CefBrowserSettings& settings() const { return settings_; }
  CefRefPtr<CefClient> client() const { return client_; }
  scoped_refptr<CefBrowserInfo> browser_info() const { return browser_info_; }
  int browser_id() const;
  CefRefPtr<CefRequestContextImpl> request_context() const {
    return request_context_;
  }
  bool is_views_hosted() const { return is_views_hosted_; }
  SkColor GetBackgroundColor() const;

  // Returns true if windowless rendering is enabled.
  virtual bool IsWindowless() const;

  // Accessors that must be called on the UI thread.
  content::WebContents* GetWebContents() const;
  content::BrowserContext* GetBrowserContext() const;
  CefBrowserPlatformDelegate* platform_delegate() const {
    return platform_delegate_.get();
  }
  CefBrowserContentsDelegate* contents_delegate() const {
    return contents_delegate_.get();
  }

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_OHOS)
  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with Views-
  // based browsers.
  CefRefPtr<CefBrowserView> GetBrowserView() const;
#endif

  void SetNativeWindow(cef_native_window_t window) override;
  cef_accelerated_widget_t GetAcceleratedWidget();

  void SetWebDebuggingAccess(bool isEnableDebug) override;
  bool GetWebDebuggingAccess() override;

#if BUILDFLAG(IS_OHOS)
  void SetFileAccess(bool flag) override;
  void SetBlockNetwork(bool flag) override;
  void SetCacheMode(int flag) override;
  bool GetFileAccess();
  bool GetBlockNetwork();
  int GetCacheMode();
  bool file_access_ = false;
  bool network_blocked_ = false;
  int cache_mode_ = 0;
#endif

#if BUILDFLAG(IS_OHOS)
bool ConvertCefValueToBlinkMsg(CefRefPtr<CefValue>& original, blink::WebMessagePort::Message& message);
#endif

#if BUILDFLAG(IS_OHOS)
  bool ShouldShowLoadingUI() override;
#endif

 protected:
  bool EnsureDevToolsManager();
  void InitializeDevToolsRegistrationOnUIThread(
      CefRefPtr<CefRegistration> registration);
  // Called from LoadMainFrameURL to perform the actual navigation.
  virtual bool Navigate(const content::OpenURLParams& params);

  // Thread-safe members.
  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefRefPtr<CefRequestContextImpl> request_context_;
  const bool is_views_hosted_;

  // Only accessed on the UI thread.
  std::unique_ptr<CefBrowserContentsDelegate> contents_delegate_;

  // Observers that want to be notified of changes to this object.
  // Only accessed on the UI thread.
  base::ObserverList<Observer> observers_;

  // Volatile state accessed from multiple threads. All access must be protected
  // by |state_lock_|.
  base::Lock state_lock_;
  bool is_loading_ = false;
  bool has_document_ = false;
  bool is_fullscreen_ = false;
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // Used for creating and managing DevTools instances.
  std::unique_ptr<CefDevToolsManager> devtools_manager_;

 private:
#if BUILDFLAG(IS_OHOS)
  js_injection::JsCommunicationHost* GetJsCommunicationHost();
  void StoreWebArchiveInternal(
      CefRefPtr<CefStoreWebArchiveResultCallback> callback,
      const CefString& path);
  uint64_t GetCurrentTimestamp();

  // ResumeDownloadWithId is callback param in DownloadManager::GetNextId to
  // resume an interrupted download.
  void ResumeDownloadWithId(
      const GURL& url,
      const base::FilePath& full_path,
      int64 received_bytes,
      int64 total_bytes,
      const std::string& etag,
      const std::string& mime_type,
      const std::string& last_modified,
      std::vector<download::DownloadItem::ReceivedSlice> received_slices,
      uint32_t next_id);
#endif  // IS_OHOS
  bool UseLegacyGeolocationPermissionAPI();
  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<std::string, cef_permission_callback_t> OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> unhandled_geolocation_prompts_;

#if BUILDFLAG(IS_OHOS)
  using MessagePipe = std::pair<blink::WebMessagePort, blink::WebMessagePort>;
  using PortHandle = std::pair<uint64_t, uint64_t>;
  std::map<PortHandle, MessagePipe> portMap_;
  std::set<std::string> postedPorts_;
  std::unordered_map<std::string, scoped_refptr<base::SequencedTaskRunner>>
      runnerMap_;
  std::unordered_map<std::string, std::shared_ptr<WebMessageReceiverImpl>>
      receiverMap_;
  uint64_t last_zoom_time_ = 0;
  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;
  std::map<std::string, int> script_result_map_;
#endif

  CefRefPtr<CefGeolocationAcess> geolocation_permissions_;
  std::unique_ptr<AlloyPermissionRequestHandler> permission_request_handler_;
  IMPLEMENT_REFCOUNTING(CefBrowserHostBase);

  cef_accelerated_widget_t widget_;
  bool is_web_debugging_access_ = false;
  float virtual_pixel_ratio_ = 2.0;

#if BUILDFLAG(IS_OHOS)
  base::WeakPtrFactory<CefBrowserHostBase> weak_ptr_factory_;
#endif  // IS_OHOS
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
