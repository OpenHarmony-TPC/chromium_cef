// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/web_contents_view_osr.h"

#include "arkweb/build/features/features.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "cef/libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "cef/libcef/common/drag_data_impl.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
#include "components/performance_manager/embedder/performance_manager_registry.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
#include "base/command_line.h"
#include "cef/ohos_cef_ext/include/arkweb_client_ext.h"
#include "content/public/common/content_switches.h"
#endif

CefWebContentsViewOSR::CefWebContentsViewOSR(SkColor background_color,
                                             bool use_shared_texture,
                                             bool use_external_begin_frame)
    : background_color_(background_color),
      use_shared_texture_(use_shared_texture),
      use_external_begin_frame_(use_external_begin_frame) {}

CefWebContentsViewOSR::~CefWebContentsViewOSR() = default;

void CefWebContentsViewOSR::WebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
  LOG(INFO) << "CefWebContentsViewOSR::WebContentsCreated";
  if (auto* registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    registry->MaybeCreatePageNodeForWebContents(web_contents_);
  }
#endif

  RenderViewCreated();
}

void CefWebContentsViewOSR::RenderViewCreated() {
  if (auto* view = GetView()) {
    view->InstallTransparency();
  }
}

gfx::NativeView CefWebContentsViewOSR::GetNativeView() const {
  // TODO(osr): Fix all calling code paths and convert to DCHECK.
  NOTIMPLEMENTED();
  return gfx::NativeView();
}

gfx::NativeView CefWebContentsViewOSR::GetContentNativeView() const {
  // TODO(osr): Fix all calling code paths and convert to DCHECK.
  NOTIMPLEMENTED();
  return gfx::NativeView();
}

gfx::NativeWindow CefWebContentsViewOSR::GetTopLevelNativeWindow() const {
  // TODO(osr): Fix all calling code paths and convert to DCHECK.
  NOTIMPLEMENTED();
  return gfx::NativeWindow();
}

gfx::Rect CefWebContentsViewOSR::GetContainerBounds() const {
  return GetViewBounds();
}

void CefWebContentsViewOSR::GotFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl) {
      web_contents_impl->NotifyWebContentsFocused(render_widget_host);
    }
  }
}

void CefWebContentsViewOSR::LostFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl) {
      web_contents_impl->NotifyWebContentsLostFocus(render_widget_host);
    }
  }
}

void CefWebContentsViewOSR::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate()) {
    web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse);
  }
}

gfx::Rect CefWebContentsViewOSR::GetViewBounds() const {
  ArkWebRenderWidgetHostViewOSRExt* view = GetView();
  return view ? view->GetViewBounds() : gfx::Rect();
}

content::RenderWidgetHostViewBase* CefWebContentsViewOSR::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    return static_cast<content::RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  return new ArkWebRenderWidgetHostViewOSRExt(
      background_color_, use_shared_texture_, use_external_begin_frame_,
      render_widget_host, nullptr);
}

// Called for popup and fullscreen widgets.
content::RenderWidgetHostViewBase*
CefWebContentsViewOSR::CreateViewForChildWidget(
    content::RenderWidgetHost* render_widget_host) {
  ArkWebRenderWidgetHostViewOSRExt* view = GetView();
  CHECK(view);

  return new ArkWebRenderWidgetHostViewOSRExt(
      background_color_, use_shared_texture_, use_external_begin_frame_,
      render_widget_host, view);
}

void CefWebContentsViewOSR::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  auto selection_controller_client = GetSelectionControllerClient();
  if (selection_controller_client &&
      selection_controller_client->HandleContextMenu(params)) {
    // Context menu display, if any, will be handled via
    // AlloyWebContentsViewDelegate::ShowContextMenu.
    return;
  }

  if (auto browser = GetBrowser()) {
    browser->ShowContextMenu(params);
  }
}

#if BUILDFLAG(ARKWEB_AI)
bool CefWebContentsViewOSR::CloseImageOverlaySelection() {
  auto* rwhv = GetView();
  if (rwhv) {
    return rwhv->AsArkWebRenderWidgetHostViewOSRExt()
        ->CloseImageOverlaySelection();
  }
  return false;
}
#endif  // BUILDFLAG(ARKWEB_AI)

#if BUILDFLAG(ARKWEB_DRAG_DROP)
gfx::Rect CefWebContentsViewOSR::GetVisibleRectToWeb() {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    return browser->GetVisibleRectToWeb();
  }
  return gfx::Rect();
}
#endif

