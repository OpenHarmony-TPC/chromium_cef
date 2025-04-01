// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_

#include "arkweb/build/features/features.h"
#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "cef/libcef/browser/native/browser_platform_delegate_native.h"

#if BUILDFLAG(ARKWEB_DRAG_DROP)
#include "content/browser/web_contents/web_contents_impl.h"
#endif  // BUILDFLAG(ARKWEB_DRAG_DROP)

class CefRenderWidgetHostViewOSR;
class CefWebContentsViewOSR;

namespace content {
class RenderWidgetHostImpl;
}  // namespace content

// Base implementation of windowless browser functionality.
class CefBrowserPlatformDelegateOsr
    : public CefBrowserPlatformDelegateAlloy,
      public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // CefBrowserPlatformDelegate methods:
  void CreateViewForWebContents(
      raw_ptr<content::WebContentsView>* view,
      raw_ptr<content::RenderViewHostDelegateView>* delegate_view) override;
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
#if BUILDFLAG(ARKWEB_TOUCHPAD_FLING)
  void SendTouchpadFlingEvent(const CefMouseEvent& event,
                              double vx,
                              double vy) override;
#endif
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;

  void SetFocus(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const input::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner()
      override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  void WasHidden(bool hidden) override;
  bool IsHidden() const override;
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  void AdvanceFocusForIME(int focusType) override;
  void SendTouchEventList(
      const std::vector<CefTouchEvent>& event_list) override;
  void SetScrollable(bool enable) override;
  void ScrollFocusedEditableNodeIntoView() override;
  void ScaleGestureChangeV2(int type,
                            float scale,
                            float originScale,
                            float centerX,
                            float centerY) override;
  void ScrollBy(float delta_x, float delta_y) override;
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
  void WasOccluded(bool occluded) override;
#endif

  void NotifyScreenInfoChanged() override;
  void Invalidate(cef_paint_element_type_t type) override;
  void SendExternalBeginFrame() override;
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
                           cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragLeave() override;
#if BUILDFLAG(ARKWEB_DRAG_DROP)
  bool GetCurRWH(content::WebContentsImpl* web_contents,
                 const gfx::PointF& client_pt,
                 gfx::PointF* transformed_pt);
#endif  // BUILDFLAG(ARKWEB_DRAG_DROP)
  void DragTargetDrop(const CefMouseEvent& event) override;
  void StartDragging(const content::DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragOperation(ui::mojom::DragOperation operation,
                           bool document_is_handling_drag) override;
  void DragSourceEndedAt(int x, int y, cef_drag_operations_mask_t op) override;
  void DragSourceSystemDragEnded() override;
  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) override;
  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) override;

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  void WasKeyboardResized() override;
  void SetDrawMode(int mode) override;
  void SetFitContentMode(int mode) override;
  bool GetPendingSizeStatus() override;
  void SetDrawRect(int x, int y, int width, int height) override;
  void SetShouldFrameSubmissionBeforeDraw(bool should) override;
  std::string GetCurrentLanguage() override;
  int drawMode_ = 0;
  gfx::Rect drawRect_{0, 0, 0, 0};
  int fit_content_mode_ = 0;
#endif

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  void OnNativeEmbedLifecycleChange(
      const ArkWebRenderHandlerExt::CefNativeEmbedData& info) override;
  void OnNativeEmbedFirstFramePaint(
      const content::NativeEmbedFirstPaintEvent& event) override;
  void SetNativeEmbedMode(bool flag) override;
  void OnNativeEmbedVisibilityChange(const std::string& embed_id,
                                     bool visibility) override;
#endif

#if BUILDFLAG(ARKWEB_AI)
  void OnTextSelected(bool flag) override;
  void OnDestroyImageAnalyzerOverlay() override;
  void OnFoldStatusChanged(uint32_t foldStatus) override;
  float GetPageScaleFactor() override;
