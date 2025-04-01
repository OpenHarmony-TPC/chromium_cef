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

#include "arkweb/build/features/features.h"
#include "base/synchronization/lock.h"
#include "cef/include/cef_browser.h"
#include "cef/include/cef_client.h"
#include "cef/include/cef_frame.h"
#if BUILDFLAG(ARKWEB_ACTIVITY_STATE)
#include "cef/include/internal/cef_types.h"
#endif
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_info.h"
#include "cef/libcef/browser/frame_host_impl.h"
#include "cef/libcef/browser/menu_manager.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ohos_cef_ext/libcef/browser/arkweb_browser_host_ext.h"

#if BUILDFLAG(ARKWEB_HTML_SELECT)
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#endif

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
#include "include/cef_custom_media_player_delegate.h"
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_DATALIST)
#include "components/autofill/core/browser/ui/suggestion.h"
#endif

class CefAudioCapturer;
class CefBrowserInfo;

#if !BUILDFLAG(ARKWEB_ZOOM)
class SiteInstance;
#endif

// CefBrowser implementation for Alloy style. Method calls are delegated to the
// CefPlatformDelegate or the WebContents as appropriate. All methods are
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
#if BUILDFLAG(ARKWEB_DEVTOOLS)
class AlloyBrowserHostImpl : public ArkWebBrowserHostExtImpl,
                             public content::WebContentsDelegate,
                             public content::WebContentsObserver
#else
class AlloyBrowserHostImpl : virtual public ArkWebBrowserHostExtImpl,
                             public content::WebContentsDelegate,
                             public content::WebContentsObserver
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
{
 public:
  // Used for handling the response to command messages.
  class CommandResponseHandler : public virtual CefBaseRefCounted {
   public:
    virtual void OnResponse(const std::string& response) = 0;
  };

  ~AlloyBrowserHostImpl() override;

  CefRefPtr<AlloyBrowserHostImpl> AsAlloyBrowserHostImpl() override {
    return this;
  }

  // Create a new AlloyBrowserHostImpl instance with owned WebContents.
  static CefRefPtr<AlloyBrowserHostImpl> Create(
      CefBrowserCreateParams& create_params);

  // Safe (checked) conversion from CefBrowserHostBase to AlloyBrowserHostImpl.
  // Use this method instead of static_cast.
  static CefRefPtr<AlloyBrowserHostImpl> FromBaseChecked(
      CefRefPtr<CefBrowserHostBase> host_base);

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
  void Find(const CefString& searchText,
            bool forward,
            bool matchCase,
            bool findNext) override;
  void StopFinding(bool clearSelection) override;
#if BUILDFLAG(ARKWEB_DEVTOOLS)
  void ShowDevToolsWith(
      CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
      const CefPoint& inspect_element_at) override;
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
  bool IsWindowRenderingDisabled() override;
  void WasResized() override;
  void WasHidden(bool hidden) override;

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
  void WasOccluded(bool occluded) override;
  void SetEnableLowerFrameRate(bool enabled) override;
#endif
#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  void UpdateVSyncFrequency();
  void ResetVSyncFrequency();
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
#if BUILDFLAG(IS_ARKWEB)
  void GetRootBrowserAccessibilityManager(void** manager) override;
#endif

  void SetAutoResizeEnabled(bool enabled,
                            const CefSize& min_size,
                            const CefSize& max_size) override;
  bool CanExecuteChromeCommand(int command_id) override;
  void ExecuteChromeCommand(int command_id,
                            cef_window_open_disposition_t disposition) override;

  // CefBrowserHostBase methods:
  bool IsWindowless() const override;
  bool IsAlloyStyle() const override { return true; }
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
                            const content::OpenURLParams& params) override;

  // Convert from view DIP coordinates to screen coordinates. If
  // |want_dip_coords| is true return DIP instead of device (pixel) coordinates
  // on Windows/Linux.
  gfx::Point GetScreenPoint(const gfx::Point& view, bool want_dip_coords) const;

  void StartDragging(const content::DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh);
  void UpdateDragOperation(ui::mojom::DragOperation operation,
                           bool document_is_handling_drag);

  void OnSetFocus(cef_focus_source_t source) override;

  bool ShowContextMenu(const content::ContextMenuParams& params);

