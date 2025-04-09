// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/devtools/devtools_manager.h"
#include "libcef/browser/file_dialog_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/media_stream_registrar.h"
#include "libcef/browser/request_context_impl.h"

#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "extensions/common/mojom/view_type.mojom.h"

#if BUILDFLAG(IS_OHOS)
#include <map>
#include <set>
#include <unordered_map>

#include "include/cef_image.h"
#include "include/cef_permission_request.h"
#include "include/cef_task.h"
#include "libcef/browser/permission/alloy_permission_request_handler.h"
#endif

#include "components/download/public/common/download_item.h"

#if BUILDFLAG(IS_OHOS)
#include "components/js_injection/browser/js_communication_host.h"
#endif //IS_OHOS

#ifdef OHOS_DEVTOOLS
#include "include/cef_devtools_message_handler_delegate.h"
#endif // OHOS_DEVTOOLS

namespace extensions {
class Extension;
}

#if BUILDFLAG(IS_OHOS)
class AlloyPermissionRequestHandler;
#endif

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
    if (that.window_info) {
      MaybeSetWindowInfo(*that.window_info);
    }
    browser_view = that.browser_view;
    return *this;
  }

  // Set |window_info| if appropriate (see below).
  void MaybeSetWindowInfo(const CefWindowInfo& window_info);

  // Platform-specific window creation info. Will be nullptr for Views-hosted
  // browsers except when using the Chrome runtime with a native parent handle.
  std::unique_ptr<CefWindowInfo> window_info;

  // The BrowserView that will own a Views-hosted browser. Will be nullptr for
  // popup browsers.
  CefRefPtr<CefBrowserView> browser_view;

  // True if this browser is a popup and has a Views-hosted opener, in which
  // case the BrowserView for this browser will be created later (from
  // PopupWebContentsCreated).
  bool popup_with_views_hosted_opener = false;

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

#if BUILDFLAG(IS_OHOS)
class WebMessageReceiverImpl : public blink::WebMessagePort::MessageReceiver {
 public:
  WebMessageReceiverImpl() = default;
  ~WebMessageReceiverImpl();

  // WebMessagePort::MessageReceiver implementation:
  bool OnMessage(blink::WebMessagePort::Message message) override;

  void SetOnMessageCallback(CefRefPtr<CefWebMessageReceiver> callback);
  void ConvertBlinkMsgToCefValue(blink::WebMessagePort::Message& message,
                                 CefRefPtr<CefValue> data);

 private:
  CefRefPtr<CefWebMessageReceiver> callback_;
};
#endif

// Base class for CefBrowserHost implementations. Includes functionality that is
// shared by the alloy and chrome runtimes. All methods are thread-safe unless
// otherwise indicated.
class CefBrowserHostBase : public CefBrowserHost,
                           public CefBrowser,
                           public CefBrowserContentsDelegate::Observer
#if BUILDFLAG(IS_OHOS)
    ,
                           public CefBrowserPermissionRequestDelegate
#endif
{
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
  // Returns the browser associated with the specified top-level window.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForTopLevelNativeWindow(
      gfx::NativeWindow owning_window);

  // Returns the browser most likely to be focused. This may be somewhat iffy
  // with windowless browsers as there is no guarantee that the client has only
  // one browser focused at a time.
  static CefRefPtr<CefBrowserHostBase> GetLikelyFocusedBrowser();

#ifdef OHOS_NOTIFICATION
  static void GetPermissionStatusAsync(const CefString& origin,
                                       cef_permission_status_query_callback_t callback);