void CefWebContentsViewOSR::StartDragging(
    const content::DropData& drop_data,
    const url::Origin& source_origin,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->StartDragging(drop_data, allowed_ops, image, cursor_offset,
                           event_info, source_rwh);
#if BUILDFLAG(ARKWEB_DRAG_DROP)
    CefRenderWidgetHostViewOSR* view = GetView();
    if (view) {
      view->AsArkWebRenderWidgetHostViewOSRExt()
          ->SetTextHandlesTemporarilyHiddenByDrag(true, true);
    }
#endif
  } else if (web_contents_) {
    static_cast<content::WebContentsImpl*>(web_contents_)
        ->SystemDragEnded(source_rwh);
  }
}

void CefWebContentsViewOSR::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->UpdateDragOperation(operation, document_is_handling_drag);
  }
}

ArkWebRenderWidgetHostViewOSRExt* CefWebContentsViewOSR::GetView() const {
  if (web_contents_) {
    return static_cast<ArkWebRenderWidgetHostViewOSRExt*>(
        web_contents_->GetRenderViewHost()->GetWidget()->GetView());
  }
  return nullptr;
}

AlloyBrowserHostImpl* CefWebContentsViewOSR::GetBrowser() const {
  ArkWebRenderWidgetHostViewOSRExt* view = GetView();
  if (view) {
    return view->browser_impl().get();
  }
  return nullptr;
}

CefTouchSelectionControllerClientOSR*
CefWebContentsViewOSR::GetSelectionControllerClient() const {
  ArkWebRenderWidgetHostViewOSRExt* view = GetView();
  return view ? view->selection_controller_client() : nullptr;
}

#if BUILDFLAG(ARKWEB_AI)
void CefWebContentsViewOSR::CreateOverlay(const gfx::ImageSkia& image,
                                          const gfx::Rect& image_rect,
                                          const gfx::Point& touch_point) {
  ArkWebRenderWidgetHostViewOSRExt* view = GetView();
  if (view) {
    view->AsArkWebRenderWidgetHostViewOSRExt()->CreateOverlay(image, image_rect,
                                                              touch_point);
  }
}
#endif

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
void CefWebContentsViewOSR::OnSafeInsetsChange(const gfx::Insets& safe_insets) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl) {
      web_contents_impl->SetDisplayCutoutSafeArea(safe_insets);
    }
  }
}
#endif
#if BUILDFLAG(ARKWEB_MENU)
void CefWebContentsViewOSR::MouseSelectMenuShow(bool show) {
  auto* rwhv = GetView();
  if (rwhv) {
    rwhv->MouseSelectMenuShow(show);
  }
}

void CefWebContentsViewOSR::ChangeVisibilityOfQuickMenu() {
  auto* rwhv = GetView();
  if (rwhv) {
    rwhv->ChangeVisibilityOfQuickMenu();
  }
}
#endif

#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
void CefWebContentsViewOSR::DidStopRefresh() {
  if (ArkWebRenderWidgetHostViewOSRExt* view = GetView()) {
    view->AsArkWebRenderWidgetHostViewOSRExt()->DidStopRefresh();
  }
}

content::WebContents* CefWebContentsViewOSR::GetWebContents() {
  return web_contents_;
}
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
int CefWebContentsViewOSR::GetTopControlsHeight() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExTopControls)) {
    return 0;
  }

  int top_controls_height = top_controls_height_;

  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get() && browser->GetClient()) {
    top_controls_height =
        browser->GetClient()->AsArkWebClient()->OnGetTopControlsHeight();
  }
  if (top_controls_height != top_controls_height_) {
    LOG(INFO) << "browser controls height changed:" << top_controls_height_
              << "->" << top_controls_height;
    top_controls_height_ = top_controls_height;
    if (CefRenderWidgetHostViewOSR* view = GetView()) {
      view->OnTopControlsHeightChanged();
    }
  }

  return top_controls_height_;
}

bool CefWebContentsViewOSR::DoBrowserControlsShrinkRendererSize() const {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get() && browser->GetClient()) {
    return browser->GetClient()
        ->AsArkWebClient()
        ->DoBrowserControlsShrinkRendererSize();
  }
  return false;
}

void CefWebContentsViewOSR::UpdateBrowserControlsHeight(int height,
                                                        bool animate) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExTopControls) ||
      height == top_controls_height_) {
    return;
  }

  LOG(INFO) << "browser controls height changed:" << top_controls_height_
            << "->" << height;
  top_controls_height_ = height;

  if (CefRenderWidgetHostViewOSR* view = GetView()) {
    view->OnTopControlsHeightChanged();
    view->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      absl::nullopt);
  }
}
#endif  // BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void CefWebContentsViewOSR::ShowPopupMenu(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
#if BUILDFLAG(ARKWEB_HTML_SELECT)
  if (browser.get()) {
    browser->ShowPopupMenu(std::move(popup_client), bounds, item_height,
                           item_font_size, selected_item, std::move(menu_items),
                           right_aligned, allow_multiple_selection);
  }
#endif  // #if BUILDFLAG(ARKWEB_HTML_SELECT)
}
#endif