#if BUILDFLAG(ARKWEB_EX_REFRESH_IFRAME)
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
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
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
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  bool WebHandleKeyboardEvent(content::WebContents* source,
                              const input::NativeWebKeyboardEvent& event);
#endif
  void ContentsZoomChange(bool zoom_in) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void GetCustomWebContentsView(
      content::WebContents* web_contents,
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_frame_id,
      raw_ptr<content::WebContentsView>* view,
      raw_ptr<content::RenderViewHostDelegateView>* delegate_view) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void RendererUnresponsive(content::WebContents* source,
                            content::RenderWidgetHost* render_widget_host,
                            base::RepeatingClosure hang_monitor_restarter
#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
                            ,
                            content::RendererIsUnresponsiveReason reason
#endif
                            ) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
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
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  void OnNativeEmbedStatusUpdate(
      const content::NativeEmbedInfo& native_embed_info,
      content::NativeEmbedInfo::TagState state) override;
  void OnNativeEmbedFirstFramePaint(
      int32_t native_embed_id,
      const std::string& embed_id_attribute) override;
  void OnLayerRectVisibilityChange(const std::string& embed_id,
                                   bool visibility) override;
#endif

  // content::WebContentsObserver methods.
  using content::WebContentsObserver::BeforeUnloadFired;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnAudioStateChanged(bool audible) override;
  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) override;
  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) override;
  void WebContentsDestroyed() override;

#if BUILDFLAG(ARKWEB_FOCUS)
  void ActivateContents(content::WebContents* contents) override;
#endif  // BUILDFLAG(ARKWEB_FOCUS)
#if BUILDFLAG(ARKWEB_SAME_LAYER)
  void SetNativeEmbedMode(bool flag) override;
#endif
#if BUILDFLAG(ARKWEB_ACTIVITY_STATE)
  void OnFormEditingStateChanged(bool state, uint64_t form_id) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override;
  void MediaPlayerGone(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id) override;
#endif
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  void SetTouchInsertHandleMenuShow(bool show) {
    web_contents()->SetTouchInsertHandleMenuShow(show);
  }

  bool GetTouchInsertHandleMenuShow() {
    return web_contents()->GetTouchInsertHandleMenuShow();
  }

  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  void UpdateZoomSupportEnabled();
#endif

#if BUILDFLAG(ARKWEB_PRINT)
  void SetToken(void* token) override;
  void CreateWebPrintDocumentAdapter(const CefString& jobName,
                                     void** webPrintDocumentAdapter) override;
  void SetPrintBackground(bool enable) override;
  bool GetPrintBackground() override;
#endif

#if BUILDFLAG(ARKWEB_DISCARD)
  bool Discard() override;
  bool Restore() override;
#endif

#if BUILDFLAG(ARKWEB_HTML_SELECT)
  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection);
#endif  // ARKWEB_HTML_SELECT

#if BUILDFLAG(ARKWEB_CSS_INPUT_TIME)
  void OpenDateTimeChooser() override;
  void CloseDateTimeChooser() override;
#endif  // ARKWEB_CSS_INPUT_TIME

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
  std::unique_ptr<content::CustomMediaPlayer> CreateCustomMediaPlayer(
      std::unique_ptr<content::CustomMediaPlayerListener> listener,
      const content::MediaInfo& media_info) override;
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  void OnShowToast(double duration, const std::string& toast) override;
  void OnShowVideoAssistant(const std::string& videoAssistantItems) override;
  void OnReportStatisticLog(const std::string& content) override;
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  // Shows the repost form confirmation dialog box.
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
#endif  // ARKWEB_NETWORK_BASE

#if BUILDFLAG(ARKWEB_ADBLOCK)
  void OnAdsBlocked(const std::string& main_frame_url,
                    const std::map<std::string, int32_t>& subresource_blocked,
                    bool is_site_first_report) override;

  bool TrigAdBlockEnabledForSiteFromUi(const std::string& main_frame_url,
                                       int main_frame_tree_node_id) override;
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
  void WebExtensionTabUpdated(
      int tab_id,
      const std::vector<CefString>& changed_property_names,
      const CefString& url) override;
  void WebExtensionTabUpdated(
      int tab_id,
      const std::vector<CefString>& changed_property_names,
      std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) override;

  void WebExtensionUpdateTabUrl(int32_t tab_id, const GURL& url) override;
  void WebExtensionUpdateTab(
      int32_t tab_id,
      const NWebExtensionTabUpdateProperties* update_properties) override;
  void ExtensionSetTabId(int tab_id) override;
  int ExtensionGetTabId() const override;
  void WebExtensionTabActivated(int tab_id, int window_id) override;
 
  void WebExtensionActionClicked(
      std::string extensionId,
      const NWebExtensionTab* tab) override;