#endif // OHOS_NOTIFICATION

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

  // Returns true if the browser is in the process of being destroyed. Called on
  // the UI thread only.
  virtual bool WillBeDestroyed() const = 0;

  // Called on the UI thread after the associated WebContents is destroyed.
  // Also called from CefBrowserInfoManager::DestroyAllBrowsers if the browser
  // was not properly shut down.
  virtual void DestroyBrowser();

  // CefBrowserHost methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefClient> GetClient() override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  bool HasView() override;
  void SetFocus(bool focus) override;
  void RunFileDialog(FileDialogMode mode,
                     const CefString& title,
                     const CefString& default_file_path,
                     const std::vector<CefString>& accept_filters,
                     CefRefPtr<CefRunFileDialogCallback> callback) override;
  void StartDownload(const CefString& url) override;
  void DownloadImage(const CefString& image_url,
                     bool is_favicon,
                     uint32 max_image_size,
                     bool bypass_cache,
                     CefRefPtr<CefDownloadImageCallback> callback) override;
  void Print() override;
  void PrintToPDF(const CefString& path,
                  const CefPdfPrintSettings& settings,
                  CefRefPtr<CefPdfPrintCallback> callback) override;
  void CreateToPDF(const CefPdfPrintSettings& settings,
                   CefRefPtr<CefPdfValueCallback> callback) override;
  void ReplaceMisspelling(const CefString& word) override;
  void AddWordToDictionary(const CefString& word) override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendTouchpadFlingEvent(const CefMouseEvent& event, double vx, double vy) override;
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
  void NotifyMoveOrResizeStarted() override;

#ifdef OHOS_DEVTOOLS
  void ShowDevToolsWith(
      CefRefPtr<CefBrowserHost> frontend_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
      const CefPoint& inspect_element_at) override {}
#endif // OHOS_DEVTOOLS

#if BUILDFLAG(IS_OHOS)
  /* ohos webview begin */
  void PostTaskToUIThread(CefRefPtr<CefTask> task) override;
  void SetWebPreferences(const CefBrowserSettings& browser_settings) override;
  void PutUserAgent(const CefString& ua) override;
  CefString DefaultUserAgent() override;
  void UpdateBrowserSettings(const CefBrowserSettings& browser_settings);
  void RegisterNativeJSProxy(const CefString& object_name,
                             const std::vector<CefString>& method_list,
                             const int32_t object_id,
                             bool is_async,
                             const CefString& permission) override;

#ifdef OHOS_ARKWEB_ADBLOCK
  void UpdateAdblockEasyListRules(
      long adBlockEasyListVersion) override;

  bool IsAdsBlockEnabled() override;

  bool IsAdsBlockEnabledForCurPage() override;

  void EnableAdsBlock(bool enable) override;

  void SetAdBlockEnabledForSite(
      bool is_adblock_enabled,
      int main_frame_tree_node_id) override;
#endif

  void RegisterArkJSfunction(const CefString& object_name,
                             const std::vector<CefString>& method_list,
                             const std::vector<CefString>& async_method_list,
                             const int32_t object_id,
                             const CefString& permission) override;
  void UnregisterArkJSfunction(
      const CefString& object_name,
      const std::vector<CefString>& method_list) override;
  void CallH5Function(int32_t routing_id,
                      int32_t h5_object_id,
                      const CefString& h5_method_name,
                      const std::vector<CefRefPtr<CefValue>>& args) override;
  void OnWebPreferencesChanged();
  bool CanStoreWebArchive() override;
  void ReloadOriginalUrl() override;
  void StoreWebArchive(
      const CefString& base_name,
      bool auto_name,
      CefRefPtr<CefStoreWebArchiveResultCallback> callback) override;
  void GetImageForContextNode() override;
  void GetImageFromCache(const CefString& url) override;
  void SetBrowserUserAgentString(const CefString& user_agent) override;
  void ExitFullScreen() override;
  void GetRootBrowserAccessibilityManager(void** manager) override;
#ifdef OHOS_I18N
  void UpdateLocale(const CefString& locale) override;
  void UpdateNavigatorLanguage(const CefString& locale) override;
#endif  // OHOS_I18N
#ifdef OHOS_NETWORK_CONNINFO
  CefString GetOriginalUrl() override;
  void PutNetworkAvailable(bool available) override;
