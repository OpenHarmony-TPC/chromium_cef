// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_BROWSER_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_BROWSER_HOST_IMPL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/internal/cef_types.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/javascript_dialog_manager.h"
#include "libcef/browser/menu_manager.h"
#include "libcef/browser/request_context_impl.h"

#include "base/synchronization/lock.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/mojom/view_type.mojom-forward.h"

#ifdef OHOS_HTML_SELECT
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#endif

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
#include "components/autofill/core/browser/ui/suggestion.h"
#endif

#ifdef OHOS_EX_GET_ZOOM_LEVEL
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_observer.h"
#endif

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
#include "include/cef_custom_media_player_delegate.h"
#endif // OHOS_CUSTOM_VIDEO_PLAYER

class CefAudioCapturer;
class CefBrowserInfo;
class SiteInstance;

// CefBrowser implementation for the alloy runtime. Method calls are delegated
// to the CefPlatformDelegate or the WebContents as appropriate. All methods are
// thread-safe unless otherwise indicated.
//
// WebContentsDelegate: Interface for handling WebContents delegations. There is
// a one-to-one relationship between AlloyBrowserHostImpl and WebContents
// instances.
//
// WebContentsObserver: Interface for observing WebContents notifications and
// IPC messages. There is a one-to-one relationship between WebContents and
// RenderViewHost instances. IPC messages received by the RenderViewHost will be
// forwarded to this WebContentsObserver implementation via WebContents. IPC
// messages sent using AlloyBrowserHostImpl::Send() will be forwarded to the
// RenderViewHost (after posting to the UI thread if necessary). Use
// WebContentsObserver::routing_id() when sending IPC messages.
class AlloyBrowserHostImpl : public CefBrowserHostBase,
                             public content::WebContentsDelegate,
                             public content::WebContentsObserver
#ifdef OHOS_EX_GET_ZOOM_LEVEL
                             , public zoom::ZoomObserver
#endif
 {
 public:
  // Used for handling the response to command messages.
  class CommandResponseHandler : public virtual CefBaseRefCounted {
   public:
    virtual void OnResponse(const std::string& response) = 0;
  };

  ~AlloyBrowserHostImpl() override;

  // Create a new AlloyBrowserHostImpl instance with owned WebContents.
  static CefRefPtr<AlloyBrowserHostImpl> Create(
      CefBrowserCreateParams& create_params);

  // Returns the browser associated with the specified RenderViewHost.
  static CefRefPtr<AlloyBrowserHostImpl> GetBrowserForHost(
      const content::RenderViewHost* host);
  // Returns the browser associated with the specified RenderFrameHost.
  static CefRefPtr<AlloyBrowserHostImpl> GetBrowserForHost(
      const content::RenderFrameHost* host);
  // Returns the browser associated with the specified WebContents.
  static CefRefPtr<AlloyBrowserHostImpl> GetBrowserForContents(
      const content::WebContents* contents);
  // Returns the browser associated with the specified global ID.
  static CefRefPtr<AlloyBrowserHostImpl> GetBrowserForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);

  // CefBrowserHost methods.
  void CloseBrowser(bool force_close) override;
  bool TryCloseBrowser() override;
  CefWindowHandle GetWindowHandle() override;
  CefWindowHandle GetOpenerWindowHandle() override;
  double GetZoomLevel() override;
  void SetZoomLevel(double zoomLevel) override;
  void SetBrowserZoomLevel(double zoom_factor) override;
  void Find(const CefString& searchText,
            bool forward,
            bool matchCase,
            bool findNext
#if BUILDFLAG(IS_OHOS)
            ,
            bool newSession
#endif
            ) override;
  void StopFinding(bool clearSelection) override;
  void ShowDevTools(const CefWindowInfo& windowInfo,
                    CefRefPtr<CefClient> client,
                    const CefBrowserSettings& settings,
                    const CefPoint& inspect_element_at) override;
#ifdef OHOS_DEVTOOLS
  void ShowDevToolsWith(
      CefRefPtr<CefBrowserHost> frontend_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
      const CefPoint& inspect_element_at) override;
#endif // OHOS_DEVTOOLS
  void CloseDevTools() override;
  bool HasDevTools() override;
  bool IsWindowRenderingDisabled() override;
  void WasResized() override;
  void WasHidden(bool hidden) override;
#if defined(OHOS_INPUT_EVENTS)
  void ScrollFocusedEditableNodeIntoView() override;
#endif
#if BUILDFLAG(IS_OHOS)
  void WasOccluded(bool occluded) override;
  void OnWindowShow() override;
  void OnWindowHide() override;
  void OnOnlineRenderToForeground() override;
  void SetEnableLowerFrameRate(bool enabled) override;
  void SetWindowId(int window_id, int nweb_id) override;
  void RenderViewReady() override;
  void SendTouchEventList(const std::vector<CefTouchEvent>& event_list) override;
  void SetWakeLockHandler(int32_t windowId, CefRefPtr<CefSetLockCallback> callback) override;
  void NotifyForNextTouchEvent() override;
