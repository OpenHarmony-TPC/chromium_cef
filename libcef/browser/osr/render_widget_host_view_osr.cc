// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/base/switches.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/osr/browser_platform_delegate_osr.h"
#include "cef/libcef/browser/osr/osr_util.h"
#include "cef/libcef/browser/osr/synthetic_gesture_target_osr.h"
#include "cef/libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "cef/libcef/browser/osr/video_consumer_osr.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "arkweb/build/features/features.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#include "cef/libcef/browser/osr/browser_platform_delegate_osr.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_OHOS)
#include "base/ohos/sys_info_utils_ext.h"
#include "cef/ohos_cef_ext/include/arkweb_render_handler_ext.h"
#include "content/browser/gpu/gpu_process_host.h"

#if BUILDFLAG(ARKWEB_SCROLLBAR)
const int SCALE_FACTOR_CONVERT_RATIO = 100;
#endif

std::unordered_map<gfx::AcceleratedWidget, ui::Compositor*>
    CefRenderWidgetHostViewOSR::compositor_map_;

std::unordered_map<gfx::AcceleratedWidget, uint32_t>
    CefRenderWidgetHostViewOSR::accelerate_widget_map_;
#endif

#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
#include "third_party/ohos_ndk/includes/ohos_adapter/res_sched_client_adapter.h"
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
#include "libcef/browser/native/cursor_util.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/blink/did_overscroll_params.h"
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
const int SOC_PERF_WEB_GESTURE_ID = 10012;
const int TOUCH_DOWN_DELAY_TIME = 200;
const int TOUCH_UP_DURATION_TIME = 100;
#endif

#if BUILDFLAG(ARKWEB_DRAG_RESIZE)
#include "ohos_nweb/src/nweb_resize_helper.h"
#endif

namespace {

// The maximum number of damage rects to cache for outstanding frame requests
// (for OnAcceleratedPaint).
const size_t kMaxDamageRects = 10;

const float kDefaultScaleFactor = 1.0;

#if BUILDFLAG(IS_ARKWEB)
display::mojom::ScreenOrientation ConvertOrientationType(
    cef_screen_orientation_type_t type) {
  switch (type) {
    case cef_screen_orientation_type_t::UNDEFINED:
      return display::mojom::ScreenOrientation::kUndefined;
    case cef_screen_orientation_type_t::PORTRAIT_PRIMARY:
      return display::mojom::ScreenOrientation::kPortraitPrimary;
    case cef_screen_orientation_type_t::LANDSCAPE_PRIMARY:
      return display::mojom::ScreenOrientation::kLandscapePrimary;
    case cef_screen_orientation_type_t::PORTRAIT_SECONDARY:
      return display::mojom::ScreenOrientation::kPortraitSecondary;
    case cef_screen_orientation_type_t::LANDSCAPE_SECONDARY:
      return display::mojom::ScreenOrientation::kLandscapeSecondary;
    default:
      return display::mojom::ScreenOrientation::kUndefined;
  }
}
#endif

display::ScreenInfo ScreenInfoFrom(const CefScreenInfo& src) {
  display::ScreenInfo screenInfo;
  screenInfo.device_scale_factor = src.device_scale_factor;
  screenInfo.depth = src.depth;
  screenInfo.depth_per_component = src.depth_per_component;
  screenInfo.is_monochrome = src.is_monochrome ? true : false;
  screenInfo.rect =
      gfx::Rect(src.rect.x, src.rect.y, src.rect.width, src.rect.height);
  screenInfo.available_rect =
      gfx::Rect(src.available_rect.x, src.available_rect.y,
                src.available_rect.width, src.available_rect.height);
#if BUILDFLAG(IS_ARKWEB)
  screenInfo.orientation_angle = src.angle;
  screenInfo.orientation_type = ConvertOrientationType(src.orientation);
#endif
  return screenInfo;
}

class CefDelegatedFrameHostClient : public content::DelegatedFrameHostClient {
 public:
  explicit CefDelegatedFrameHostClient(CefRenderWidgetHostViewOSR* view)
      : view_(view) {}

  CefDelegatedFrameHostClient(const CefDelegatedFrameHostClient&) = delete;
  CefDelegatedFrameHostClient& operator=(const CefDelegatedFrameHostClient&) =
      delete;

  ui::Layer* DelegatedFrameHostGetLayer() const override {
    return view_->GetRootLayer();
  }

  bool DelegatedFrameHostIsVisible() const override {
    // Called indirectly from DelegatedFrameHost::WasShown.
    return view_->IsShowing();
  }

  SkColor DelegatedFrameHostGetGutterColor() const override {
    // When making an element on the page fullscreen the element's background
    // may not match the page's, so use black as the gutter color to avoid
    // flashes of brighter colors during the transition.
    if (view_->render_widget_host()->delegate() &&
        view_->render_widget_host()->delegate()->IsFullscreen()) {
      return SK_ColorBLACK;
    }
    return *view_->GetBackgroundColor();
  }

  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {
    view_->render_widget_host()->DidProcessFrame(frame_token, activation_time);
  }

  float GetDeviceScaleFactor() const override {
    return view_->GetDeviceScaleFactor();
  }

  viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction() override {
    viz::FrameEvictorClient::EvictIds ids;
    ids.embedded_ids =
        view_->render_widget_host()->CollectSurfaceIdsForEviction();
    return ids;
  }

  void InvalidateLocalSurfaceIdOnEviction() override {
    view_->InvalidateLocalSurfaceId();
  }

  bool ShouldShowStaleContentOnEviction() override { return false; }

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
  void RestoreRenderFit() override {
    if (view_ && view_->AsArkWebRenderWidgetHostViewOSRExt()) {
      view_->AsArkWebRenderWidgetHostViewOSRExt()->RestoreRenderFit();
    }
  }
#endif  // ARKWEB_MAXIMIZE_RESIZE

 private:
  const raw_ptr<CefRenderWidgetHostViewOSR> view_;
};

ui::GestureProvider::Config CreateGestureProviderConfig() {
  ui::GestureProvider::Config config = ui::GetGestureProviderConfig(
      ui::GestureProviderConfigType::CURRENT_PLATFORM);
  return config;
}

ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  base::TimeTicks time = event.TimeStamp();
  if (!time.is_null()) {
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time);
  }
  return latency_info;
}

gfx::Rect GetViewBounds(AlloyBrowserHostImpl* browser) {
  if (!browser) {
    return gfx::Rect();
  }

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->GetViewRect(browser, rc);
  CHECK_GT(rc.width, 0);
  CHECK_GT(rc.height, 0);

  return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
}

ui::ImeTextSpan::UnderlineStyle GetImeUnderlineStyle(
    cef_composition_underline_style_t style) {
  switch (style) {
    case CEF_CUS_SOLID:
      return ui::ImeTextSpan::UnderlineStyle::kSolid;
    case CEF_CUS_DOT:
      return ui::ImeTextSpan::UnderlineStyle::kDot;
    case CEF_CUS_DASH:
      return ui::ImeTextSpan::UnderlineStyle::kDash;
    case CEF_CUS_NONE:
      return ui::ImeTextSpan::UnderlineStyle::kNone;
  }

  DCHECK(false);
  return ui::ImeTextSpan::UnderlineStyle::kSolid;
}

}  // namespace

// Logic copied from RenderWidgetHostViewAura::CreateSelectionController.
void CefRenderWidgetHostViewOSR::CreateSelectionController() {
  ui::TouchSelectionController::Config tsc_config;
  tsc_config.max_tap_duration = base::Milliseconds(
      ui::GestureConfiguration::GetInstance()->long_press_time_in_ms());
  tsc_config.tap_slop = ui::GestureConfiguration::GetInstance()
                            ->max_touch_move_in_pixels_for_click();
#if BUILDFLAG(ARKWEB_MENU)
  tsc_config.enable_longpress_drag_selection = true;
#else
  tsc_config.enable_longpress_drag_selection = false;
#endif
  selection_controller_ = std::make_unique<ui::TouchSelectionController>(
      selection_controller_client_.get(), tsc_config);
}

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame,
    content::RenderWidgetHost* widget,
    CefRenderWidgetHostViewOSR* parent_host_view)
    : content::RenderWidgetHostViewBase(widget),
      background_color_(background_color),
      use_shared_texture_(use_shared_texture),
      render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
      has_parent_(parent_host_view != nullptr),
      parent_host_view_(parent_host_view),
      pinch_zoom_enabled_(input::switches::IsPinchToZoomEnabled()),
      mouse_wheel_phase_handler_(this),
      gesture_provider_(CreateGestureProviderConfig(), this),
      weak_ptr_factory_(this) {
  DCHECK(render_widget_host_);
  DCHECK(!render_widget_host_->GetView());

  if (parent_host_view_) {
    browser_impl_ = parent_host_view_->browser_impl();
    DCHECK(browser_impl_);
#if BUILDFLAG(IS_ARKWEB)
    is_popup_ = true;
#endif
  } else if (content::RenderViewHost::From(render_widget_host_)) {
    // AlloyBrowserHostImpl might not be created at this time for popups.
    browser_impl_ = AlloyBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }

  delegated_frame_host_client_ =
      std::make_unique<CefDelegatedFrameHostClient>(this);

  // Matching the attributes from BrowserCompositorMac.
  delegated_frame_host_ = std::make_unique<content::DelegatedFrameHost>(
      AllocateFrameSinkId(), delegated_frame_host_client_.get(),
#if BUILDFLAG(IS_OHOS)
      true /* should_register_frame_sink_id */);
#else
      false /* should_register_frame_sink_id */);
#endif

  root_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);

  bool opaque = SkColorGetA(background_color_) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(background_color_);

  external_begin_frame_enabled_ = use_external_begin_frame;

  auto context_factory = content::GetContextFactory();

  // Matching the attributes from RecyclableCompositorMac.
#ifdef DISABLE_GPU
  compositor_ = std::make_unique<ui::Compositor>(
      context_factory->AllocateFrameSinkId(), context_factory,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      false /* enable_pixel_canvas */, use_external_begin_frame);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

  compositor_->SetDelegate(this);
  compositor_->SetRootLayer(root_layer_.get());
  compositor_->AddChildFrameSink(GetFrameSinkId());

  content::RenderWidgetHostImpl* render_widget_host_impl =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (render_widget_host_impl) {
    render_widget_host_impl->SetCompositorForFlingScheduler(compositor_.get());
  }
#else
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "compositor construct, widget = "
            << static_cast<uint32_t>(
                   browser_impl_->GetAcceleratedWidget(is_popup_));
  ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  accelerate_widget_map_[browser_impl_->GetAcceleratedWidget(is_popup_)]++;
  if (!compositor) {
    compositor = new ui::Compositor(
        context_factory->AllocateFrameSinkId(), context_factory,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        false /* enable_pixel_canvas */, use_external_begin_frame);
    compositor->SetAcceleratedWidget(
        browser_impl_->GetAcceleratedWidget(is_popup_));
    CefRenderWidgetHostViewOSR::AddCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_), compositor);
  }