#endif
  void RemoveCache(bool include_disk_files) override;
  void SetVirtualKeyBoardArg(int32_t width,
                             int32_t height,
                             double keyboard) override;
  bool ShouldVirtualKeyboardOverlay() override;

#if defined(OHOS_JSPROXY)
  void JavaScriptOnDocumentStart(
      const CefString& script,
      const std::vector<CefString>& script_rules) override;
  void RemoveJavaScriptOnDocumentStart() override;
  void JavaScriptOnDocumentEnd(
      const CefString& script,
      const std::vector<CefString>& script_rules) override;
  void JavaScriptOnHeadReady(
      const CefString& script,
      const std::vector<CefString>& script_rules) override;
  void RemoveJavaScriptOnHeadReady() override;
  void RemoveJavaScriptOnDocumentEnd() override;
#endif

  void SetDrawRect(int x, int y, int width, int height) override;
  void SetDrawMode(int mode) override;
  void SetFitContentMode(int mode) override;
  bool GetPendingSizeStatus() override;
  void SetZoomLevel(double zoomLevel) override;
  double GetZoomLevel() override;
  void SetBrowserZoomLevel(double zoom_factor) override;
  void UpdateBrowserControlsState(int constraints,
                                  int current,
                                  bool animate) override;
  void UpdateBrowserControlsHeight(int height, bool animate) override;

  void StartCamera() override;
  void StopCamera() override;
  void CloseCamera() override;
  void StopScreenCapture(int32_t nweb_id, const CefString& session_id) override;
  void RegisterScreenCaptureDelegateListener(
      CefRefPtr<CefScreenCaptureCallback> listener) override;

  void SetNWebId(int NWebID) override;
  void PrecompileJavaScript(const std::string& url,
                            const std::string& script,
                            CefRefPtr<CefCacheOptions> cacheOptions,
                            CefRefPtr<CefPrecompileCallback> callback) override;
  void AdvanceFocusForIME(int focusType) override;
  void SetNativeEmbedMode(bool flag) override;
  void MaximizeResize() override;
  void SetNativeInnerWeb(bool isInnerWeb) override;
  /* ohos webview end */
#endif
#ifdef OHOS_NAVIGATION
  CefRefPtr<CefBinaryValue> GetWebState() override;
  bool RestoreWebState(const CefRefPtr<CefBinaryValue> state) override;
#endif

#ifdef OHOS_RENDER_PROCESS_MODE
void NotifyNeedsReload(bool needs_reload) override;
bool NeedsReload() override;
#endif

  // CefBrowser methods:
  bool IsValid() override;
  CefRefPtr<CefBrowserHost> GetHost() override;
  bool CanGoBack() override;
  void GoBack() override;
  bool CanGoForward() override;
  void GoForward() override;
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

#if BUILDFLAG(IS_OHOS)
  int GetSecurityLevel() override;
  void DeleteHistory() override;
  void SetFocusOnWeb() override;
  bool CanGoBackOrForward(int num_steps) override;
  void GoBackOrForward(int num_steps) override;
  CefRefPtr<CefBrowserPermissionRequestDelegate> GetPermissionRequestDelegate()
      override;
  CefRefPtr<CefGeolocationAcess> GetGeolocationPermissions() override;

  CefString Title() override;
  void SetInitialScale(float scale) override;
  void SetVirtualPixelRatio(float ratio) override;
  float GetVirtualPixelRatio() override;
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

  void ExecuteJSCallback(CefRefPtr<CefJavaScriptResultCallback> callback,
                         base::Value result);

  void ExecuteExtensionJSCallback(CefRefPtr<CefJavaScriptResultCallback> callback,
                                  base::Value result);

  void ExecuteJavaScript(const std::string& code,
                         CefRefPtr<CefJavaScriptResultCallback> callback,
                         bool extention) override;

  void ExecuteJavaScriptExt(const int fd,
                            const uint64 scriptLength,
                            CefRefPtr<CefJavaScriptResultCallback> callback,
                            bool extention) override;

  void ResumeDownload(const CefString& url,
                      const CefString& full_path,
                      int64 received_bytes,
                      int64 total_bytes,
                      const CefString& etag,
                      const CefString& mime_type,
                      const CefString& last_modified,
                      const CefString& received_slices_string) override;
