// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"

#ifdef OHOS_EX_PASSWORD
#include "components/autofill/core/browser/ui/suggestion.h"
#endif

#ifdef OHOS_DRAG_DROP
#include "content/browser/web_contents/web_contents_impl.h"
#endif

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
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendTouchpadFlingEvent(const CefMouseEvent& event,
                              double vx,
                              double vy) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;

  void SetFocus(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner()
      override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  void WasHidden(bool hidden) override;
  bool IsHidden() const override;
#if BUILDFLAG(IS_OHOS)
  void WasOccluded(bool occluded) override;
  void SendTouchEventList(const std::vector<CefTouchEvent>& event_list) override;
  void NotifyForNextTouchEvent() override;
  void SetNativeEmbedMode(bool flag) override;
  void MaximizeResize() override;
#endif
#if defined(OHOS_INPUT_EVENTS)
  void ScrollFocusedEditableNodeIntoView() override;
#endif
  void NotifyScreenInfoChanged() override;
#if BUILDFLAG(IS_OHOS)
  void NotifyScreenInfoChangedV2() override;
#endif
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
#ifdef OHOS_DRAG_DROP
  bool GetCurRWH(content::WebContentsImpl* web_contents,
    const gfx::PointF& client_pt, gfx::PointF* transformed_pt);
#endif
  void DragTargetDrop(const CefMouseEvent& event) override;
  void StartDragging(const content::DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(ui::mojom::DragOperation operation) override;
  void DragSourceEndedAt(int x, int y, cef_drag_operations_mask_t op) override;
  void DragSourceSystemDragEnded() override;
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& eventData) override;
  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& locData)
      override;

#if defined(OHOS_COMPOSITE_RENDER)
  void SetShouldFrameSubmissionBeforeDraw(bool should) override;
  void WasKeyboardResized() override;
  void SetDrawRect(int x, int y, int width, int height) override;
  void SetDrawMode(int mode) override;
  bool GetPendingSizeStatus() override;
  void SetFitContentMode(int mode) override;
  int drawMode_ = 0;
  gfx::Rect drawRect_{0, 0, 0, 0};
  int fit_content_mode_ = 0;
#endif  // defined(OHOS_COMPOSITE_RENDER)

#ifdef OHOS_HTML_SELECT
  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
#endif  // #ifdef OHOS_HTML_SELECT

#if defined(OHOS_INPUT_EVENTS)
  void SetVirtualKeyBoardArg(int32_t width, int32_t height, double keyboard) override;
  bool ShouldVirtualKeyboardOverlay() override;
  void OnNativeEmbedLifecycleChange(const CefRenderHandler::CefNativeEmbedData& info) override;
  void OnNativeEmbedVisibilityChange(const std::string& embed_id, bool visibility) override;
  void SetScrollable(bool enable) override;
  void AdvanceFocusForIME(int focusType) override;
  void ScrollBy(float delta_x, float delta_y) override;
#endif

#ifdef OHOS_ARKWEB_ADBLOCK
  void OnAdsBlocked(const std::string& main_frame_url,
                    const std::map<std::string, int32_t>& subresource_blocked,
                    bool is_site_first_report) override;

  bool TrigAdBlockEnabledForSiteFromUi(const std::string& main_frame_url,
                                       int main_frame_tree_node_id) override;
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

#ifdef OHOS_EX_TOPCONTROLS
  int GetTopControlsOffset() override;
  int GetShrinkViewportHeight() override;
#endif

#ifdef OHOS_DISPLAY_CUTOUT
  void OnSafeInsetsChange(int left, int top, int right, int bottom) override;
#endif

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

#ifdef OHOS_AI
  void CreateOverlay(const gfx::ImageSkia& image,
                     const gfx::Rect& image_rect,
                     const gfx::Point& touch_point) override;
  void OnTextSelected(bool flag) override;
  void OnDestroyImageAnalyzerOverlay() override;
  float GetPageScaleFactor() override;
  void OnFoldStatusChanged(uint32_t foldstatus) override;
#endif

#if defined(OHOS_SOFTWARE_COMPOSITOR)
bool WebPageSnapshot(const char* id,
                     int width,
                     int height,
                     cef_web_snapshot_callback_t callback) override;
#endif

  void ScaleGestureChangeV2(int type, float scale, float originScale, float centerX, float centerY) override;

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
  void OnBeforeUnloadFired(bool proceed) override;
#endif // OHOS_DISPATCH_BEFORE_UNLOAD

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

  CefWebContentsViewOSR* view_osr_ = nullptr;  // Not owned by this class.

  // Pending drag/drop data.
  CefRefPtr<CefDragData> drag_data_;
  cef_drag_operations_mask_t drag_allowed_ops_;

  // We keep track of the RenderWidgetHost we're dragging over. If it changes
  // during a drag, we need to re-send the DragEnter message.
  base::WeakPtr<content::RenderWidgetHostImpl> current_rwh_for_drag_;

  // We also keep track of the RenderViewHost we're dragging over to avoid
  // sending the drag exited message after leaving the current
  // view. |current_rvh_for_drag_| should not be dereferenced.
  void* current_rvh_for_drag_;

#ifdef OHOS_EX_TOPCONTROLS
  int shrink_viewport_height_ = 0;
#endif

#ifdef OHOS_FOCUS
  // We keep track of the view's focus-capturing logic, and if the view hasn't been created yet,
  // we temporarily store the focus-capturing event until RenderViewCreated is created and then re-focus-capturing.
  bool is_view_focus_failed_ = false;
  bool focus_status_ = false;
#endif

#ifdef OHOS_DISPLAY_CUTOUT
  gfx::Insets safe_insets_;
#endif

#ifdef OHOS_AI
  uint32_t fold_status_ = 0;
#endif
  bool native_embed_mode_ = false;
  // We keep track of the RenderWidgetHost from which the current drag started,
  // in order to properly route the drag end message to it.
  base::WeakPtr<content::RenderWidgetHostImpl> drag_start_rwh_;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