#endif  // IS_OHOS
#endif

  cursor_manager_ = std::make_unique<input::CursorManager>(this);

  // This may result in a call to GetFrameSinkId().
  render_widget_host_->SetView(this);

  if (GetTextInputManager()) {
    GetTextInputManager()->AddObserver(this);
  }

  if (browser_impl_ && !parent_host_view_) {
    // For child/popup views this will be called from the associated InitAs*()
    // method.
    SetRootLayerSize(false /* force */);
    if (!render_widget_host_->is_hidden()) {
      Show();
    }
  }
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  selection_controller_client_ =
      std::make_unique<ArkWebTouchSelectionControllerClientOSRExt>(this);
#else
  selection_controller_client_ =
      std::make_unique<CefTouchSelectionControllerClientOSR>(this);
#endif
  CreateSelectionController();

#if BUILDFLAG(ARKWEB_ZOOM)
  bool excludable_devices = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDoubleTapSupportForPlatformEnabled);
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(excludable_devices);
#endif
}

CefRenderWidgetHostViewOSR::~CefRenderWidgetHostViewOSR() {
  ReleaseCompositor();
  root_layer_.reset(nullptr);

  DCHECK(!parent_host_view_);
  DCHECK(!popup_host_view_);
  DCHECK(!child_host_view_);
  DCHECK(guest_host_views_.empty());

  if (text_input_manager_) {
    text_input_manager_->RemoveObserver(this);
  }
}

void CefRenderWidgetHostViewOSR::ReleaseCompositor() {
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
#ifdef DISABLE_GPU
  if (!compositor_) {
    return;  // already released
  }
#else
  if (!browser_impl_) {
    return;
  }
  auto it1 = accelerate_widget_map_.find(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (it1 == accelerate_widget_map_.end()) {
    return;
  }
#endif
  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
  if (delegated_frame_host_) {
    if (is_showing_) {
      delegated_frame_host_->WasHidden(
          content::DelegatedFrameHost::HiddenCause::kOther);
    }
    delegated_frame_host_->DetachFromCompositor();
    delegated_frame_host_.reset(nullptr);
  }

#ifdef DISABLE_GPU
  compositor_.reset(nullptr);
#else
  LOG(INFO) << "ReleaseCompositor";
  auto com =
      compositor_map_.find(browser_impl_->GetAcceleratedWidget(is_popup_));
  if (--accelerate_widget_map_[browser_impl_->GetAcceleratedWidget(
          is_popup_)] == 0) {
    if (!browser_impl_) {
      return;
    }
    LOG(INFO) << "ReleaseCompositor, widget = "
              << static_cast<uint32_t>(
                     browser_impl_->GetAcceleratedWidget(is_popup_));
    if (com != compositor_map_.end()) {
      if (com->second != nullptr) {
        delete com->second;
      }
      compositor_map_.erase(com);
    }
    accelerate_widget_map_.erase(
        browser_impl_->GetAcceleratedWidget(is_popup_));
  } else {
    if (com != compositor_map_.end()) {
      if (com->second->delegate() == this) {
        com->second->SetDelegate(nullptr);
      }
    }
  }
#endif  // DISABLE_GPU
#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  if (overscroll_controller_) {
    overscroll_controller_.reset();
  }
#endif  // ARKWEB_EXT_PULL_TO_REFRESH
#else
  if (!compositor_) {
    return;  // already released
  }

  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
  if (is_showing_) {
    delegated_frame_host_->WasHidden(
        content::DelegatedFrameHost::HiddenCause::kOther);
  }
  delegated_frame_host_->DetachFromCompositor();
  delegated_frame_host_.reset(nullptr);

  content::RenderWidgetHostImpl* render_widget_host_impl =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (render_widget_host_impl) {
    render_widget_host_impl->SetCompositorForFlingScheduler(nullptr);
  }

  host_display_client_ = nullptr;

  compositor_.reset(nullptr);
#endif  // ARKWEB_COMPOSITE_RENDER
}

// Called for full-screen widgets.
void CefRenderWidgetHostViewOSR::InitAsChild(gfx::NativeView parent_view) {
  DCHECK(parent_host_view_);
  DCHECK(browser_impl_);

  if (parent_host_view_->child_host_view_) {
    // Cancel the previous popup widget.
    parent_host_view_->child_host_view_->CancelWidget();
  }

  parent_host_view_->set_child_host_view(this);

  // The parent view should not render while the full-screen view exists.
  parent_host_view_->Hide();

  SetRootLayerSize(false /* force */);
  Show();
}

void CefRenderWidgetHostViewOSR::SetSize(const gfx::Size& size) {}

void CefRenderWidgetHostViewOSR::SetBounds(const gfx::Rect& rect) {}

gfx::NativeView CefRenderWidgetHostViewOSR::GetNativeView() {
  // TODO(osr): Fix all calling code paths and convert to DCHECK.
  NOTIMPLEMENTED();
  return gfx::NativeView();
}

gfx::NativeViewAccessible
CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return gfx::NativeViewAccessible();
}

void CefRenderWidgetHostViewOSR::Focus() {
#if BUILDFLAG(ARKWEB_FOCUS)
  if (!render_widget_host_) {
    return;
  }
  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  widget->GotFocus();
  widget->SetActive(true);
#endif
#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  OnFocusInternal();
#endif
}

bool CefRenderWidgetHostViewOSR::HasFocus() {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  content::RenderWidgetHostImpl* target_host = render_widget_host_;
  if (render_widget_host_ && render_widget_host_->delegate()) {
    target_host = render_widget_host_->delegate()->GetFocusedRenderWidgetHost(
        render_widget_host_);
  }
  if (target_host && target_host->GetView()) {
    return target_host->is_focused();
  }
#endif

  return false;
}

bool CefRenderWidgetHostViewOSR::IsSurfaceAvailableForCopy() {
  return delegated_frame_host_
             ? delegated_frame_host_->CanCopyFromCompositingSurface()
             : false;
}

void CefRenderWidgetHostViewOSR::ShowWithVisibility(
    content::PageVisibilityState) {
  if (is_showing_) {
    return;
  }

  if (!content::GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled() &&
      !browser_impl_ &&
      (!parent_host_view_ || !parent_host_view_->browser_impl_)) {
    return;
  }

  is_showing_ = true;

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  top_controls_offset_ = 0;
  top_content_offset_ = 0;
#endif

#if !BUILDFLAG(IS_ARKWEB)
  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // and no other visual properties have since been changed. Allocate a new id
  // and start synchronizing.
  if (!GetLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                GetLocalSurfaceId());
  }
#endif

#ifndef DISABLE_GPU
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  compositor->SetDelegate(this);
  compositor->SetRootLayer(root_layer_.get());
  content::RenderWidgetHostImpl* render_widget_host_impl =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (render_widget_host_impl) {
    render_widget_host_impl->SetCompositorForFlingScheduler(compositor);
  }
#endif
  LOG(INFO) << "CefRenderWidgetHostViewOSR::ShowWithVisibility compositor"
            << compositor;

  if (render_widget_host_) {
    render_widget_host_->WasShown(
        /*record_tab_switch_time_request=*/{});

    // Call OnRenderFrameMetadataChangedAfterActivation for every frame.
    auto provider = content::RenderWidgetHostImpl::From(render_widget_host_)
                        ->render_frame_metadata_provider();
    provider->AddObserver(this);
    provider->ReportAllFrameSubmissionsForTesting(true);
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (GetTextInputManager() && !GetTextInputManager()->HasObserver(this)) {
    GetTextInputManager()->AddObserver(this);
  }
#endif

  if (delegated_frame_host_) {
#ifdef DISABLE_GPU
    delegated_frame_host_->AttachToCompositor(compositor_.get());
#else
    LOG(INFO)
        << "CefRenderWidgetHostViewOSR::ShowWithVisibility AttachToCompositor";
    delegated_frame_host_->AttachToCompositor(compositor);
#endif

    delegated_frame_host_->WasShown(GetLocalSurfaceId(),
#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
                                    GetPhysicalViewBounds().size(),
#else
                                    GetViewBounds().size(),
#endif
                                    /*record_tab_switch_time_request=*/{});
  }

#if BUILDFLAG(IS_ARKWEB)
  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // and no other visual properties have since been changed. Allocate a new id
  // and start synchronizing.
  if (!GetLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                GetLocalSurfaceId());
  }
#endif

  if (!content::GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    // Start generating frames when we're visible and at the correct size.
    if (!video_consumer_) {
#ifdef DISABLE_GPU
      video_consumer_ =
          std::make_unique<CefVideoConsumerOSR>(this, use_shared_texture_);
      UpdateFrameRate();
#endif
    } else {
      video_consumer_->SetActive(true);
    }
  }

#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  if (overscroll_controller_) {
    overscroll_controller_->Enable();
  }
#endif
}

void CefRenderWidgetHostViewOSR::Hide() {
  if (!is_showing_) {
    return;
  }

  is_showing_ = false;

  if (browser_impl_) {
    browser_impl_->CancelContextMenu();
  }

  if (selection_controller_client_) {
    selection_controller_client_->CloseQuickMenuAndHideHandles();
  }

  if (video_consumer_) {
    video_consumer_->SetActive(false);
  }

  if (render_widget_host_) {
    render_widget_host_->WasHidden();

    auto provider = content::RenderWidgetHostImpl::From(render_widget_host_)
                        ->render_frame_metadata_provider();
    provider->RemoveObserver(this);
  }

  if (delegated_frame_host_) {
    delegated_frame_host_->WasHidden(
        content::DelegatedFrameHost::HiddenCause::kOther);
    delegated_frame_host_->DetachFromCompositor();
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (GetTextInputManager()) {
    GetTextInputManager()->RemoveObserver(this);
  }
#endif

#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  if (overscroll_controller_) {
    overscroll_controller_->Enable();
  }
#endif
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return is_showing_;
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void CefRenderWidgetHostViewOSR::SendTouchEventList(
    const std::vector<CefTouchEvent>& event_list) {
  TRACE_EVENT0("base", "CefRenderWidgetHostViewOSR::SendTouchEventList");

  for (const auto& event : event_list) {
#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
    if (event.type == CEF_TET_PRESSED) {
      is_editable_node_ = false;
      auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
          browser_impl_->GetAcceleratedWidget(is_popup_));
      if (compositor) {
        compositor->SetCurrentFrameSinkId(GetFrameSinkId());
      } else {
        LOG(ERROR) << "compositor is null when send touch event";
      }
    }
#endif

    if (!IsPopupWidget() && popup_host_view_) {
      if (!forward_touch_to_popup_ && event.type == CEF_TET_PRESSED &&
          pointer_state_.GetPointerCount() == 0) {
        forward_touch_to_popup_ =
            popup_host_view_->popup_position_.Contains(event.x, event.y);
      }

      if (forward_touch_to_popup_) {
        CefTouchEvent popup_event(event);
        popup_event.x -= popup_host_view_->popup_position_.x();
        popup_event.y -= popup_host_view_->popup_position_.y();
        popup_host_view_->SendTouchEvent(popup_event);
        return;
      }
    }
  }

  bool had_no_pointer = true;
  std::vector<CefTouchEvent> filtered_event_list;
  for (const auto& event : event_list) {
    // Update the touch event first.
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    had_no_pointer = had_no_pointer && !pointer_state_.GetPointerCount();
    pointer_state_.SetFromOverlay(event.from_overlay);
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)
    if (!pointer_state_.OnTouch(event)) {
      continue;
    }
#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
    OnTouchMove();
#endif
    if (selection_controller_->WillHandleTouchEvent(pointer_state_)) {
      pointer_state_.CleanupRemovedTouchPoints(event);
      continue;
    }
    filtered_event_list.emplace_back(event);
  }
  if (filtered_event_list.empty()) {
    return;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(pointer_state_);

  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, result.moved_beyond_slop_region, false);

  for (const auto& event : filtered_event_list) {
    pointer_state_.CleanupRemovedTouchPoints(event);

    // Set unchanged touch point to StateStationary for touchmove and
    // touchcancel to make sure only send one ack per WebTouchEvent.
    if (!result.succeeded) {
      pointer_state_.MarkUnchangedTouchPointsAsStationary(&touch_event, event);
    }
  }

  if (!render_widget_host_) {
    return;
  }

  SendTouchGestureEvent(touch_event);

  bool touch_end =
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
void CefRenderWidgetHostViewOSR::StopBoosting() {
  if (is_fling_) {
    return;
  }
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, false);
}