#endif
  void NotifyScreenInfoChanged() override;
  void Invalidate(PaintElementType type) override;
  void SendExternalBeginFrame() override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  void SendCaptureLostEvent() override;
  int GetWindowlessFrameRate() override;
  void SetWindowlessFrameRate(int frame_rate) override;
  void ImeSetComposition(const CefString& text,
                         const std::vector<CefCompositionUnderline>& underlines,
                         const CefRange& replacement_range,
                         const CefRange& selection_range) override;
  void ImeCommitText(const CefString& text,
                     const CefRange& replacement_range,
                     int relative_cursor_pos) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void ImeCancelComposition() override;
  void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                           const CefMouseEvent& event,
                           DragOperationsMask allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          DragOperationsMask allowed_ops) override;
  void DragTargetDragLeave() override;
  void DragTargetDrop(const CefMouseEvent& event) override;
  void DragSourceSystemDragEnded() override;
  void DragSourceEndedAt(int x, int y, DragOperationsMask op) override;
  void SetAudioMuted(bool mute) override;
  bool IsAudioMuted() override;
#if defined(OHOS_MEDIA_POLICY)
  void SetAudioResumeInterval(int resumeInterval) override;
  void SetAudioExclusive(bool audioExclusive) override;
#endif // defined(OHOS_MEDIA_POLICY)
  void SetAccessibilityState(cef_state_t accessibility_state) override;
#if BUILDFLAG(IS_OHOS)
  void GetRootBrowserAccessibilityManager(void** manager) override;
#endif
  void SetAutoResizeEnabled(bool enabled,
                            const CefSize& min_size,
                            const CefSize& max_size) override;
  CefRefPtr<CefExtension> GetExtension() override;
  bool IsBackgroundHost() override;

  // Returns true if windowless rendering is enabled.
  bool IsWindowless() const override;

  bool IsVisible() const override;

  // Returns true if this browser supports picture-in-picture.
  bool IsPictureInPictureSupported() const;

  // Called when the OS window hosting the browser is destroyed.
  void WindowDestroyed() override;

  bool WillBeDestroyed() const override;

  // Destroy the browser members. This method should only be called after the
  // native browser window is not longer processing messages.
  void DestroyBrowser() override;

  // Cancel display of the context menu, if any.
  void CancelContextMenu();

  bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                            bool is_guest_view,
                            const content::OpenURLParams& params) override;

  // Convert from view DIP coordinates to screen coordinates. If
  // |want_dip_coords| is true return DIP instead of device (pixel) coordinates
  // on Windows/Linux.
  gfx::Point GetScreenPoint(const gfx::Point& view, bool want_dip_coords) const;

#ifdef OHOS_DRAG_DROP
  gfx::Rect GetVisibleRectToWeb();
#endif

  void StartDragging(const content::DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh);
  void UpdateDragCursor(ui::mojom::DragOperation operation);

  // Accessors that must be called on the UI thread.
  extensions::ExtensionHost* GetExtensionHost() const;

  void OnSetFocus(cef_focus_source_t source) override;

  bool ShowContextMenu(const content::ContextMenuParams& params);

  bool Discard() override;

  bool Restore() override;

#if defined(OHOS_COMPOSITE_RENDER)
  void WasKeyboardResized() override;
#endif  // defined(OHOS_COMPOSITE_RENDER)

#if defined(OHOS_PRINT)
  void SetToken(void* token) override;
  void CreateWebPrintDocumentAdapter(const CefString& jobName,
                                     void** webPrintDocumentAdapter) override;
  void SetPrintBackground(bool enable) override;
  bool GetPrintBackground() override;
#endif // defined(OHOS_PRINT)

#if defined(OHOS_INPUT_EVENTS)
  void ContentsZoomChange(bool zoom_in) override;
#endif

#if defined(OHOS_INPUT_EVENTS)
  void SetVirtualKeyBoardArg(int32_t width, int32_t height, double keyboard) override;
  bool ShouldVirtualKeyboardOverlay() override;
  void AdvanceFocusForIME(int focusType) override;
#endif

#ifdef OHOS_RENDER_PROCESS_MODE
  void SetNeedsReload(bool needs_reload) override;
  bool NeedsReload() override;
#endif

#if defined(OHOS_RENDERER_ANR_DUMP)
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter,
      content::RenderProcessNotRespondingReason reason) override;

  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
#endif

#if defined(OHOS_EX_REFRESH_IFRAME)
  bool IsIframe() override;
  void ReloadFocusedFrame() override;