#endif

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
  void OnSafeInsetsChange(int left, int top, int right, int bottom) override;
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  int GetTopControlsOffset() override;
  int GetShrinkViewportHeight() override;
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
      bool allow_multiple_selection) override;
#endif  // #if BUILDFLAG(ARKWEB_HTML_SELECT)

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
  bool WebPageSnapshot(const char* id,
                       int width,
                       int height,
                       cef_web_snapshot_callback_t callback) override;
#endif
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  void SetVirtualKeyBoardArg(int32_t width,
                             int32_t height,
                             double keyboard) override;
  bool ShouldVirtualKeyboardOverlay() override;
#endif

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
  void MaximizeResize() override;
#endif  // ARKWEB_MAXIMIZE_RESIZE

#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
  void OnBeforeUnloadFired(bool proceed) override;
#endif  // ARKWEB_DISATCH_BEFORE_UNLOAD

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  void OnShareFile(const std::string& file_path,
                   const std::string& utd_type_id) override;
#endif

#if BUILDFLAG(ARKWEB_ADBLOCK)
  void OnAdsBlocked(const std::string& main_frame_url,
                    const std::map<std::string, int32_t>& subresource_blocked,
                    bool is_site_first_report) override;

  bool TrigAdBlockEnabledForSiteFromUi(const std::string& main_frame_url,
                                       int main_frame_tree_node_id) override;
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

#if BUILDFLAG(ARKWEB_DATALIST)
  void OnShowAutofillPopup(const gfx::RectF& element_bounds,
                           bool is_rtl,
                           const std::vector<autofill::Suggestion>& suggestions,
                           bool is_password_popup_type) override;
  void OnHideAutofillPopup() override;
#endif

 protected:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  CefBrowserPlatformDelegateOsr(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      bool use_shared_texture,
      bool use_external_begin_frame);

  // Returns the primary OSR host view for the underlying browser. If a
  // full-screen host view currently exists then it will be returned. Otherwise,
  // the main host view will be returned.
  CefRenderWidgetHostViewOSR* GetOSRHostView() const;

  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
  const bool use_shared_texture_;
  const bool use_external_begin_frame_;

  // Not owned by this class.
  raw_ptr<CefWebContentsViewOSR> view_osr_ = nullptr;

  // Pending drag/drop data.
  CefRefPtr<CefDragData> drag_data_;
  cef_drag_operations_mask_t drag_allowed_ops_;

  // We keep track of the RenderWidgetHost we're dragging over. If it changes
  // during a drag, we need to re-send the DragEnter message.
  base::WeakPtr<content::RenderWidgetHostImpl> current_rwh_for_drag_;

  // We also keep track of the RenderViewHost we're dragging over to avoid
  // sending the drag exited message after leaving the current
  // view. |current_rvh_for_drag_| should not be dereferenced.
  raw_ptr<void> current_rvh_for_drag_ = nullptr;

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  int shrink_viewport_height_ = 0;
#endif

#if BUILDFLAG(ARKWEB_FOCUS)
  // We keep track of the view's focus-capturing logic, and if the view hasn't
  // been created yet, we temporarily store the focus-capturing event until
  // RenderViewCreated is created and then re-focus-capturing.
  bool is_view_focus_failed_ = false;
  bool focus_status_ = false;
#endif  // BUILDFLAG(ARKWEB_FOCUS)

#if BUILDFLAG(ARKWEB_AI)
  uint32_t fold_status_ = 0;
#endif

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
  gfx::Insets safe_insets_;
#endif
#if BUILDFLAG(ARKWEB_SAME_LAYER)
  bool native_embed_mode_ = false;
#endif

  // We keep track of the RenderWidgetHost from which the current drag started,
  // in order to properly route the drag end message to it.
  base::WeakPtr<content::RenderWidgetHostImpl> drag_start_rwh_;

  // Set to true when the document is handling the drag. This means that the
  // document has registered an interest in the dropped data and the renderer
  // process should pass the data to the document on drop.
  bool document_is_handling_drag_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