void CefRenderWidgetHostViewOSR::BoostingPreiodly() {
  if (pointer_state_.GetPointerCount() == 0) {
    return;
  }
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, true);
  LOG(DEBUG) << "hwtlog:CefRenderWidgetHostViewOSR::BoostingPreiodly";
  CEF_POST_DELAYED_TASK(
      CEF_UIT,
      base::BindOnce(&ArkWebRenderWidgetHostViewOSRExt::BoostingPreiodly,
                     weak_ptr_factory_.GetWeakPtr()),
      TOUCH_DOWN_DELAY_TIME);
}

void CefRenderWidgetHostViewOSR::OnTouchDown() {
  if (pointer_state_.GetPointerCount() == 0) {
    has_touch_point_ = false;
    if (isBoosting_) {
      isBoosting_ = false;
      CEF_POST_DELAYED_TASK(
          CEF_UIT,
          base::BindOnce(&CefRenderWidgetHostViewOSR::StopBoosting,
                         weak_ptr_factory_.GetWeakPtr()),
          TOUCH_UP_DURATION_TIME);
      auto* host = content::GpuProcessHost::Get();
      if (host && host->gpu_host()) {
        host->gpu_host()->SetHasTouchPoint(false);
      }
    }
    return;
  }
  if (isBoosting_) {
    OHOS::NWeb::OhosAdapterHelper::GetInstance()
        .CreateSocPerfClientAdapter()
        ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, true);
    LOG(DEBUG) << "hwtlog:CefRenderWidgetHostViewOSR::OnTouchDown";
    CEF_POST_DELAYED_TASK(
        CEF_UIT,
        base::BindOnce(&CefRenderWidgetHostViewOSR::OnTouchDown,
                       weak_ptr_factory_.GetWeakPtr()),
        TOUCH_DOWN_DELAY_TIME);
  }
#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
  OHOS::NWeb::ResSchedClientAdapter::ReportScene(
      OHOS::NWeb::ResSchedStatusAdapter::WEB_SCENE_ENTER,
      OHOS::NWeb::ResSchedSceneAdapter::SLIDE);
#endif
  if (!has_touch_point_) {
    if (auto* host = content::GpuProcessHost::Get()) {
      if (auto* host_impl = host->gpu_host()) {
        host_impl->SetHasTouchPoint(true);
      }
    }
  }
  has_touch_point_ = true;
}

void CefRenderWidgetHostViewOSR::OnTouchMove() {
  if (pointer_state_.GetPointerCount() == 0) {
    return;
  }
  isBoosting_ = true;
  BoostingPreiodly();
}
#endif

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
void CefRenderWidgetHostViewOSR::EvictFrameBackBuffers(bool invisible) {
  TRACE_EVENT1("base", "CefRenderWidgetHostViewOSR::EvictFrameBackBuffers",
               "invisible", invisible);
  if (browser_impl_.get() && browser_impl_->GetAcceleratedWidget(is_popup_)) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_));
    if (compositor) {
      compositor->EvictFrameBackBuffers(invisible);
    }
  }
}
#endif

void CefRenderWidgetHostViewOSR::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              std::nullopt);
}

content::TouchSelectionControllerClientManager*
CefRenderWidgetHostViewOSR::GetTouchSelectionControllerClientManager() {
  return selection_controller_client_.get();
}

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
gfx::Rect CefRenderWidgetHostViewOSR::GetPhysicalViewBounds() {
  // the physical view bounds means the size and position information
  // of the device, utilized for the root layer and surface.
  if (IsPopupWidget()) {
    return popup_position_;
  }

  return current_view_bounds_;
}

gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() {
  // the view bounds refers to the size of the viewport
  // area where the webpage is drawn and rendered.
  gfx::Rect bounds = GetPhysicalViewBounds();
  bounds.set_height(bounds.height() - GetShrinkViewportHeight());

  return bounds;
}
#else
gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() {
  if (IsPopupWidget()) {
    return popup_position_;
  }

  return current_view_bounds_;
}
#endif

void CefRenderWidgetHostViewOSR::SetBackgroundColor(SkColor color) {
  // The renderer will feed its color back to us with the first CompositorFrame.
  // We short-cut here to show a sensible color before that happens.
  UpdateBackgroundColorFromRenderer(color);

  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  content::RenderWidgetHostViewBase::SetBackgroundColor(color);
}

std::optional<SkColor> CefRenderWidgetHostViewOSR::GetBackgroundColor() {
  return background_color_;
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColor() {
#if BUILDFLAG(ARKWEB_BACKGROUND_COLOR)
  if (SkColorGetA(background_color_) != SK_AlphaOPAQUE) {
#ifdef DISABLE_GPU
    if (compositor_) {
      compositor_->SetBackgroundColor(background_color_);
    }
#else
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_));
    if (compositor) {
      compositor->SetBackgroundColor(background_color_);
    }
#endif
  }
#endif  // BUILDFLAG(ARKWEB_BACKGROUND_COLOR)
}

std::optional<content::DisplayFeature>
CefRenderWidgetHostViewOSR::GetDisplayFeature() {
  return std::nullopt;
}

void CefRenderWidgetHostViewOSR::SetDisplayFeatureForTesting(
    const content::DisplayFeature* display_feature) {
  DCHECK(false);
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::LockPointer(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kPermissionDenied;
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::ChangePointerLock(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kPermissionDenied;
}

void CefRenderWidgetHostViewOSR::UnlockPointer() {}

void CefRenderWidgetHostViewOSR::TakeFallbackContentFrom(
    content::RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  CefRenderWidgetHostViewOSR* view_cef =
      static_cast<CefRenderWidgetHostViewOSR*>(view);
  SetBackgroundColor(view_cef->background_color_);
  if (delegated_frame_host_ && view_cef->delegated_frame_host_) {
    delegated_frame_host_->TakeFallbackContentFrom(
        view_cef->delegated_frame_host_.get());
  }
}

void CefRenderWidgetHostViewOSR::OnPresentCompositorFrame() {}

void CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  if (host()->is_hidden()) {
    // When an embedded child responds, we want to accept its changes to the
    // viz::LocalSurfaceId. However we do not want to embed surfaces while
    // hidden. Nor do we want to embed invalid ids when we are evicted. Becoming
    // visible will generate a new id, if necessary, and begin embedding.
    UpdateLocalSurfaceIdFromEmbeddedClient(metadata.local_surface_id);
  } else {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                metadata.local_surface_id);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    // Fix egl resize pending problem.
    ReleaseResizeHold();
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
  }
}

void CefRenderWidgetHostViewOSR::AllocateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  parent_local_surface_id_allocator_->GenerateId();
}

const viz::LocalSurfaceId&
CefRenderWidgetHostViewOSR::GetCurrentLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::UpdateLocalSurfaceIdFromEmbeddedClient(
    const std::optional<viz::LocalSurfaceId>&
        embedded_client_local_surface_id) {
  if (embedded_client_local_surface_id) {
    parent_local_surface_id_allocator_->UpdateFromChild(
        *embedded_client_local_surface_id);
  } else {
    AllocateLocalSurfaceId();
  }
}

const viz::LocalSurfaceId&
CefRenderWidgetHostViewOSR::GetOrCreateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    AllocateLocalSurfaceId();
  }
  return GetCurrentLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::InvalidateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    return;
  }
  parent_local_surface_id_allocator_->Invalidate();
}

void CefRenderWidgetHostViewOSR::AddDamageRect(uint32_t sequence,
                                               const gfx::Rect& rect) {
  // Associate the given damage rect with the presentation token.
  // For OnAcceleratedPaint we'll lookup the corresponding damage area based on
  // the frame token which is passed back to OnPresentCompositorFrame.
  base::AutoLock lock_scope(damage_rect_lock_);

  // We assume our presentation_token is a counter. Since we're using an ordered
  // map we can enforce a max size and remove oldest from the front. Worst case,
  // if a damage rect isn't associated, we can simply pass the entire view size.
  while (damage_rects_.size() >= kMaxDamageRects) {
    damage_rects_.erase(damage_rects_.begin());
  }
  damage_rects_[sequence] = rect;
}

void CefRenderWidgetHostViewOSR::InvalidateLocalSurfaceIdAndAllocationGroup() {
  InvalidateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::ClearFallbackSurfaceForCommitPending() {
  if (delegated_frame_host_) {
    delegated_frame_host_->ClearFallbackSurfaceForCommitPending();
  }
  InvalidateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_) {
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
  }
}

void CefRenderWidgetHostViewOSR::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds,
    const gfx::Rect& anchor_rect) {
#if BUILDFLAG(IS_ARKWEB)
  if (base::ohos::IsPcDevice()) {
#endif
    DCHECK_EQ(parent_host_view_, parent_host_view);
    DCHECK(browser_impl_);

    if (parent_host_view_->popup_host_view_) {
      // Cancel the previous popup widget.
      parent_host_view_->popup_host_view_->CancelWidget();
    }

    parent_host_view_->set_popup_host_view(this);

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);

    handler->OnPopupShow(browser_impl_.get(), true);

    CefRect view_rect;
    handler->GetViewRect(browser_impl_.get(), view_rect);
    gfx::Rect client_pos(bounds.x() - view_rect.x, bounds.y() - view_rect.y,
                         bounds.width(), bounds.height());

    popup_position_ = client_pos;

    CefRect widget_pos(client_pos.x(), client_pos.y(), client_pos.width(),
                       client_pos.height());

    if (handler.get()) {
      handler->OnPopupSize(browser_impl_.get(), widget_pos);
    }

    // The size doesn't change for popups so we need to force the
    // initialization.
    SetRootLayerSize(true /* force */);
    Show();
#if BUILDFLAG(IS_ARKWEB)
  }
#endif
}