#if defined(OHOS_EX_DOWNLOAD)
  CefRefPtr<CefDownloadItem> GetDownloadItem(uint32 item_id) override;
#endif
  void WasOccluded(bool occluded) override;
  void OnWindowShow() override;
  void OnWindowHide() override;
  void OnOnlineRenderToForeground() override;
  void WasKeyboardResized() override;
  void SetWindowId(int window_id, int nweb_id) override;
  void SetWakeLockHandler(int32_t windowId, CefRefPtr<CefSetLockCallback> callback) override;
#if defined(OHOS_PRINT)
  void SetToken(void* token) override;
  void CreateWebPrintDocumentAdapter(const CefString& jobName,
                                     void** webPrintDocumentAdapter) override;
  void SetPrintBackground(bool enable) override;
  bool GetPrintBackground() override;
#endif // defined(OHOS_PRINT)
  void SetEnableLowerFrameRate(bool enabled) override;
  void SetEnableHalfFrameRate(bool enabled) override;
  void SetAudioResumeInterval(int resumeInterval) override;
  void SetAudioExclusive(bool audioExclusive) override;
  void SendTouchEventList(const std::vector<CefTouchEvent>& event_list) override;
  void NotifyForNextTouchEvent() override;
#if defined(OHOS_INPUT_EVENTS)
  void ScrollFocusedEditableNodeIntoView() override;
#endif
#ifdef OHOS_PAGE_UP_DOWN
  void ScrollPageUpDown(bool is_up, bool is_half, float view_height) override;
#ifdef OHOS_GET_SCROLL_OFFSET
  void GetScrollOffset(float* offset_x, float* offset_y) override;
#endif
#endif  // OHOS_PAGE_UP_DOWN
#if defined(OHOS_INPUT_EVENTS)
  void ScrollTo(float x, float y) override;
  void ScrollBy(float delta_x, float delta_y) override;
  void SlideScroll(float vx, float vy) override;
  void ZoomBy(float delta, float width, float height) override;
  void GetHitData(int& type, CefString& extra_data) override;
  void GetLastHitData(int& type, CefString& extra_data) override;
  uint64_t GetCurrentTimestamp();
  void SetOverscrollMode(int overScrollMode) override;
  void SetScrollable(bool enable, int scrollType) override;
  void UpdateDrawRect() override;
  void ScrollToWithAnime(float x, float y, int32_t duration) override;
  void ScrollByWithAnime(float delta_x, float delta_y, int32_t duration) override;
#if defined(OHOS_GET_SCROLL_OFFSET)
  void GetOverScrollOffset(float* offset_x, float* offset_y) override;
#endif
#endif  // defined(OHOS_INPUT_EVENTS)
#ifdef OHOS_NETWORK_CONNINFO
  void SetFileAccess(bool falg) override;
  void SetBlockNetwork(bool falg) override;
  void SetDisallowSandboxFileAccessFromFileUrl(bool disallow) override;
  void SetCacheMode(int falg) override;
  void SetGrantFileAccessDirs(const std::vector<CefString>& dir_list) override;
#endif
  void SetShouldFrameSubmissionBeforeDraw(bool should) override;
  bool Discard() override { return false; }
  bool Restore() override { return false; }
  // #ifdef OHOS_EX_FREE_COPY
  void SelectAndCopy() override;
  bool ShouldShowFreeCopy() override;
  std::string GetSelectedTextFromContextParam() override;
  // #endif
  int GetNWebId() override;
#ifdef OHOS_ITP
  void EnableIntelligentTrackingPrevention(bool enable) override;
  bool IsIntelligentTrackingPreventionEnabled() override;
#endif
#endif  // BUILDFLAG(IS_OHOS)

  void EnableVideoAssistant(bool enable) override;
  void ExecuteVideoAssistantFunction(const CefString& cmdId) override;
  void CustomWebMediaPlayer(bool enable) override;