#endif

  enum DestructionState {
    DESTRUCTION_STATE_NONE = 0,
    DESTRUCTION_STATE_PENDING,
    DESTRUCTION_STATE_ACCEPTED,
    DESTRUCTION_STATE_COMPLETED
  };
  DestructionState destruction_state() const { return destruction_state_; }

  // content::WebContentsDelegate methods.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_main_frame_navigation) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void CloseContents(content::WebContents* source) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
#ifdef OHOS_CLIPBOARD
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
#endif
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void GetCustomWebContentsView(
      content::WebContents* web_contents,
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_frame_id,
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void UpdatePreferredSize(content::WebContents* source,
                           const gfx::Size& pref_size) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool IsNeverComposited(content::WebContents* web_contents) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  bool IsBackForwardCacheSupported() override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;

#ifdef OHOS_FOCUS
  void ActivateContents(content::WebContents* contents) override;
#endif

#if BUILDFLAG(IS_OHOS)
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostMouseLock() override;

  // Shows the repost form confirmation dialog box.
  void ShowRepostFormWarningDialog(content::WebContents* source) override;

  void OnNativeEmbedStatusUpdate(
      const content::NativeEmbedInfo& native_embed_info,
      content::NativeEmbedInfo::TagState state) override;
  
  void OnLayerRectVisibilityChange(const std::string& embed_id, bool visibility) override;
#endif

  // content::WebContentsObserver methods.
  using content::WebContentsObserver::BeforeUnloadFired;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnAudioStateChanged(bool audible) override;
  void OnFormEditingStateChanged(bool state, uint64_t form_id) override;
  void MediaStartedPlaying(const content::WebContentsObserver::MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) override;
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& content_event_bundle) override;
  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& locData)
      override;
  void WebContentsDestroyed() override;

#if BUILDFLAG(IS_OHOS)
  /* ohos webview begin */
  void AddVisitedLinks(const std::vector<CefString>& urls) override;
  void SetBackgroundColor(int color) override;
  SkColor GetBackgroundColor() const;
  void GetZoomLevelCallback();

  void SetTouchInsertHandleMenuShow(bool show) {
    web_contents()->SetTouchInsertHandleMenuShow(show);
  }
  bool GetTouchInsertHandleMenuShow() {
    return web_contents()->GetTouchInsertHandleMenuShow();
  }

#ifdef OHOS_HTML_SELECT
  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection);
#endif  // #ifdef OHOS_HTML_SELECT

#ifdef OHOS_CSS_INPUT_TIME
  void OpenDateTimeChooser() override;
  void CloseDateTimeChooser() override;
#endif  // #ifdef OHOS_CSS_INPUT_TIME
  /* ohos webview end */
#endif

#ifdef OHOS_ARKWEB_ADBLOCK
  void OnAdsBlocked(const std::string& main_frame_url,
                    const std::map<std::string, int32_t>& subresource_blocked,
                    bool is_site_first_report) override;
#endif

#if defined(OHOS_ARKWEB_EXTENSIONS)
  void WebExtensionUpdateTabUrl(int32_t tab_id, const GURL& url) override;
  void SetTabId(int32_t tab_id) override;
  int32_t GetTabId() override;
#endif

#if defined(OHOS_EX_PASSWORD)
  void ShowPasswordDialog(bool is_update, const std::string& url) override;
#endif

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
  void OnShowAutofillPopup(const gfx::RectF& element_bounds,
                           bool is_rtl,
                           const std::vector<autofill::Suggestion>& suggestions,
                           bool is_password_popup_type) override;
  void OnHideAutofillPopup() override;
#endif

#ifdef OHOS_EX_GET_ZOOM_LEVEL
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override {}
#endif

#if defined(OHOS_SECURE_JAVASCRIPT_PROXY)
  CefString GetLastJavascriptProxyCallingFrameUrl() override;
#endif

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
  std::unique_ptr<content::CustomMediaPlayer> CreateCustomMediaPlayer(
      std::unique_ptr<content::CustomMediaPlayerListener> listener,
      const content::MediaInfo& media_info) override;
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#if defined(OHOS_VIDEO_ASSISTANT)
  void OnShowToast(double duration, const std::string& toast) override;
  void OnShowVideoAssistant(const std::string& videoAssistantItems) override;
  void OnReportStatisticLog(const std::string& content) override;
  std::unique_ptr<content::MediaPlayerListener> OnFullScreenOverlayEnter(
      media::mojom::MediaInfoForVASTPtr media_info_ptr,
      const content::MediaPlayerId& media_player_id) override;
#endif  // defined(OHOS_VIDEO_ASSISTANT)

#ifdef OHOS_AI
  void CreateOverlay(const gfx::ImageSkia& image,
                     const gfx::Rect& image_rect,
                     const gfx::Point& touch_point);
  void OnTextSelected(bool flag) override;
  void OnDestroyImageAnalyzerOverlay() override;
  float GetPageScaleFactor() override;