void CefRenderWidgetHostViewOSR::UpdateCursor(const ui::Cursor& cursor) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (!browser_impl_) {
    LOG(ERROR) << "browser is null when update cursor";
    return;
  }
  cursor_util::OnCursorChange(browser_impl_->GetBrowser(), cursor);
#endif
}

input::CursorManager* CefRenderWidgetHostViewOSR::GetCursorManager() {
  return cursor_manager_.get();
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
  if (!is_loading) {
    return;
  }
#if !BUILDFLAG(ARKWEB_SCROLL_PERFORMANCE)
  // Make sure gesture detection is fresh.
  gesture_provider_.ResetDetection();
  forward_touch_to_popup_ = false;
#endif
}

void CefRenderWidgetHostViewOSR::RenderProcessGone() {
  Destroy();
}

void CefRenderWidgetHostViewOSR::Destroy() {
  if (!is_destroyed_) {
    is_destroyed_ = true;

    if (has_parent_) {
      CancelWidget();
    } else {
      if (popup_host_view_) {
        popup_host_view_->CancelWidget();
      }
      if (child_host_view_) {
        child_host_view_->CancelWidget();
      }
      if (!guest_host_views_.empty()) {
        // Guest RWHVs will be destroyed when the associated RWHVGuest is
        // destroyed. This parent RWHV may be destroyed first, so disassociate
        // the guest RWHVs here without destroying them.
        for (auto guest_host_view : guest_host_views_) {
          guest_host_view->parent_host_view_ = nullptr;
        }
        guest_host_views_.clear();
      }
      Hide();
    }
  }

  delete this;
}

void CefRenderWidgetHostViewOSR::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
  if (!browser_impl_.get()) {
    return;
  }

#if BUILDFLAG(IS_OHOS)
  const size_t kMaxTooltipLength = 1024;
  std::u16string truncated_text =
    gfx::TruncateString(tooltip_text, kMaxTooltipLength, gfx::WORD_BREAK);
  CefString tooltip(truncated_text);
#else
  CefString tooltip(tooltip_text);
#endif // BUILDFLAG(IS_OHOS)
  CefRefPtr<ArkWebDisplayHandlerExt> handler =
      browser_impl_->GetClient()->GetDisplayHandler();
  if (handler.get()) {
    handler->OnTooltip(browser_impl_.get(), tooltip);
  }
}

gfx::Size CefRenderWidgetHostViewOSR::GetCompositorViewportPixelSize() {
#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  return gfx::ScaleToCeiledSize(GetPhysicalViewBounds().size(),
                                GetDeviceScaleFactor());
#else
  return gfx::ScaleToCeiledSize(GetRequestedRendererSize(),
                                GetDeviceScaleFactor());
#endif
}

uint32_t CefRenderWidgetHostViewOSR::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void CefRenderWidgetHostViewOSR::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (delegated_frame_host_) {
    delegated_frame_host_->CopyFromCompositingSurface(src_rect, output_size,
                                                      std::move(callback));
  }
}

display::ScreenInfos CefRenderWidgetHostViewOSR::GetNewScreenInfosForUpdate() {
  display::ScreenInfo display_screen_info;

  if (browser_impl_) {
#if BUILDFLAG(IS_OHOS)
    CefScreenInfo screen_info(kDefaultScaleFactor, 0, 0, false, CefRect(),
                              CefRect(), 0,
                              cef_screen_orientation_type_t::UNDEFINED);
#else
    CefScreenInfo screen_info(kDefaultScaleFactor, 0, 0, false, CefRect(),
                              CefRect());
#endif

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    if (!handler->GetScreenInfo(browser_impl_.get(), screen_info) ||
        screen_info.rect.width == 0 || screen_info.rect.height == 0 ||
        screen_info.available_rect.width == 0 ||
        screen_info.available_rect.height == 0) {
      // If a screen rectangle was not provided, try using the view rectangle
      // instead. Otherwise, popup views may be drawn incorrectly, or not at
      // all.
      CefRect screenRect;
      handler->GetViewRect(browser_impl_.get(), screenRect);
      CHECK_GT(screenRect.width, 0);
      CHECK_GT(screenRect.height, 0);

      if (screen_info.rect.width == 0 || screen_info.rect.height == 0) {
        screen_info.rect = screenRect;
      }

      if (screen_info.available_rect.width == 0 ||
          screen_info.available_rect.height == 0) {
        screen_info.available_rect = screenRect;
      }
    }

    display_screen_info = ScreenInfoFrom(screen_info);
  }

  return display::ScreenInfos(display_screen_info);
}

void CefRenderWidgetHostViewOSR::TransformPointToRootSurface(
    gfx::PointF* point) {}

gfx::Rect CefRenderWidgetHostViewOSR::GetBoundsInRootWindow() {
  if (!browser_impl_.get()) {
    return gfx::Rect();
  }

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);
  if (handler->GetRootScreenRect(browser_impl_.get(), rc)) {
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  }
  return GetViewBounds();
}

#if !BUILDFLAG(IS_MAC)
viz::ScopedSurfaceIdAllocator
CefRenderWidgetHostViewOSR::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}
#endif

viz::SurfaceId CefRenderWidgetHostViewOSR::GetCurrentSurfaceId() const {
  return delegated_frame_host_ ? delegated_frame_host_->GetCurrentSurfaceId()
                               : viz::SurfaceId();
}

void CefRenderWidgetHostViewOSR::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeSetComposition");
  if (!render_widget_host_) {
    return;
  }

  std::vector<ui::ImeTextSpan> web_underlines;
  web_underlines.reserve(underlines.size());
  for (const CefCompositionUnderline& line : underlines) {
    web_underlines.emplace_back(
        ui::ImeTextSpan::Type::kComposition, line.range.from, line.range.to,
        line.thick ? ui::ImeTextSpan::Thickness::kThick
                   : ui::ImeTextSpan::Thickness::kThin,
        GetImeUnderlineStyle(line.style), line.background_color, line.color,
        std::vector<std::string>());
  }
  gfx::Range range(replacement_range.from, replacement_range.to);

  // Start Monitoring for composition updates before we set.
  RequestImeCompositionUpdate(true);

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    text_input_manager_->GetActiveWidget()->ImeSetComposition(
        text, web_underlines, range, selection_range.from, selection_range.to);
  }
#else
  render_widget_host_->ImeSetComposition(
      text, web_underlines, range, selection_range.from, selection_range.to);
#endif
}

void CefRenderWidgetHostViewOSR::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeCommitText");
  if (!render_widget_host_) {
    return;
  }

  gfx::Range range(replacement_range.from, replacement_range.to);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    text_input_manager_->GetActiveWidget()->ImeCommitText(
        text, std::vector<ui::ImeTextSpan>(), range, relative_cursor_pos);
  }
#else
  render_widget_host_->ImeCommitText(text, std::vector<ui::ImeTextSpan>(),
                                     range, relative_cursor_pos);
#endif

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeFinishComposingText(bool keep_selection) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeFinishComposingText");
  if (!render_widget_host_) {
    return;
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    text_input_manager_->GetActiveWidget()->ImeFinishComposingText(
        keep_selection);
  }
#else
  render_widget_host_->ImeFinishComposingText(keep_selection);
#endif

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeCancelComposition");
  if (!render_widget_host_) {
    return;
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    text_input_manager_->GetActiveWidget()->ImeCancelComposition();
  }
#else
  render_widget_host_->ImeCancelComposition();
#endif

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::SelectionChanged(const std::u16string& text,
                                                  size_t offset,
                                                  const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (!browser_impl_.get()) {
    return;
  }

  CefString selected_text;
  if (!range.is_empty() && !text.empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n <= text.length()) {
      selected_text = text.substr(pos, n);
    }
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  CefRange cef_range(range.start(), range.end());
  handler->OnTextSelectionChanged(browser_impl_.get(), selected_text,
                                  cef_range);
}

const viz::LocalSurfaceId& CefRenderWidgetHostViewOSR::GetLocalSurfaceId()
    const {
  return const_cast<CefRenderWidgetHostViewOSR*>(this)
      ->GetOrCreateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();
  if (delegated_frame_host_) {
    delegated_frame_host_->SetIsFrameSinkIdOwner(is_frame_sink_id_owner());
  }
}

const viz::FrameSinkId& CefRenderWidgetHostViewOSR::GetFrameSinkId() const {
  return delegated_frame_host_
             ? delegated_frame_host_->frame_sink_id()
             : viz::FrameSinkIdAllocator::InvalidFrameSinkId();
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::GetRootFrameSinkId() {
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  return compositor ? compositor->frame_sink_id() : viz::FrameSinkId();
}

void CefRenderWidgetHostViewOSR::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

void CefRenderWidgetHostViewOSR::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

void CefRenderWidgetHostViewOSR::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

std::unique_ptr<content::SyntheticGestureTarget>
CefRenderWidgetHostViewOSR::CreateSyntheticGestureTarget() {
  return std::make_unique<CefSyntheticGestureTargetOSR>(host());
}

bool CefRenderWidgetHostViewOSR::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return target_view->TransformPointToLocalCoordSpace(point, GetFrameSinkId(),
                                                      transformed_point);
}

void CefRenderWidgetHostViewOSR::DidNavigate() {
  if (!IsShowing()) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    InvalidateLocalSurfaceId();
  } else {
    if (is_first_navigation_) {
      // The first navigation does not need a new LocalSurfaceID. The renderer
      // can use the ID that was already provided.
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  GetLocalSurfaceId());
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      // Fix egl resize pending problem.
      ReleaseResizeHold();
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  std::nullopt);
    }
  }
  if (delegated_frame_host_) {
    delegated_frame_host_->DidNavigate();
  }
  is_first_navigation_ = false;
}

void CefRenderWidgetHostViewOSR::OnFrameComplete(
    const viz::BeginFrameAck& ack) {
  DCHECK(begin_frame_pending_);
  DCHECK_EQ(begin_frame_source_.source_id(), ack.frame_id.source_id);
  DCHECK_EQ(begin_frame_number_, ack.frame_id.sequence_number);
  begin_frame_pending_ = false;
}

#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
void CefRenderWidgetHostViewOSR::OnRendererWidgetCreated() {
  software_compositor_ = std::make_unique<content::SoftwareCompositorHostOhos>(
      render_widget_host_);
}
#endif

void CefRenderWidgetHostViewOSR::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  auto metadata =
      host_->render_frame_metadata_provider()->LastRenderFrameMetadata();

  if (video_consumer_) {
    // Need to wait for the first frame of the new size before calling
    // SizeChanged. Otherwise, the video frame will be letterboxed.
    video_consumer_->SizeChanged(metadata.viewport_size_in_pixels);
  }

  gfx::PointF root_scroll_offset;
  if (metadata.root_scroll_offset) {
    root_scroll_offset = *metadata.root_scroll_offset;
  }
  if (root_scroll_offset != last_scroll_offset_) {
    last_scroll_offset_ = root_scroll_offset;

    if (!is_scroll_offset_changed_pending_) {
      is_scroll_offset_changed_pending_ = true;

      // Send the notification asynchronously.
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefRenderWidgetHostViewOSR::OnScrollOffsetChanged,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

#if BUILDFLAG(IS_ARKWEB)
  gfx::SizeF root_layer_size = metadata.root_layer_size;
  if (root_layer_size != root_layer_size_) {
    root_layer_size_ = root_layer_size;

    // Send the notification asynchronously.
#ifdef DISABLE_GPU
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefRenderWidgetHostViewOSR::OnRootLayerChanged,
                                weak_ptr_factory_.GetWeakPtr()));
#else
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefRenderWidgetHostViewOSR::OnRootLayerChanged,
                                weak_ptr_factory_.GetWeakPtr()));