#endif

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  void OnShareFile(const std::string& filePath,
                   const std::string& utdTypeId) override;
#endif

#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
  void OnBeforeUnloadFired(bool proceed) override;
#endif  // ARKWEB_DISATCH_BEFORE_UNLOAD

#if BUILDFLAG(ARKWEB_DATALIST)
  void OnShowAutofillPopup(const gfx::RectF& element_bounds,
                           bool is_rtl,
                           const std::vector<autofill::Suggestion>& suggestions,
                           bool is_password_popup_type) override;
  void OnHideAutofillPopup() override;
#endif

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
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate);

  AlloyBrowserHostImpl(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<AlloyBrowserHostImpl> opener,
      CefRefPtr<CefRequestContextImpl> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate);

  // Give the platform delegate an opportunity to create the host window.
  bool CreateHostWindow();

  void StartAudioCapturer();
  void OnRecentlyAudibleTimerFired();

#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
  void OnDumpJavaScriptStackCallback(
      int pid,
      content::RendererIsUnresponsiveReason reason,
      const std::string& stack);
#endif

  CefWindowHandle opener_window_handle_ = kNullWindowHandle;
  const bool is_windowless_;
  CefWindowHandle host_window_handle_ = kNullWindowHandle;

  // Represents the current browser destruction state. Only accessed on the UI
  // thread.
  DestructionState destruction_state_ = DESTRUCTION_STATE_NONE;

  // True if the OS window hosting the browser has been destroyed. Only accessed
  // on the UI thread.
  bool window_destroyed_ = false;

  // Used for creating and managing context menus.
  std::unique_ptr<CefMenuManager> menu_manager_;

  // Used for capturing audio for CefAudioHandler.
  std::unique_ptr<CefAudioCapturer> audio_capturer_;

  // Timer for determining when "recently audible" transitions to false. This
  // starts running when a tab stops being audible, and is canceled if it starts
  // being audible again before it fires.
  std::unique_ptr<base::OneShotTimer> recently_audible_timer_;

#if BUILDFLAG(IS_OHOS)
  void OnWindowShow() override;
  void OnWindowHide() override;
  void OnOnlineRenderToForeground() override;
  void SetWindowId(int window_id, int nweb_id) override;
  void RenderViewReady() override;
  void InactiveUnloadOldProcess(base::ProcessId pid);
  int nweb_id_ = -1;
  int window_id_ = -1;
  base::ProcessId last_pid_ = -1;
  void SetVisible(int32_t nweb_id, bool visible);
  static int32_t ltpo_strategy_;
#endif

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
  void ReportWindowStatus(bool first_view_ready);
  bool is_hidden_ = false;
#endif

#if BUILDFLAG(ARKWEB_REPORT_RENDER_STATE)
  base::ProcessId GetRenderProcessId();
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  void WasKeyboardResized() override;
  void SetDrawRect(int x, int y, int width, int height) override;
  void SetDrawMode(int mode) override;
  bool GetPendingSizeStatus() override;
  void SetFitContentMode(int mode) override;
  void SetShouldFrameSubmissionBeforeDraw(bool should) override;
  std::string GetCurrentLanguage() override;
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  std::unique_ptr<content::VideoAssistant> CreateVideoAssistant() override;
  void PopluateVideoAssistantConfig(
      const std::string& url,
      media::mojom::VideoAssistantConfigPtr& config) override;
#endif  // ARKWEB_VIDEO_ASSISTANT

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
  bool has_video_playing_ = false;
  bool has_touch_event_ = false;
  bool set_lower_frame_rate_ = false;
  int video_stream_cnt_ = 0;
  static constexpr int WAIT_TOUCH_EVENT_DELAY_TIME = 3000 /*ms*/;
#endif
#if BUILDFLAG(ARKWEB_ACTIVITY_STATE)
  bool start_play_ = false;
#endif
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
  int tab_id_ = -1;
#endif
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_BROWSER_HOST_IMPL_H_