#if defined(OHOS_MEDIA_POLICY)
  void CloseMedia() override;
  void StopMedia() override;
  void ResumeMedia() override;
  void PauseMedia() override;
  int GetMediaPlaybackState() override;
#endif // defined(OHOS_MEDIA_POLICY)

#if defined(OHOS_NO_STATE_PREFETCH)
  void PrefetchPage(CefString& url, CefString& additionalHttpHeaders) override;
#endif  // defined(OHOS_NO_STATE_PREFETCH)

#if defined(OHOS_MSGPORT)
  void CreateWebMessagePorts(std::vector<CefString>& ports) override;
  void PostWebMessage(CefString& message,
                      std::vector<CefString>& ports,
                      CefString& targetUri) override;
  void ClosePort(CefString& port_handle) override;
  bool ConvertCefValueToBlinkMsg(CefRefPtr<CefValue>& original,
                                 blink::WebMessagePort::Message& message);
  void PostPortMessage(const CefString& port_handle,
                       CefRefPtr<CefValue> message) override;
  void SetPortMessageCallback(
      const CefString& port_handle,
      CefRefPtr<CefWebMessageReceiver> callback) override;
  void DestroyAllWebMessagePorts() override;
#endif  // defined(OHOS_MSGPORT)

  void SetAutofillCallback(CefRefPtr<CefWebMessageReceiver> callback) override;
  void FillAutofillData(CefRefPtr<CefValue> message) override;

#if defined(OHOS_PASSWORD_AUTOFILL)
  void ProcessAutofillCancel(const std::string& fillContent) override;
  void AutoFillWithIMFEvent(bool is_username,
                            bool is_other_account,
                            bool is_new_password,
                            const std::string& content) override;
#endif

  // #if defined(OHOS_NWEB_EX)
  // NOTE: Keep the previous line commented, add NWebEx APIs below.
  bool ShouldShowLoadingUI() override;

  void SetForceEnableZoom(bool forceEnableZoom) override;
  bool GetForceEnableZoom() override;
  // #ifdef OHOS_EX_BLANK_TARGET_POPUP_INTERCEPT
  void SetEnableBlankTargetPopupIntercept(bool enableBlankTargetPopup) override;
  // #endif
  // if defined(OHOS_EX_PASSWORD)
  void PasswordSuggestionSelected(int list_index) override;
  bool GetSavePasswordAutomatically() override;
  void SetSavePasswordAutomatically(bool enable) override;
  void SaveOrUpdatePassword(bool is_update) override;
  bool GetSavePassword() override;
  void SetSavePassword(bool enable) override;
  // #endif // defined(OHOS_EX_PASSWORD)
  // if defined(OHOS_EX_TOPCONTROLS)
  int GetTopControlsOffset() override;
  int GetShrinkViewportHeight() override;
  // #endif
  // #endif // defined(OHOS_NWEB_EX)

#if BUILDFLAG(IS_OHOS)
bool TerminateRenderProcess() override;
#endif

#if BUILDFLAG(IS_OHOS)
  bool IsSafeBrowsingEnabled() override;
  void EnableSafeBrowsing(bool enable) override;
  void EnableSafeBrowsingDetection(bool enable, bool strictMode) override;
  bool IsSafeBrowsingDetectionEnabled() const;
