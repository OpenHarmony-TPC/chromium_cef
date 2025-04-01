// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/touch_handle_drawable_osr.h"

#include <cmath>

#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ARKWEB)
#include "cef/ohos_cef_ext/include/arkweb_render_handler_ext.h"
#endif

namespace {
// Copied from touch_handle_drawable_aura.cc

// The distance by which a handle image is offset from the focal point (i.e.
// text baseline) downwards.
#if !BUILDFLAG(ARKWEB_MENU)
constexpr int kSelectionHandleVerticalVisualOffset = 2;
#endif  // !BUILDFLAG(ARKWEB_MENU)

// The padding around the selection handle image can be used to extend the
// handle so that touch events near the selection handle image are
// targeted to the selection handle.
constexpr int kSelectionHandlePadding = 0;

}  // namespace

int CefTouchHandleDrawableOSR::counter_ = 0;

CefTouchHandleDrawableOSR::CefTouchHandleDrawableOSR(
    CefRenderWidgetHostViewOSR* rwhv)
    : rwhv_(rwhv), id_(counter_++) {}

void CefTouchHandleDrawableOSR::SetEnabled(bool enabled) {
  if (enabled == enabled_) {
    return;
  }

  enabled_ = enabled;

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ENABLED;
  touch_handle_state.enabled = enabled_;
  TouchHandleStateChanged(touch_handle_state);
}

void CefTouchHandleDrawableOSR::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  if (orientation == orientation_) {
    return;
  }

  orientation_ = orientation;
#if BUILDFLAG(ARKWEB_MENU)
  UpdateVisiableBounds();
#else
  CefSize size;
  auto browser = rwhv_->browser_impl();
  auto handler = browser->GetClient()->GetRenderHandler();
  handler->GetTouchHandleSize(
      browser.get(), static_cast<cef_horizontal_alignment_t>(orientation_),
      size);

  const gfx::Size& image_size = gfx::Size(size.width, size.height);
  int handle_width = image_size.width() + 2 * kSelectionHandlePadding;
  int handle_height = image_size.height() + 2 * kSelectionHandlePadding;
  relative_bounds_ =
      gfx::RectF(-kSelectionHandlePadding,
                 kSelectionHandleVerticalVisualOffset - kSelectionHandlePadding,
                 handle_width, handle_height);

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ORIENTATION;
  touch_handle_state.orientation =
      static_cast<cef_horizontal_alignment_t>(orientation_);
  touch_handle_state.mirror_vertical = mirror_vertical;
  touch_handle_state.mirror_horizontal = mirror_horizontal;
  TouchHandleStateChanged(touch_handle_state);
#endif
}

void CefTouchHandleDrawableOSR::SetOrigin(const gfx::PointF& position) {
  if (position == origin_position_) {
    return;
  }

  origin_position_ = position;

#if !BUILDFLAG(ARKWEB_MENU)
  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ORIGIN;
  touch_handle_state.origin = {static_cast<int>(std::round(position.x())),
                               static_cast<int>(std::round(position.y()))};
  TouchHandleStateChanged(touch_handle_state);
#endif  // !BUILDFLAG(ARKWEB_MENU)
}

void CefTouchHandleDrawableOSR::SetAlpha(float alpha) {
  if (alpha == alpha_) {
    return;
  }

  alpha_ = alpha;

#if !BUILDFLAG(ARKWEB_MENU)
  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ALPHA;
  touch_handle_state.alpha = alpha_;
  TouchHandleStateChanged(touch_handle_state);
#endif  // !BUILDFLAG(ARKWEB_MENU)
}

gfx::RectF CefTouchHandleDrawableOSR::GetVisibleBounds() const {
  gfx::RectF bounds = relative_bounds_;
  bounds.Offset(origin_position_.x(), origin_position_.y());

#if !BUILDFLAG(ARKWEB_MENU)
  bounds.Inset(gfx::InsetsF::TLBR(
      kSelectionHandlePadding,
      kSelectionHandlePadding + kSelectionHandleVerticalVisualOffset,
      kSelectionHandlePadding, kSelectionHandlePadding));
#endif  // BUILDFLAG(ARKWEB_MENU)
  return bounds;
}

float CefTouchHandleDrawableOSR::GetDrawableHorizontalPaddingRatio() const {
  return 0.0f;
}

void CefTouchHandleDrawableOSR::TouchHandleStateChanged(
    const CefTouchHandleState& state) {
  auto browser = rwhv_->browser_impl();
  auto handler = browser->GetClient()->GetRenderHandler();
  handler->OnTouchHandleStateChanged(browser.get(), state);
}

#if BUILDFLAG(ARKWEB_MENU)
void CefTouchHandleDrawableOSR::SetEdge(const gfx::PointF& top,
                                        const gfx::PointF& bottom) {
  edge_height_ = std::abs(bottom.y() - top.y());

  UpdateVisiableBounds();
}

void CefTouchHandleDrawableOSR::UpdateVisiableBounds() {
  CefSize size;
  auto browser = rwhv_->browser_impl();
  auto handler = browser->GetClient()->GetRenderHandler();
  handler->GetTouchHandleSize(
      browser.get(), static_cast<cef_horizontal_alignment_t>(orientation_),
      size);

  const gfx::Size& image_size = gfx::Size(size.width, size.height);
  int handle_width = image_size.width() + 2 * kSelectionHandlePadding;
  int handle_height = image_size.height() + 2 * kSelectionHandlePadding;
  if (orientation_ != ui::TouchHandleOrientation::LEFT) {
    relative_bounds_ = gfx::RectF(-handle_width / 2.0, -edge_height_,
                                  handle_width, handle_height + edge_height_);
  } else {
    relative_bounds_ =
        gfx::RectF(-handle_width / 2.0, -edge_height_ - handle_height,
                   handle_width, handle_height + edge_height_);
  }
}
#endif  // BUILDFLAG(ARKWEB_MENU)