#endif
  }
  bool size_changed = false;
#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
  gfx::SizeF scrollable_viewport_size = metadata.scrollable_viewport_size;
  if (scrollable_viewport_size != scrollable_viewport_size_) {
    scrollable_viewport_size_ = scrollable_viewport_size;
    size_changed = true;
  }
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (size_changed && needFocusViewport_ > 0) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    if (handler) {
      needFocusViewport_--;
      handler->OnResizeScrollableViewport(browser_impl_->GetBrowser());
    }
  }
#endif

  gfx::Size viewport_size_in_pixels = metadata.viewport_size_in_pixels;
  float device_scale_factor = metadata.device_scale_factor;
  if (viewport_size_in_pixels != viewport_size_in_pixels_ ||
      device_scale_factor != device_scale_factor_) {
    TRACE_EVENT2("cef",
                 "CefRenderWidgetHostViewOSR::"
                 "OnRenderFrameMetadataChangedAfterActivation",
                 "viewport_size_in_pixels", viewport_size_in_pixels.ToString(),
                 "device_scale_factor", device_scale_factor);
    viewport_size_in_pixels_ = viewport_size_in_pixels;
    device_scale_factor_ = device_scale_factor;
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRenderWidgetHostViewOSR::ReleaseResizeHold,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

  if (!page_scale_factor_) {
    // set init page scale factor.
    page_scale_factor_ = metadata.page_scale_factor;
#if BUILDFLAG(ARKWEB_SCROLLBAR)
    if (browser_impl_.get()) {
      CefRefPtr<ArkWebDisplayHandlerExt> handler =
          browser_impl_->client()->GetDisplayHandler();
      CHECK(handler);
      handler->OnScaleInited(
          browser_impl_.get(),
          std::round(page_scale_factor_ * SCALE_FACTOR_CONVERT_RATIO));
    }
#endif
    return;
  }
  float new_page_scale_factor = metadata.page_scale_factor;
  if (new_page_scale_factor != page_scale_factor_) {
    float old_page_scale_factor = page_scale_factor_;
    page_scale_factor_ = new_page_scale_factor;
    // Send the notification asynchronously.
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRenderWidgetHostViewOSR::OnScaleChanged,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 old_page_scale_factor, new_page_scale_factor));
  }
#endif

  if (metadata.selection.start != selection_start_ ||
      metadata.selection.end != selection_end_) {
    selection_start_ = metadata.selection.start;
    selection_end_ = metadata.selection.end;
    selection_controller_client_->UpdateClientSelectionBounds(selection_start_,
                                                              selection_end_);
  }
}

std::unique_ptr<viz::HostDisplayClient>
CefRenderWidgetHostViewOSR::CreateHostDisplayClient() {
  host_display_client_ =
      new CefHostDisplayClientOSR(this, gfx::kNullAcceleratedWidget);
  host_display_client_->SetActive(true);
  return base::WrapUnique(host_display_client_.get());
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
#if BUILDFLAG(ARKWEB_BACKGROUND_COLOR)
  if (SkColorGetA(background_color_) != SK_AlphaOPAQUE) {
#else
  if (background_color_ == SK_ColorTRANSPARENT) {
#endif  // BUILDFLAG(ARKWEB_BACKGROUND_COLOR)
    SetBackgroundColor(background_color_);
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_));
    if (compositor_) {
      compositor_->SetBackgroundColor(background_color_);
    }
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::WasResized() {
  // Only one resize will be in-flight at a time.
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  TRACE_EVENT2("base", "CefRenderWidgetHostViewOSR::WasResized",
               "hold_resize_", hold_resize_,
               "pending_resize_", pending_resize_);
#endif
  if (hold_resize_) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    isKeyboardResized_ = false;
#endif
    if (!pending_resize_) {
      pending_resize_ = true;
    }
    return;
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                              std::nullopt);
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void CefRenderWidgetHostViewOSR::WasKeyboardResized() {
  // Only one resize will be in-flight at a time.
  if (hold_resize_) {
    isKeyboardResized_ = true;
    if (!pending_resize_) {
      pending_resize_ = true;
    }
    return;
  }

  bool isKeyboardResized = true;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                              absl::nullopt, isKeyboardResized);
}
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void CefRenderWidgetHostViewOSR::SetShouldFrameSubmissionBeforeDraw(
    bool should) {
  TRACE_EVENT0(
      "base", "CefRenderWidgetHostViewOSR::SetShouldFrameSubmissionBeforeDraw");
  should_wait_ = should;
}
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

void CefRenderWidgetHostViewOSR::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id,
    bool isKeyboard) {
#else
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id) {
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  SetFrameRate();
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  bool visible_changed = false;
  bool resized = ResizeRootLayer(isKeyboard, visible_changed);
  if (!resized) {
    resized = visible_changed;
  }
#else
  const bool resized = ResizeRootLayer();
#endif

#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
  if (resized) {
    OHOS::NWeb::ResSchedClientAdapter::ReportScene(
        OHOS::NWeb::ResSchedStatusAdapter::WEB_SCENE_ENTER,
        OHOS::NWeb::ResSchedSceneAdapter::RESIZE);
  }
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  if (resized && should_wait_) {
    if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
            browser_impl_->GetAcceleratedWidget(is_popup_))) {
      compositor->SetShouldFrameSubmissionBeforeDraw(true);
    }
  }
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  bool surface_id_updated = false;

  if (!resized && child_local_surface_id) {
    // Update the current surface ID.
    parent_local_surface_id_allocator_->UpdateFromChild(
        *child_local_surface_id);
    surface_id_updated = true;
  }

  // Allocate a new surface ID if the surface has been resized or if the current
  // ID is invalid (meaning we may have been evicted).
  if (resized || !GetCurrentLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    surface_id_updated = true;
  }

  if (surface_id_updated) {
#if BUILDFLAG(ARKWEB_EX_TOPCONTROLS)
    delegated_frame_host_->EmbedSurface(GetCurrentLocalSurfaceId(),
                                        GetPhysicalViewBounds().size(),
                                        deadline_policy);
#else
    delegated_frame_host_->EmbedSurface(
        GetCurrentLocalSurfaceId(), GetViewBounds().size(), deadline_policy);
#endif
    // |render_widget_host_| will retrieve resize parameters from the
    // DelegatedFrameHost and this view, so SynchronizeVisualProperties must be
    // called last.
    if (render_widget_host_) {
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
      if (isKeyboard) {
        needFocusViewport_++;
      }
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
      render_widget_host_->SynchronizeVisualProperties();
    }
  }
}

void CefRenderWidgetHostViewOSR::OnScreenInfoChanged() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnScreenInfoChanged");
  InvalidateLocalSurfaceId();
  if (!render_widget_host_) {
    return;
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              std::nullopt);

  if (render_widget_host_->delegate()) {
    render_widget_host_->delegate()->SendScreenRects();
  } else {
    render_widget_host_->SendScreenRects();
  }

  render_widget_host_->NotifyScreenInfoChanged();

  // We might want to change the cursor scale factor here as well - see the
  // cache for the current_cursor_, as passed by UpdateCursor from the
  // renderer in the rwhv_aura (current_cursor_.SetScaleFactor)

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    guest_host_view->OnScreenInfoChanged();
  }
}

void CefRenderWidgetHostViewOSR::Invalidate(
    CefBrowserHost::PaintElementType type) {
  TRACE_EVENT1("cef", "CefRenderWidgetHostViewOSR::Invalidate", "type", type);
  if (!IsPopupWidget() && type == PET_POPUP) {
    if (popup_host_view_) {
      popup_host_view_->Invalidate(type);
    }
    return;
  }
  InvalidateInternal(gfx::Rect(SizeInPixels()));
}

void CefRenderWidgetHostViewOSR::SendExternalBeginFrame() {
  DCHECK(external_begin_frame_enabled_);

  if (begin_frame_pending_) {
    return;
  }
  begin_frame_pending_ = true;

  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeTicks deadline = base::TimeTicks();
  base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, begin_frame_source_.source_id(),
      ++begin_frame_number_, frame_time, deadline, interval,
      viz::BeginFrameArgs::NORMAL);

  DCHECK(begin_frame_args.IsValid());

  if (render_widget_host_) {
    render_widget_host_->ProgressFlingIfNeeded(frame_time);
  }

#ifdef DISABLE_GPU
  if (compositor_) {
    compositor_->IssueExternalBeginFrame(
#else
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (compositor) {
    compositor->IssueExternalBeginFrame(
#endif
        begin_frame_args, /* force= */ true,
        base::BindOnce(&CefRenderWidgetHostViewOSR::OnFrameComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    begin_frame_pending_ = false;
  }

  if (!IsPopupWidget() && popup_host_view_) {
    popup_host_view_->SendExternalBeginFrame();
  }
}

void CefRenderWidgetHostViewOSR::SendKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    OHOS::NWeb::ResSchedClientAdapter::ReportScene(
        OHOS::NWeb::ResSchedStatusAdapter::WEB_SCENE_ENTER,
        OHOS::NWeb::ResSchedSceneAdapter::KEYBOARD_CLICK);
  }
#endif
  content::RenderWidgetHostImpl* target_host = render_widget_host_;

#if !BUILDFLAG(ARKWEB_CLIPBOARD)
  if (selection_controller_client_) {
    selection_controller_client_->CloseQuickMenuAndHideHandles();
  }
#endif  // #if !BUILDFLAG(ARKWEB_CLIPBOARD)

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (render_widget_host_ && render_widget_host_->delegate()) {
    target_host = render_widget_host_->delegate()->GetFocusedRenderWidgetHost(
        render_widget_host_);
  }

  if (target_host && target_host->GetView()) {
    // Direct routing requires that events go directly to the View.
    target_host->ForwardKeyboardEventWithLatencyInfo(event, ui::LatencyInfo());
  }
}

void CefRenderWidgetHostViewOSR::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseEvent");
  if (!IsPopupWidget()) {
    if (browser_impl_ &&
        event.GetType() == blink::WebMouseEvent::Type::kMouseDown &&
        event.button != blink::WebPointerProperties::Button::kRight) {
      browser_impl_->CancelContextMenu();
    }
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    if (selection_controller_client_ &&
        event.GetType() == blink::WebMouseEvent::Type::kMouseDown) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }
#else
    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }
#endif
    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              event.PositionInWidget().x(), event.PositionInWidget().y())) {
        blink::WebMouseEvent popup_event(event);
        popup_event.SetPositionInWidget(
            event.PositionInWidget().x() -
                popup_host_view_->popup_position_.x(),
            event.PositionInWidget().y() -
                popup_host_view_->popup_position_.y());
        popup_event.SetPositionInScreen(popup_event.PositionInWidget().x(),
                                        popup_event.PositionInWidget().y());

        popup_host_view_->SendMouseEvent(popup_event);
        return;
      }
    } else if (!guest_host_views_.empty()) {
      for (auto guest_host_view : guest_host_views_) {
        if (!guest_host_view->render_widget_host_ ||
            !guest_host_view->render_widget_host_->GetView()) {
          continue;
        }
        const gfx::Rect& guest_bounds =
            guest_host_view->render_widget_host_->GetView()->GetViewBounds();
        if (guest_bounds.Contains(event.PositionInWidget().x(),
                                  event.PositionInWidget().y())) {
          blink::WebMouseEvent guest_event(event);
          guest_event.SetPositionInWidget(
              event.PositionInWidget().x() - guest_bounds.x(),
              event.PositionInWidget().y() - guest_bounds.y());
          guest_event.SetPositionInScreen(guest_event.PositionInWidget().x(),
                                          guest_event.PositionInWidget().y());

          guest_host_view->SendMouseEvent(guest_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    if (ShouldRouteEvents()) {
      // RouteMouseEvent wants non-const pointer to WebMouseEvent, but it only
      // forwards it to RenderWidgetTargeter::FindTargetAndDispatch as a const
      // reference, so const_cast here is safe.
      render_widget_host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
          this, const_cast<blink::WebMouseEvent*>(&event), ui::LatencyInfo());
    } else {
      render_widget_host_->GetView()->ProcessMouseEvent(event,
                                                        ui::LatencyInfo());
    }
  }
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  else {
    LOG(ERROR) << "SendMouseEvent event dropped because render_widget_host "
               << !!render_widget_host_;
  }
#endif
}

void CefRenderWidgetHostViewOSR::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");

  if (!IsPopupWidget()) {
    if (browser_impl_) {
      browser_impl_->CancelContextMenu();
    }
#ifndef ARKWEB_CLIPBOARD
    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }
#endif
    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              event.PositionInWidget().x(), event.PositionInWidget().y())) {
        blink::WebMouseWheelEvent popup_mouse_wheel_event(event);
        popup_mouse_wheel_event.SetPositionInWidget(
            event.PositionInWidget().x() -
                popup_host_view_->popup_position_.x(),
            event.PositionInWidget().y() -
                popup_host_view_->popup_position_.y());
        popup_mouse_wheel_event.SetPositionInScreen(
            popup_mouse_wheel_event.PositionInWidget().x(),
            popup_mouse_wheel_event.PositionInWidget().y());

        popup_host_view_->SendMouseWheelEvent(popup_mouse_wheel_event);
        return;
      } else {
        // Scrolling outside of the popup widget so destroy it.
        // Execute asynchronously to avoid deleting the widget from inside
        // some other callback.
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefRenderWidgetHostViewOSR::CancelWidget,
                           popup_host_view_->weak_ptr_factory_.GetWeakPtr()));
      }
    } else if (!guest_host_views_.empty()) {
      for (auto guest_host_view : guest_host_views_) {
        if (!guest_host_view->render_widget_host_ ||
            !guest_host_view->render_widget_host_->GetView()) {
          continue;
        }
        const gfx::Rect& guest_bounds =
            guest_host_view->render_widget_host_->GetView()->GetViewBounds();
        if (guest_bounds.Contains(event.PositionInWidget().x(),
                                  event.PositionInWidget().y())) {
          blink::WebMouseWheelEvent guest_mouse_wheel_event(event);
          guest_mouse_wheel_event.SetPositionInWidget(
              event.PositionInWidget().x() - guest_bounds.x(),
              event.PositionInWidget().y() - guest_bounds.y());
          guest_mouse_wheel_event.SetPositionInScreen(
              guest_mouse_wheel_event.PositionInWidget().x(),
              guest_mouse_wheel_event.PositionInWidget().y());

          guest_host_view->SendMouseWheelEvent(guest_mouse_wheel_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    blink::WebMouseWheelEvent mouse_wheel_event(event);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    bool shouldRoute = ShouldRouteEvents();
    mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(
        shouldRoute);
    mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
        mouse_wheel_event, shouldRoute);
#else
    mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(false);
    mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
        mouse_wheel_event, false);
#endif
    if (ShouldRouteEvents()) {
      render_widget_host_->delegate()
          ->GetInputEventRouter()
          ->RouteMouseWheelEvent(
              this, const_cast<blink::WebMouseWheelEvent*>(&mouse_wheel_event),
              ui::LatencyInfo());
    } else {
      render_widget_host_->GetView()->ProcessMouseWheelEvent(mouse_wheel_event,
                                                             ui::LatencyInfo());
    }
  }
}

void CefRenderWidgetHostViewOSR::SendTouchGestureEvent(
    blink::WebTouchEvent& touch_event) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchGestureEvent");
#endif
  ui::LatencyInfo latency_info = CreateLatencyInfo(touch_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    render_widget_host_->GetRenderInputRouter()
        ->ForwardTouchEventWithLatencyInfo(touch_event, latency_info);
  }
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void CefRenderWidgetHostViewOSR::SendGestureEvent(
    const ui::GestureEventData& gesture) {
#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_) {
    overscroll_controller_->Enable();
  }
#endif

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  // without this check, forwarding gestures does not work!
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

#if BUILDFLAG(ARKWEB_MENU)
  // We let the touch selection controller see gesture events here, since they
  // may be routed and not make it to FilterInputEvent().
  if (selection_controller_ &&
      web_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    is_tap_down_in_cursor_update_ = false;
    switch (web_event.GetType()) {
      case blink::WebInputEvent::Type::kGestureLongPress:
        selection_controller_->HandleLongPressEvent(
            web_event.TimeStamp(), web_event.PositionInWidget());
        break;
      case blink::WebInputEvent::Type::kGestureScrollBegin:
        selection_controller_->OnScrollBeginEvent();
        break;
      case blink::WebInputEvent::Type::kGestureTapDown:
        is_tap_down_in_cursor_update_ = true;
        break;
      case blink::WebInputEvent::Type::kGestureShowPress:
        is_tap_down_in_cursor_update_ = true;
        break;
      case blink::WebInputEvent::Type::kGestureTap:
        is_tap_down_in_cursor_update_ = true;
        break;
      default:
        break;
    }
  }
#endif

  ui::LatencyInfo latency_info = CreateLatencyInfo(web_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &web_event, latency_info);
  } else {
    render_widget_host_->GetRenderInputRouter()
        ->ForwardGestureEventWithLatencyInfo(web_event, latency_info);
  }
}
#endif

void CefRenderWidgetHostViewOSR::SendTouchEvent(const CefTouchEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchEvent");

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
  if (event.type == CEF_TET_PRESSED) {
    is_editable_node_ = false;
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_));
    if (compositor) {
      compositor->SetCurrentFrameSinkId(GetFrameSinkId());
    } else {
      LOG(ERROR) << "compositor is null when send touch event";
    }
  }
#endif

  if (!IsPopupWidget() && popup_host_view_) {
    if (!forward_touch_to_popup_ && event.type == CEF_TET_PRESSED &&
        pointer_state_.GetPointerCount() == 0) {
      forward_touch_to_popup_ =
          popup_host_view_->popup_position_.Contains(event.x, event.y);
    }

    if (forward_touch_to_popup_) {
      CefTouchEvent popup_event(event);
      popup_event.x -= popup_host_view_->popup_position_.x();
      popup_event.y -= popup_host_view_->popup_position_.y();
      popup_host_view_->SendTouchEvent(popup_event);
      return;
    }
  }

  // Update the touch event first.
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  pointer_state_.SetFromOverlay(event.from_overlay);
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
  if (!pointer_state_.OnTouch(event)) {
    return;
  }

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
  OnTouchDown();
#endif

  if (selection_controller_->WillHandleTouchEvent(pointer_state_)) {
    pointer_state_.CleanupRemovedTouchPoints(event);
    return;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(pointer_state_);

  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, result.moved_beyond_slop_region, false
#if BUILDFLAG(ARKWEB_FIT_CONTENT)
      ,
      is_fit_content_
#endif
  );

  pointer_state_.CleanupRemovedTouchPoints(event);

  // Set unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  if (!result.succeeded) {
    pointer_state_.MarkUnchangedTouchPointsAsStationary(&touch_event, event);
  }

  if (!render_widget_host_) {
    return;
  }

  ui::LatencyInfo latency_info = CreateLatencyInfo(touch_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    render_widget_host_->GetRenderInputRouter()
        ->ForwardTouchEventWithLatencyInfo(touch_event, latency_info);
  }

  bool touch_end =
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}

bool CefRenderWidgetHostViewOSR::ShouldRouteEvents() const {
  if (!render_widget_host_->delegate()) {
    return false;
  }

  // Do not route events that are currently targeted to page popups such as
  // <select> element drop-downs, since these cannot contain cross-process
  // frames.
  if (!render_widget_host_->delegate()->IsWidgetForPrimaryMainFrame(
          render_widget_host_)) {
    return false;
  }

  return !!render_widget_host_->delegate()->GetInputEventRouter();
}

void CefRenderWidgetHostViewOSR::SetFocus(bool focus) {
  if (!render_widget_host_) {
    return;
  }

#if BUILDFLAG(ARKWEB_FOCUS)
  if (HasFocus() != focus) {
    LOG(INFO) << "CefRenderWidgetHostViewOSR::SetFocus:" << focus;
  }
#endif  // #if BUILDFLAG(ARKWEB_FOCUS)
  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (focus) {
    widget->GotFocus();
    widget->SetActive(true);
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    if (selection_controller_client_) {
      selection_controller_client_->SetTemporarilyHidden(false);
    }
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)
#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
    OnFocusInternal();
#endif
  } else {
#if !BUILDFLAG(ARKWEB_BUGFIX_CRASH)
    if (browser_impl_.get()) {
      browser_impl_->CancelContextMenu();
    }
#endif
    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }

    widget->SetActive(false);
    widget->LostFocus();
#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
    LostFocusInternal();
#endif
  }
}

void CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled(
    content::TextInputManager* text_input_manager,
    content::RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled ";
  ArkWebRenderHandlerExt::TextInputMode mode = CEF_TEXT_INPUT_MODE_NONE;
  ArkWebRenderHandlerExt::TextInputType type = CEF_TEXT_INPUT_TYPE_NONE;
  ArkWebRenderHandlerExt::TextInputAction action =
      CEF_TEXT_INPUT_ACTION_DEFAULT;
  ArkWebRenderHandlerExt::TextInputFlags flags = CEF_TEXT_INPUT_FLAG_NONE;
  bool show_keyboard = false;
  std::map<CefString, CefString> text_input_attributes;
  const auto state = text_input_manager->GetTextInputState();
  if (state) {
    for (const auto& atrribute : state->input_element_attributes) {
      text_input_attributes.insert(std::make_pair<CefString, CefString>(
          CefString(atrribute.first), CefString(atrribute.second)));
    }
  }

  CefRefPtr<ArkWebRenderHandlerExt> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);