#endif

  // CefBrowserContentsDelegate::Observer methods:
  void OnStateChanged(CefBrowserContentsState state_changed) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

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
  virtual void OnSetFocus(cef_focus_source_t source) = 0;
  void ViewText(const std::string& text);

  // Calls CefFileDialogManager methods.
  void RunFileChooserForBrowser(
      const blink::mojom::FileChooserParams& params,
      CefFileDialogManager::RunFileChooserCallback callback);
  void RunSelectFile(ui::SelectFileDialog::Listener* listener,
                     std::unique_ptr<ui::SelectFilePolicy> policy,
                     ui::SelectFileDialog::Type type,
                     const std::u16string& title,
                     const base::FilePath& default_path,
                     const ui::SelectFileDialog::FileTypeInfo* file_types,
                     int file_type_index,
                     const base::FilePath::StringType& default_extension,
                     gfx::NativeWindow owning_window,
                     void* params);
  void SelectFileListenerDestroyed(ui::SelectFileDialog::Listener* listener);

  // Called from CefBrowserInfoManager::MaybeAllowNavigation.
  virtual bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                                    bool is_guest_view,
                                    const content::OpenURLParams& params);

  // Helpers for executing client callbacks. Must be called on the UI thread.
  void OnAfterCreated();
  void OnBeforeClose();
  void OnBrowserDestroyed();

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
  CefMediaStreamRegistrar* GetMediaStreamRegistrar();

  // Returns the Widget owner for the browser window. Only used with windowed
  // browsers.
  views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with Views-
  // based browsers.
  CefRefPtr<CefBrowserView> GetBrowserView() const;

  // Returns the top-level native window for this browser. With windowed
  // browsers this will be an aura::Window* on Aura platforms (Windows/Linux)
  // and an NSWindow wrapper object from native_widget_types.h on MacOS. With
  // windowless browsers this method will always return an empty value.
  gfx::NativeWindow GetTopLevelNativeWindow() const;

  // Returns true if this browser is currently focused. A browser is considered
  // focused when the top-level RenderFrameHost is in the parent chain of the
  // currently focused RFH within the frame tree. In addition, its associated
  // RenderWidgetHost must also be focused. With windowed browsers only one
  // browser should be focused at a time. With windowless browsers this relies
  // on the client to properly configure focus state.
  bool IsFocused() const;

  // Returns true if this browser is currently visible.
  virtual bool IsVisible() const;

#if BUILDFLAG(IS_OHOS)
  AlloyPermissionRequestHandler* GetPermissionRequestHandler() {
    return permission_request_handler_.get();
  }

  // CefBrowserPermissionRequestDelegate methods:
  void AskGeolocationPermission(const CefString& origin,
                                cef_permission_callback_t callback) override;
  void AbortAskGeolocationPermission(const CefString& origin) override;
  void NotifyGeolocationPermission(bool value,
                                   const CefString& origin) override;
#if defined(OHOS_SENSOR)
  void AskSensorsPermission(const CefString& origin,
                                cef_permission_callback_t callback) override;
  void AbortAskSensorsPermission(const CefString& origin) override;
#endif // defined(OHOS_SENSOR)
#ifdef OHOS_NOTIFICATION
  void AskNotificationPermission(const CefString& origin,
                                 cef_permission_callback_t callback) override;
  void AbortAskNotificationPermission(const CefString& origin) override;
#endif // OHOS_NOTIFICATION
  void AskProtectedMediaIdentifierPermission(
      const CefString& origin,
      cef_permission_callback_t callback) override;
  void AbortAskProtectedMediaIdentifierPermission(
      const CefString& origin) override;
  void AskMIDISysexPermission(const CefString& origin,
                              cef_permission_callback_t callback) override;
  void AbortAskMIDISysexPermission(const CefString& origin) override;

  void AskClipboardReadWritePermission(const CefString& origin,
                              cef_permission_callback_t callback) override;
  void AbortAskClipboardReadWritePermission(const CefString& origin) override;
  // Geolocation API support
  void PopupGeolocationPrompt(std::string origin,
                              cef_permission_callback_t callback);
  void RemoveGeolocationPrompt(std::string origin);
  void OnGeolocationShow(std::string origin);

  bool IsBase64Encoded(std::string encoding);
  std::string GetDataURI(const std::string& data);

  void SetNativeWindow(cef_native_window_t window) override;
  uint32_t GetAcceleratedWidget(bool isPopup) override;

  void SetWebDebuggingAccess(bool isEnableDebug) override;
  bool GetWebDebuggingAccess() override;

