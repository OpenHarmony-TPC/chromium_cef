// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_

#include "arkweb/build/features/features.h"
#include "base/memory/raw_ptr.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class WebContents;
}  // namespace content

class AlloyBrowserHostImpl;
class ArkWebRenderWidgetHostViewOSRExt;
class CefTouchSelectionControllerClientOSR;

// An implementation of WebContentsView for off-screen rendering.
class CefWebContentsViewOSR : public content::WebContentsView,
                              public content::RenderViewHostDelegateView {
 public:
  explicit CefWebContentsViewOSR(SkColor background_color,
                                 bool use_shared_texture,
                                 bool use_external_begin_frame);

  CefWebContentsViewOSR(const CefWebContentsViewOSR&) = delete;
  CefWebContentsViewOSR& operator=(const CefWebContentsViewOSR&) = delete;

  ~CefWebContentsViewOSR() override;

  void WebContentsCreated(content::WebContents* web_contents);
  content::WebContents* web_contents() const { return web_contents_; }

  void RenderViewCreated();

  // WebContentsView methods.
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  gfx::Rect GetContainerBounds() const override;
  void Focus() override {}
  void SetInitialFocus() override {}
  void StoreFocus() override {}
  void RestoreFocus() override {}
  void FocusThroughTabTraversal(bool reverse) override {}
  content::DropData* GetDropData() const override { return nullptr; }
  gfx::Rect GetViewBounds() const override;
  void CreateView(gfx::NativeView context) override {}
  content::RenderWidgetHostViewBase* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host) override;
  content::RenderWidgetHostViewBase* CreateViewForChildWidget(
      content::RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const std::u16string& title) override {}
  void RenderViewReady() override {}
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override {}
  void SetOverscrollControllerEnabled(bool enabled) override {}
  void OnCapturerCountChanged() override {}
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override {}
  content::BackForwardTransitionAnimationManager*
  GetBackForwardTransitionAnimationManager() override {
    return nullptr;
  }

#if BUILDFLAG(IS_MAC)
  bool CloseTabAfterEventTrackingIfNeeded() override { return false; }
#endif

  // RenderViewHostDelegateView methods.
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;
#if BUILDFLAG(ARKWEB_AI)
  bool CloseImageOverlaySelection() override;
#endif  // BUILDFLAG(ARKWEB_AI)

#if BUILDFLAG(ARKWEB_DRAG_DROP)
  gfx::Rect GetVisibleRectToWeb() override;
#endif

  void StartDragging(const content::DropData& drop_data,
                     const url::Origin& source_origin,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& cursor_offset,
                     const gfx::Rect& drag_obj_rect,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragOperation(ui::mojom::DragOperation operation,
                           bool document_is_handling_drag) override;
  void GotFocus(content::RenderWidgetHostImpl* render_widget_host) override;
  void LostFocus(content::RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;
  void FullscreenStateChanged(bool is_fullscreen) override {}

#if BUILDFLAG(ARKWEB_AI)
  void CreateOverlay(const gfx::ImageSkia& image,
                     const gfx::Rect& image_rect,
                     const gfx::Point& touch_point) override;
#endif

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
  void OnSafeInsetsChange(const gfx::Insets& safe_insets) override;
#endif
#if BUILDFLAG(ARKWEB_MENU)
  void MouseSelectMenuShow(bool show) override;
  void ChangeVisibilityOfQuickMenu() override;
#endif

#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  virtual content::WebContents* GetWebContents() override;
  void DidStopRefresh() override;
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize() const override;
  void UpdateBrowserControlsHeight(int height, bool animate) override;
#endif

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void ShowPopupMenu(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
#endif
  void DestroyBackForwardTransitionAnimationManager() override {}

 private:
  ArkWebRenderWidgetHostViewOSRExt* GetView() const;
  AlloyBrowserHostImpl* GetBrowser() const;
  CefTouchSelectionControllerClientOSR* GetSelectionControllerClient() const;

  const SkColor background_color_;
  const bool use_shared_texture_;
  const bool use_external_begin_frame_;

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  int top_controls_height_ = 0;
#endif

  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_