#if BUILDFLAG(ARKWEB_SCROLLBAR)
  if (AsArkWebRenderWidgetHostViewOSRExt()->UpdateEditBounds()) {
    auto processedOffset =
        AsArkWebRenderWidgetHostViewOSRExt()->HandleCursorOffset();
    handler->OnCursorUpdate(
        browser_impl_->GetBrowser(),
        CefRect(processedOffset.first, processedOffset.second,
                AsArkWebRenderWidgetHostViewOSRExt()->focus_rect_width_,
                AsArkWebRenderWidgetHostViewOSRExt()->focus_rect_height_));
  }
#endif
  bool is_need_reset_ime_listener = false;
  if (state) {
    int32_t current_node_id = state->node_id;
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: node->ID " << state->node_id
               << ", node_id_ " << node_id_;
    if (current_node_id != node_id_) {
      is_need_reset_ime_listener = true;
      node_id_ = current_node_id;
    }
  }
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    static_assert(
        static_cast<int>(CEF_TEXT_INPUT_MODE_MAX) ==
            static_cast<int>(ui::TEXT_INPUT_MODE_MAX),
        "Enum values in cef_text_input_mode_t must match ui::TextInputMode");
    mode = static_cast<ArkWebRenderHandlerExt::TextInputMode>(state->mode);
    type =
        state->flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD
            ? CEF_TEXT_INPUT_TYPE_PASSWORD
            : static_cast<ArkWebRenderHandlerExt::TextInputType>(state->type);
    action =
        static_cast<ArkWebRenderHandlerExt::TextInputAction>(state->action);
    flags = static_cast<ArkWebRenderHandlerExt::TextInputFlags>(state->flags);
    show_keyboard = state->show_ime_if_needed;
  }
  if (state && !state->show_ime_if_needed && did_update_state) {
    LOG(INFO) << "Autofocus requires a keyboard to be bound, "
                 "but there is no need to pull up the keyboard";
    // TODO(OHOS) Specific implementation needs to be completed
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested "
                  "update state";
    ArkWebRenderHandlerExt::TextInputInfo input_info = {
        .node_id = state->node_id,
        .show_keyboard = state->show_ime_if_needed,
        .input_mode = mode,
        .input_type = type,
        .input_action = action,
        .input_flags = flags,
        .always_hide_ime = state->always_hide_ime};
    handler->OnVirtualKeyboardRequestedEx(
        browser_impl_->GetBrowser(), input_info, is_need_reset_ime_listener,
        text_input_attributes);
    UpdateTextInputState(state);
    return;
  }
  if (!show_keyboard) {
    last_key_code_ = -1;
  }

  if (state && state->show_ime_if_needed) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested "
                  "show_ime_if_needed";
    // TODO(OHOS) Specific implementation needs to be completed
    ArkWebRenderHandlerExt::TextInputInfo input_info = {
        .node_id = state->node_id,
        .show_keyboard = show_keyboard,
        .input_mode = mode,
        .input_type = type,
        .input_action = action,
        .input_flags = flags,
        .always_hide_ime = state->always_hide_ime};
    handler->OnVirtualKeyboardRequestedEx(
        browser_impl_->GetBrowser(), input_info, is_need_reset_ime_listener,
        text_input_attributes);
  } else if (!state || mode == CEF_TEXT_INPUT_MODE_NONE) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested not "
                  "editable";
    ArkWebRenderHandlerExt::TextInputInfo input_info;
    handler->OnVirtualKeyboardRequestedEx(
        browser_impl_->GetBrowser(), input_info, is_need_reset_ime_listener,
        text_input_attributes);
  }
  UpdateTextInputState(state);
#else
  const auto state = text_input_manager->GetTextInputState();
  if (state && !state->show_ime_if_needed) {
    return;
  }

  CefRenderHandler::TextInputMode mode = CEF_TEXT_INPUT_MODE_NONE;
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    static_assert(
        static_cast<int>(CEF_TEXT_INPUT_MODE_MAX) ==
            static_cast<int>(ui::TEXT_INPUT_MODE_MAX),
        "Enum values in cef_text_input_mode_t must match ui::TextInputMode");
    mode = static_cast<CefRenderHandler::TextInputMode>(state->mode);
  }

  CefRefPtr<ArkWebRenderHandlerExt> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

#if BUILDFLAG(IS_OHOS)
  handler->OnVirtualKeyboardRequestedEx(browser_impl_->GetBrowser(),
                                        CEF_TEXT_INPUT_MODE_NONE,
                                        CEF_TEXT_INPUT_TYPE_NONE, false);
#else
  handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), mode);
#endif  // BUILDFLAG(IS_OHOS)
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
}

void CefRenderWidgetHostViewOSR::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(touch.event.unique_touch_event_id,
                                    event_consumed, false);
}

void CefRenderWidgetHostViewOSR::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::EventType::kGesturePinchBegin ||
       gesture.type() == ui::EventType::kGesturePinchUpdate ||
       gesture.type() == ui::EventType::kGesturePinchEnd) &&
      !pinch_zoom_enabled_) {
    return;
  }

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  // without this check, forwarding gestures does not work!
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

  ui::LatencyInfo latency_info = CreateLatencyInfo(web_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &web_event, latency_info);
  } else {
    render_widget_host_->GetRenderInputRouter()
        ->ForwardGestureEventWithLatencyInfo(web_event, latency_info);
  }
}

#if BUILDFLAG(ARKWEB_ZOOM)
bool CefRenderWidgetHostViewOSR::RequiresDoubleTapGestureEvents() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDoubleTapSupportForPlatformEnabled);
}
#endif

void CefRenderWidgetHostViewOSR::UpdateFrameRate() {
  frame_rate_threshold_us_ = 0;
  SetFrameRate();

  if (video_consumer_) {
    video_consumer_->SetFrameRate(base::Microseconds(frame_rate_threshold_us_));
  }

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    guest_host_view->UpdateFrameRate();
  }
}

gfx::Size CefRenderWidgetHostViewOSR::SizeInPixels() {
  return gfx::ScaleToCeiledSize(GetViewBounds().size(), GetDeviceScaleFactor());
}

#if BUILDFLAG(IS_MAC)
void CefRenderWidgetHostViewOSR::SetActive(bool active) {}

void CefRenderWidgetHostViewOSR::ShowDefinitionForSelection() {}

void CefRenderWidgetHostViewOSR::SpeakSelection() {}

void CefRenderWidgetHostViewOSR::SetWindowFrameInScreen(const gfx::Rect& rect) {
}

void CefRenderWidgetHostViewOSR::ShowSharePicker(
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    blink::mojom::ShareService::ShareCallback callback) {
  std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
}

uint64_t CefRenderWidgetHostViewOSR::GetNSViewId() const {
  // TODO(osr): Fix all calling code paths and convert to DCHECK.
  NOTIMPLEMENTED();
  return 0;
}
#endif  // BUILDFLAG(IS_MAC)

void CefRenderWidgetHostViewOSR::OnPaint(const gfx::Rect& damage_rect,
                                         const gfx::Size& pixel_size,
                                         const void* pixels) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnPaint");

  // Workaround for issue #2817.
  if (!is_showing_) {
    return;
  }

  if (!pixels) {
    return;
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  if (!browser_impl_ || !browser_impl_->client()) {
    LOG(ERROR) << "get client failed.";
    return;
  }
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);

  gfx::Rect rect_in_pixels(0, 0, pixel_size.width(), pixel_size.height());
  rect_in_pixels.Intersect(damage_rect);

  CefRenderHandler::RectList rcList;
  rcList.emplace_back(rect_in_pixels.x(), rect_in_pixels.y(),
                      rect_in_pixels.width(), rect_in_pixels.height());

  handler->OnPaint(browser_impl_.get(), IsPopupWidget() ? PET_POPUP : PET_VIEW,
                   rcList, pixels, pixel_size.width(), pixel_size.height());

  // Release the resize hold when we reach the desired size.
  if (hold_resize_) {
    DCHECK_GT(cached_scale_factor_, 0);
#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
    gfx::Size expected_size = gfx::ScaleToCeiledSize(
        GetPhysicalViewBounds().size(), cached_scale_factor_);
#else
    gfx::Size expected_size =
        gfx::ScaleToCeiledSize(GetViewBounds().size(), cached_scale_factor_);
#endif
    if (pixel_size == expected_size) {
      ReleaseResizeHold();
    }
  }
}

void CefRenderWidgetHostViewOSR::OnAcceleratedPaint(
    const gfx::Rect& damage_rect,
    const gfx::Size& pixel_size,
    const CefAcceleratedPaintInfo& info) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnAcceleratedPaint");

  // Workaround for https://github.com/chromiumembedded/cef/issues/2817
  if (!is_showing_) {
    return;
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);

  gfx::Rect rect_in_pixels(0, 0, pixel_size.width(), pixel_size.height());
  rect_in_pixels.Intersect(damage_rect);

  CefRenderHandler::RectList rcList;
  rcList.emplace_back(rect_in_pixels.x(), rect_in_pixels.y(),
                      rect_in_pixels.width(), rect_in_pixels.height());

  handler->OnAcceleratedPaint(browser_impl_.get(),
                              IsPopupWidget() ? PET_POPUP : PET_VIEW, rcList,
                              info);

  // Release the resize hold when we reach the desired size.
  if (hold_resize_) {
    DCHECK_GT(cached_scale_factor_, 0);
    gfx::Size expected_size =
        gfx::ScaleToCeiledSize(GetViewBounds().size(), cached_scale_factor_);
    if (pixel_size == expected_size) {
      ReleaseResizeHold();
    }
  }
}

ui::Layer* CefRenderWidgetHostViewOSR::GetRootLayer() const {
  return root_layer_.get();
}

ui::TextInputType CefRenderWidgetHostViewOSR::GetTextInputType() {
  if (text_input_manager_ && text_input_manager_->GetTextInputState()) {
    return text_input_manager_->GetTextInputState()->type;
  }

  return ui::TEXT_INPUT_TYPE_NONE;
}

void CefRenderWidgetHostViewOSR::SetFrameRate() {
  CefRefPtr<AlloyBrowserHostImpl> browser;
  if (parent_host_view_) {
    // Use the same frame rate as the embedding browser.
    browser = parent_host_view_->browser_impl_;
  } else {
    browser = browser_impl_;
  }
  CHECK(browser);

  // Only set the frame rate one time.
  if (frame_rate_threshold_us_ != 0) {
    return;
  }

  int frame_rate =
      osr_util::ClampFrameRate(browser->settings().windowless_frame_rate);

  frame_rate_threshold_us_ = 1000000 / frame_rate;

#ifdef DISABLE_GPU
  if (compositor_) {
    compositor_->SetDisplayVSyncParameters(
        base::TimeTicks::Now(), base::Microseconds(frame_rate_threshold_us_));
#else
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (compositor) {
    compositor->SetDisplayVSyncParameters(
        base::TimeTicks::Now(), base::Microseconds(frame_rate_threshold_us_));
#endif
  }

  if (video_consumer_) {
    video_consumer_->SetFrameRate(base::Microseconds(frame_rate_threshold_us_));
  }
}

bool CefRenderWidgetHostViewOSR::SetScreenInfo() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  display::ScreenInfo current_info = screen_infos_.current();

  // This will result in a call to GetNewScreenInfosForUpdate().
  UpdateScreenInfo();
  if (screen_infos_.current() == current_info) {
    // Nothing changed.
    return false;
  }

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    content::RenderWidgetHostImpl* rwhi = guest_host_view->render_widget_host();
    if (!rwhi) {
      continue;
    }
    auto guest_view_osr =
        static_cast<CefRenderWidgetHostViewOSR*>(rwhi->GetView());
    if (guest_view_osr) {
      guest_view_osr->SetScreenInfo();
    }
  }

  return true;
}