#ifdef OHOS_SCROLLBAR
  void UpdatePixelRatio(float ratio);
#endif

#ifdef OHOS_NETWORK_CONNINFO
  bool GetFileAccess();
  bool GetBlockNetwork();
  bool GetDisallowSandboxFileAccessFromFileUrl();
  int GetCacheMode();
  std::vector<std::string> GetGrantFileAccessDirs();
  bool file_access_ = false;
  bool network_blocked_ = false;
  bool dis_allow_sandbox_file_access_from_file_url_ = false;
  int cache_mode_ = 0;
  std::vector<CefString> file_access_dirs_list_{};
#endif

#if defined(OHOS_SECURE_JAVASCRIPT_PROXY)
  CefString GetLastJavascriptProxyCallingFrameUrl() override;
#endif
#endif  // IS_OHOS

#ifdef OHOS_DISPLAY_CUTOUT
  void OnSafeInsetsChange(int left, int top, int right, int bottom) override;
#endif

#ifdef OHOS_AI
  void OnTextSelected(bool flag) override;
  void OnDestroyImageAnalyzerOverlay() override;
  float GetPageScaleFactor() override;
  void OnFoldStatusChanged(uint32_t foldstatus) override;
#endif

#ifdef OHOS_URL_TRUST_LIST
  int SetUrlTrustListWithErrMsg(
    const CefString& urlTrustList, CefString& detailErrMsg) override;
#endif

#if defined(OHOS_SOFTWARE_COMPOSITOR)
  bool WebPageSnapshot(const char* id,
                       int width,
                       int height,
                       cef_web_snapshot_callback_t callback)override;
#endif

#if defined(OHOS_ARKWEB_EXTENSIONS)
  void WebExtensionTabUpdated(
      int tab_id,
      const std::vector<CefString>& changed_property_names,
      const CefString& url) override;

  void WebExtensionTabUpdated(
      int tab_id,
      const std::vector<CefString>& changed_property_names,
      std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) override;
  void SetTabId(int tab_id) override;
  int GetTabId() override;

  void WebExtensionTabActivated(int tab_id, int window_id) override;

  void WebExtensionActionClicked(
      std::string extensionId,
      const NWebExtensionTab* tab) override;
#endif

#ifdef OHOS_BFCACHE
  void SetBackForwardCacheOptions(int32_t size, int32_t timeToLive) override;
#endif

  void SetPopupWindow(cef_native_window_t popupWindow) override;

#ifdef OHOS_USERAGENT
  std::string GetCustomUserAgent();
#endif

  void ScaleGestureChangeV2(int type, float scale, float originScale, float centerX, float centerY) override;

#ifdef OHOS_EX_REFRESH_IFRAME
  bool IsIframe() override { return false; }
  void ReloadFocusedFrame() override {}
#endif

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
  bool NeedToFireBeforeUnloadOrUnloadEvents() override;
  void DispatchBeforeUnload() override;
#endif // OHOS_DISPATCH_BEFORE_UNLOAD

#if BUILDFLAG(IS_OHOS)
  void SetOptimizeParserBudgetEnabled(bool enable) override;
#endif

 #if defined(OHOS_MEDIA_AVSESSION)
  void PutWebMediaAVSessionEnabled(bool enable) override;
 #endif // OHOS_MEDIA_AVSESSION

 protected:
  bool EnsureDevToolsManager();
  void InitializeDevToolsRegistrationOnUIThread(
      CefRefPtr<CefRegistration> registration);

  // Called from LoadMainFrameURL to perform the actual navigation.
  virtual bool Navigate(const content::OpenURLParams& params);

  // Create the CefFileDialogManager if it doesn't already exist.
  bool EnsureFileDialogManager();

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

  // Used for creating and managing file dialogs.
  std::unique_ptr<CefFileDialogManager> file_dialog_manager_;

  // Volatile state accessed from multiple threads. All access must be protected
  // by |state_lock_|.
  base::Lock state_lock_;
  bool is_loading_ = false;