#endif

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
  void OnBeforeUnloadFired(bool proceed) override;
#endif // OHOS_DISPATCH_BEFORE_UNLOAD

 private:
  friend class CefBrowserPlatformDelegateAlloy;

  static CefRefPtr<AlloyBrowserHostImpl> CreateInternal(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      bool own_web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<AlloyBrowserHostImpl> opener,
      bool is_devtools_popup,
      CefRefPtr<CefRequestContextImpl> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      CefRefPtr<CefExtension> extension);

  AlloyBrowserHostImpl(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<AlloyBrowserHostImpl> opener,
      CefRefPtr<CefRequestContextImpl> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      CefRefPtr<CefExtension> extension);

  // Give the platform delegate an opportunity to create the host window.
  bool CreateHostWindow();

  void StartAudioCapturer();
  void OnRecentlyAudibleTimerFired();

#if BUILDFLAG(IS_OHOS)
  void SetFocusInternal(bool focus);

  void UpdateBackgroundColor(int color);
  void UpdateZoomSupportEnabled();
  void ReportWindowStatus(bool first_view_ready);
  void InactiveUnloadOldProcess(base::ProcessId pid);
  base::ProcessId GetRenderProcessId();
  void UpdateVSyncFrequency();
  void ResetVSyncFrequency();
  void SetVisible(int32_t nweb_id, bool visible);
  void SetNativeEmbedMode(bool flag) override;
  void MaximizeResize() override;
#endif

#if defined(OHOS_INPUT_EVENTS)
  bool IsNeedZoomChange(const content::NativeWebKeyboardEvent& event,
    bool& zoom_in);
  bool WebHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event);
#endif

#if defined(OHOS_COMPOSITE_RENDER)
  void SetShouldFrameSubmissionBeforeDraw(bool should) override;
  void SetDrawRect(int x, int y, int width, int height) override;
  void SetDrawMode(int mode) override;
  bool GetPendingSizeStatus() override;
  void SetFitContentMode(int mode) override;
#endif  // defined(OHOS_COMPOSITE_RENDER)
#if defined(OHOS_RENDERER_ANR_DUMP)
  void OnDumpJavaScriptStackCallback(
      int pid,
      content::RenderProcessNotRespondingReason reason,
      const std::string& stack);
#endif
#if defined(OHOS_VIDEO_ASSISTANT)
  std::unique_ptr<content::VideoAssistant> CreateVideoAssistant() override;
  void PopluateVideoAssistantConfig(
      const std::string& url,
      media::mojom::VideoAssistantConfigPtr& config) override;
#endif // OHOS_VIDEO_ASSISTANT

  CefWindowHandle opener_;
  const bool is_windowless_;
  CefWindowHandle host_window_handle_ = kNullWindowHandle;
  CefRefPtr<CefExtension> extension_;
  bool is_background_host_ = false;

  // Represents the current browser destruction state. Only accessed on the UI
  // thread.
  DestructionState destruction_state_ = DESTRUCTION_STATE_NONE;

  // True if the OS window hosting the browser has been destroyed. Only accessed
  // on the UI thread.
  bool window_destroyed_ = false;

  // Used for creating and managing JavaScript dialogs.
  std::unique_ptr<CefJavaScriptDialogManager> javascript_dialog_manager_;

  // Used for creating and managing context menus.
  std::unique_ptr<CefMenuManager> menu_manager_;

  // Used for capturing audio for CefAudioHandler.
  std::unique_ptr<CefAudioCapturer> audio_capturer_;

  // Timer for determining when "recently audible" transitions to false. This
  // starts running when a tab stops being audible, and is canceled if it starts
  // being audible again before it fires.
  std::unique_ptr<base::OneShotTimer> recently_audible_timer_;

#if BUILDFLAG(IS_OHOS)
  int base_background_color_ = 0xffffffff;

  double curFactor_ = 0.0;

  std::shared_ptr<base::WaitableEvent> event_ = nullptr;
  bool is_hidden_ = false;
  int window_id_ = -1;
  int nweb_id_ = -1;
  base::ProcessId last_pid_ = -1;

  int video_stream_cnt_ = 0;
  bool has_video_playing_ = false;
  bool has_touch_event_ = false;
  bool set_lower_frame_rate_ = false;
  static constexpr int WAIT_TOUCH_EVENT_DELAY_TIME = 3000/*ms*/;
#endif

#if defined(OHOS_ARKWEB_EXTENSIONS)
  int tab_id_ = -1;
#endif
  bool start_play_ = false;

#ifdef OHOS_RENDER_PROCESS_MODE
  bool needs_reload_ = false;
#endif

};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_BROWSER_HOST_IMPL_H_