bool CefRenderWidgetHostViewOSR::SetViewBounds() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  // Popup bounds are set in InitAsPopup.
  if (IsPopupWidget()) {
    return false;
  }

  const gfx::Rect& new_bounds = ::GetViewBounds(browser_impl_.get());
  if (new_bounds == current_view_bounds_) {
    return false;
  }

  current_view_bounds_ = new_bounds;
  return true;
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
gfx::Size GetVisibleViewportSize(AlloyBrowserHostImpl* browser) {
  if (!browser) {
    return gfx::Size(0, 0);
  }

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->GetVisibleViewportRect(browser, rc);
  CHECK_GT(rc.width, 0);
  CHECK_GT(rc.height, 0);

  return gfx::Size(rc.width, rc.height);
}

bool CefRenderWidgetHostViewOSR::SetVisibleViewportSize() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  // Popup bounds are set in InitAsPopup.
  if (IsPopupWidget()) {
    return false;
  }
  const gfx::Size& visible_view_bounds =
      ::GetVisibleViewportSize(browser_impl_.get());
  if (visible_view_bounds == current_visible_view_bounds_) {
    return false;
  }

  current_visible_view_bounds_ = visible_view_bounds;
  return true;
}

gfx::Size CefRenderWidgetHostViewOSR::GetPhysicalVisibleViewportSize() {
  if (current_visible_view_bounds_.width() == 0 &&
      current_visible_view_bounds_.height() == 0) {
#if BUILDFLAG(ARKWEB_EX_TOPCONTROLS)
    return GetPhysicalViewBounds().size();
#else
    return GetViewBounds().size();
#endif
  }

  if (IsPopupWidget()) {
    return popup_position_.size();
  }
  return current_visible_view_bounds_;
}

gfx::Size CefRenderWidgetHostViewOSR::GetVisibleViewportSize() {
  gfx::Size bounds = GetPhysicalVisibleViewportSize();
#if BUILDFLAG(ARKWEB_EX_TOPCONTROLS)
  bounds.set_height(bounds.height() - GetShrinkViewportHeight());
#endif
  return bounds;
}
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
bool CefRenderWidgetHostViewOSR::SetRootLayerSize(bool force,
                                                  bool* visible_changed) {
#else
bool CefRenderWidgetHostViewOSR::SetRootLayerSize(bool force) {
#endif
  const bool screen_info_changed = SetScreenInfo();
  const bool view_bounds_changed = SetViewBounds();
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  const bool visible_view_bounds_changed = SetVisibleViewportSize();
  if (visible_changed) {
    *visible_changed = visible_view_bounds_changed;
  }
#endif
  if (!force && !screen_info_changed && !view_bounds_changed
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      && !visible_view_bounds_changed
#endif
  ) {
    return false;
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  GetRootLayer()->SetBounds(gfx::Rect(GetPhysicalViewBounds().size()));
#else
  GetRootLayer()->SetBounds(gfx::Rect(GetViewBounds().size()));
#endif

#ifdef DISABLE_GPU
  if (compositor_) {
    compositor_local_surface_id_allocator_.GenerateId();
    compositor_->SetScaleAndSize(
#else
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (compositor) {
    compositor_local_surface_id_allocator_.GenerateId();
    compositor->SetScaleAndSize(
#endif
        GetDeviceScaleFactor(), SizeInPixels(),
        compositor_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  // We only need to notify the screen information change, the visual window
  // adjustment is done by CefRenderWidgetHostViewOSR::WasResized.
  return view_bounds_changed;
#else
  return (screen_info_changed || view_bounds_changed);
#endif

#if BUILDFLAG(ARKWEB_EXT_PULL_TO_REFRESH)
  if (overscroll_controller_) {
    overscroll_controller_->Disable();
  }
#endif
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
bool CefRenderWidgetHostViewOSR::ResizeRootLayer(bool isKeyboard,
                                                 bool& visible_changed) {
#else
bool CefRenderWidgetHostViewOSR::ResizeRootLayer() {
#endif
  if (!hold_resize_) {
    bool reseize = SetRootLayerSize(false /* force */, &visible_changed);
    // The resize hold is not currently active.
    if (reseize) {
      // The size has changed. Avoid resizing again until ReleaseResizeHold() is
      // called.
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
      setReleaseResizeHoldDelayedTask_.Reset(
          base::BindOnce(&CefRenderWidgetHostViewOSR::ReleaseResizeHold,
                         weak_ptr_factory_.GetWeakPtr()));
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE, setReleaseResizeHoldDelayedTask_.callback(),
          base::Milliseconds(400));
      TRACE_EVENT0(
          "base",
          "CefRenderWidgetHostViewOSR::ResizeRootLayer, trigger delay task.");
#endif
      hold_resize_ = true;
      cached_scale_factor_ = GetDeviceScaleFactor();
      return true;
    }
  } else if (!pending_resize_) {
    // The resize hold is currently active. Another resize will be triggered
    // from ReleaseResizeHold().
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    isKeyboardResized_ = isKeyboard;
#endif
    pending_resize_ = true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::ReleaseResizeHold() {
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  setReleaseResizeHoldDelayedTask_.Cancel();
#endif
  DCHECK(hold_resize_);
  hold_resize_ = false;
  cached_scale_factor_ = -1;
  if (pending_resize_) {
    pending_resize_ = false;
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
    if (isKeyboardResized_) {
      isKeyboardResized_ = false;
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefRenderWidgetHostViewOSR::WasKeyboardResized,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
#endif
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRenderWidgetHostViewOSR::WasResized,
                                   weak_ptr_factory_.GetWeakPtr()));
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
    }
#endif
  }
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
  }

#if BUILDFLAG(ARKWEB_DRAG_RESIZE)
  bool isDragResized =
      OHOS::NWeb::NWebResizeHelper::GetInstance().IsDragResizeStart();
  if (isDragResized) {
    OHOS::NWeb::NWebResizeHelper::GetInstance().SetDragResizeStart(false);
  }
#endif
}

void CefRenderWidgetHostViewOSR::CancelWidget() {
  if (render_widget_host_) {
    render_widget_host_->LostCapture();
  }

  Hide();

  if (IsPopupWidget() && browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnPopupShow(browser_impl_.get(), false);
    browser_impl_ = nullptr;
  }

  if (parent_host_view_) {
    if (parent_host_view_->popup_host_view_ == this) {
      parent_host_view_->set_popup_host_view(nullptr);
    } else if (parent_host_view_->child_host_view_ == this) {
      parent_host_view_->set_child_host_view(nullptr);

      // Start rendering the parent view again.
      parent_host_view_->Show();
    } else {
      parent_host_view_->RemoveGuestHostView(this);
    }
    parent_host_view_ = nullptr;
  }

  if (render_widget_host_ && !is_destroyed_) {
    is_destroyed_ = true;

    // Don't delete the RWHI manually while owned by a std::unique_ptr in RVHI.
    // This matches a CHECK() in RenderWidgetHostImpl::Destroy().
    const bool also_delete = !render_widget_host_->owner_delegate();

    // Results in a call to Destroy().
    render_widget_host_->ShutdownAndDestroyWidget(also_delete);
  }
}

void CefRenderWidgetHostViewOSR::OnScrollOffsetChanged() {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    auto browser = browser_impl_->GetBrowser();
    if (browser != nullptr && browser->GetHost() != nullptr) {
      float ratio = browser->GetHost()->GetVirtualPixelRatio();
      if (ratio <= 0) {
        LOG(ERROR) << "get ratio invalid: " << ratio;
        return;
      }
      float x = last_scroll_offset_.x() / ratio;
      float y = last_scroll_offset_.y() / ratio;
      handler->OnScrollOffsetChanged(browser_impl_.get(), std::round(x),
                                     std::round(y));
    }
#else
    handler->OnScrollOffsetChanged(browser_impl_.get(), last_scroll_offset_.x(),
                                   last_scroll_offset_.y());
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
  }
  is_scroll_offset_changed_pending_ = false;
}

void CefRenderWidgetHostViewOSR::AddGuestHostView(
    CefRenderWidgetHostViewOSR* guest_host) {
  guest_host_views_.insert(guest_host);
}

void CefRenderWidgetHostViewOSR::RemoveGuestHostView(
    CefRenderWidgetHostViewOSR* guest_host) {
  guest_host_views_.erase(guest_host);
}

void CefRenderWidgetHostViewOSR::InvalidateInternal(
    const gfx::Rect& bounds_in_pixels) {
  if (video_consumer_) {
    video_consumer_->RequestRefreshFrame(bounds_in_pixels);
  } else if (host_display_client_) {
    OnPaint(bounds_in_pixels, host_display_client_->GetPixelSize(),
            host_display_client_->GetPixelMemory());
  }
}

void CefRenderWidgetHostViewOSR::RequestImeCompositionUpdate(
    bool start_monitoring) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->RequestCompositionUpdates(false, start_monitoring);
}

void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::optional<std::vector<gfx::Rect>>& character_bounds,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  if (browser_impl_.get()) {
    CefRange cef_range(range.start(), range.end());
    CefRenderHandler::RectList rcList;

    if (character_bounds.has_value()) {
      for (auto& rect : character_bounds.value()) {
        rcList.emplace_back(rect.x(), rect.y(), rect.width(), rect.height());
      }
    }

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);
    handler->OnImeCompositionRangeChanged(browser_impl_->GetBrowser(),
                                          cef_range, rcList);
  }
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::AllocateFrameSinkId() {
  return render_widget_host_->GetFrameSinkId();
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColorFromRenderer(
    SkColor color) {
  if (color == background_color_) {
    return;
  }
  background_color_ = color;

  bool opaque = SkColorGetA(color) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(color);
}
#if BUILDFLAG(IS_OHOS)
void CefRenderWidgetHostViewOSR::AddCompositor(gfx::AcceleratedWidget widget,
                                               ui::Compositor* compositor) {
  CefRenderWidgetHostViewOSR::compositor_map_.emplace(widget, compositor);
}

ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor(
    gfx::AcceleratedWidget widget) {
  auto it = CefRenderWidgetHostViewOSR::compositor_map_.find(widget);
  if (it == CefRenderWidgetHostViewOSR::compositor_map_.end()) {
    return nullptr;
  }
  return it->second;
}
#endif

#if BUILDFLAG(IS_OHOS)
ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor() {
#ifdef DISABLE_GPU
  return nullptr;
#else
  if (browser_impl_) {
    return CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup_));
  }
  return nullptr;
#endif
}
#endif