#if !BUILDFLAG(IS_OHOS)
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
#endif

  bool has_document_ = false;
  bool is_fullscreen_ = false;
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // Used for creating and managing DevTools instances.
  std::unique_ptr<CefDevToolsManager> devtools_manager_;

  std::unique_ptr<CefMediaStreamRegistrar> media_stream_registrar_;

 private:
#if BUILDFLAG(IS_OHOS)
  js_injection::JsCommunicationHost* GetJsCommunicationHost();
  void StoreWebArchiveInternal(
      CefRefPtr<CefStoreWebArchiveResultCallback> callback,
      const CefString& path);

  // ResumeDownloadWithId is callback param in DownloadManager::GetNextId to
  // resume an interrupted download.
  void ResumeDownloadWithId(
      const GURL &url, const base::FilePath &full_path, int64 received_bytes,
      int64 total_bytes, const std::string &etag, const std::string &mime_type,
      const std::string &last_modified,
      std::vector<download::DownloadItem::ReceivedSlice> received_slices,
      uint32_t next_id);

  bool UseLegacyGeolocationPermissionAPI();

  oh_code_cache::NextOp WriteResponseCache(
      const std::string& url,
      const std::string& script,
      std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions);

  void OnDidWriteResponseCache(const std::string& url,
                               const std::string& script,
                               std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions,
                               CefRefPtr<CefPrecompileCallback> callback,
                               oh_code_cache::NextOp nextOp);

  void GenerateCodeCache(const std::string& url,
                         const std::string& script,
                         std::shared_ptr<oh_code_cache::CacheOptions> cacheOptions,
                         CefRefPtr<CefPrecompileCallback> callback);

  void OnDidGenerateCodeCache(CefRefPtr<CefPrecompileCallback> callback, int32_t result);

  void SetPortMessageCallbackInternal(
    const CefString& portHandle,
    CefRefPtr<CefWebMessageReceiver> callback);
  void PostPortMessageInternal(const CefString& portHandle,
                                          CefRefPtr<CefValue> data);

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<std::string, cef_permission_callback_t> OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> unhandled_geolocation_prompts_;

  using MessagePipe = std::pair<blink::WebMessagePort, blink::WebMessagePort>;
  using PortHandle = std::pair<uint64_t, uint64_t>;
  base::Lock web_message_lock_;
  std::map<PortHandle, MessagePipe> portMap_ GUARDED_BY(web_message_lock_);
  std::set<std::string> postedPorts_ GUARDED_BY(web_message_lock_);
  std::unordered_map<std::string, std::shared_ptr<WebMessageReceiverImpl>>
      receiverMap_ GUARDED_BY(web_message_lock_);
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
          base::ThreadPool::CreateSequencedTaskRunner({});

#if defined(OHOS_INPUT_EVENTS)
  uint64_t last_zoom_time_ = 0;
  bool scrollable_ = true;
  int scrollType_ = -1;
#endif  // defined(OHOS_INPUT_EVENTS)

  CefRefPtr<CefGeolocationAcess> geolocation_permissions_;
  std::unique_ptr<AlloyPermissionRequestHandler> permission_request_handler_;

  cef_accelerated_widget_t widget_;
  cef_accelerated_widget_t popup_widget_;
  bool is_web_debugging_access_ = false;
  float virtual_pixel_ratio_ = 2.0;
  base::WeakPtrFactory<CefBrowserHostBase> weak_ptr_factory_;
  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;
  std::map<std::string, int> document_start_script_result_map_;
  std::map<std::string, int> document_end_script_result_map_;
  std::map<std::string, int> head_ready_script_result_map_;
#ifdef OHOS_ITP
  mutable base::Lock lock_;
  bool intelligent_tracking_prevention_cookies_enabled_ GUARDED_BY(lock_) = false;
#endif
#ifdef OHOS_USERAGENT
  std::string custom_user_agent_;
#endif
#endif  // IS_OHOS
  IMPLEMENT_REFCOUNTING(CefBrowserHostBase);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
