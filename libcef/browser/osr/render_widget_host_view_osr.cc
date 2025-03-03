// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/osr/synthetic_gesture_target_osr.h"
#include "libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "libcef/browser/osr/video_consumer_osr.h"
#include "libcef/browser/thread_util.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/content_switches_internal.h"
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
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"

#ifdef OHOS_EX_TOPCONTROLS
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#endif

#if BUILDFLAG(IS_OHOS)
#include "content/browser/gpu/gpu_process_host.h"
#include "base/ohos/ltpo/include/dynamic_frame_rate_decision.h"
#include "base/ohos/sys_info_utils.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "res_sched_client_adapter.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"
#include "content/browser/browser_main_loop.h"
#include "ohos_adapter_helper.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/text_elider.h"

// static
std::unordered_map<gfx::AcceleratedWidget, ui::Compositor*>
    CefRenderWidgetHostViewOSR::compositor_map_;

std::unordered_map<gfx::AcceleratedWidget, uint32_t>
    CefRenderWidgetHostViewOSR::accelerate_widget_map_;
#endif

#if defined(OHOS_INPUT_EVENTS)
#include "content/public/common/input_event_ack_state.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "libcef/browser/native/cursor_util.h"
#endif  // defined(OHOS_INPUT_EVENTS)

namespace {

// The maximum number of damage rects to cache for outstanding frame requests
// (for OnAcceleratedPaint).
const size_t kMaxDamageRects = 10;

const float kDefaultScaleFactor = 1.0;
#if BUILDFLAG(IS_OHOS)
#if defined(OHOS_PERFORMANCE_JITTER)
const int SOC_PERF_WEB_GESTURE_ID = 10012;
const int TOUCH_DOWN_DELAY_TIME = 200;
const int TOUCH_UP_DURATION_TIME = 100;

constexpr int32_t PINCH_START_TYPE = 1;
constexpr int32_t PINCH_UPDATE_TYPE = 3;
constexpr int32_t PINCH_END_TYPE = 2;

const int32_t DEFAULT_PINCH_FINGER = 2;

#endif
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
#if BUILDFLAG(IS_OHOS)
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

  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override {
    return view_->render_widget_host()->CollectSurfaceIdsForEviction();
  }

  void InvalidateLocalSurfaceIdOnEviction() override {
    view_->InvalidateLocalSurfaceId();
  }

  bool ShouldShowStaleContentOnEviction() override { return false; }

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
  void OnVsync() override { view_->OnVsync(); }

  void OnVsyncReceived() override { view_->OnVsyncReceived(); }
#endif

 private:
  CefRenderWidgetHostViewOSR* const view_;
};

#if BUILDFLAG(IS_OHOS)
class CefGestureEventCallbackImpl : public CefGestureEventCallback {
 public:
  using CallbackType = blink::InputHandlerProxy::GestureEventCallback;

  CefGestureEventCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}
  ~CefGestureEventCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefGestureEventCallbackImpl::CancelNow,
                                     std::move(callback_)));
      }
    }
  }

  void ContinueTask(bool user_input, bool stopPropagation) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::move(callback_).Run(user_input, stopPropagation);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefGestureEventCallbackImpl::ContinueTask, this,
                                   user_input, stopPropagation));
    }
  }


  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(false, true);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefGestureEventCallbackImpl);
};
#endif

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

#if defined(OHOS_INPUT_EVENTS)
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
#endif

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
  tsc_config.enable_longpress_drag_selection = true;
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
      render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
      has_parent_(parent_host_view != nullptr),
      parent_host_view_(parent_host_view),
      pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      mouse_wheel_phase_handler_(this),
      gesture_provider_(CreateGestureProviderConfig(), this),
      weak_ptr_factory_(this) {
  DCHECK(render_widget_host_);
  DCHECK(!render_widget_host_->GetView());

#ifdef OHOS_EX_TOPCONTROLS
  for_browser_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kForBrowser);
#endif

  if (parent_host_view_) {
    browser_impl_ = parent_host_view_->browser_impl();
    DCHECK(browser_impl_);
  } else if (content::RenderViewHost::From(render_widget_host_)) {
    // AlloyBrowserHostImpl might not be created at this time for popups.
    browser_impl_ = AlloyBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }

  delegated_frame_host_client_.reset(new CefDelegatedFrameHostClient(this));

  // Matching the attributes from BrowserCompositorMac.
  delegated_frame_host_ = std::make_unique<content::DelegatedFrameHost>(
      AllocateFrameSinkId(), delegated_frame_host_client_.get(),
#if BUILDFLAG(IS_OHOS)
      true /* should_register_frame_sink_id */);
#else
      false /* should_register_frame_sink_id */);
#endif

  root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));

  bool opaque = SkColorGetA(background_color_) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(background_color_);

  external_begin_frame_enabled_ = use_external_begin_frame;

  auto context_factory = content::GetContextFactory();

  // Matching the attributes from RecyclableCompositorMac.
#ifdef DISABLE_GPU
  compositor_.reset(new ui::Compositor(
      context_factory->AllocateFrameSinkId(), context_factory,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      false /* enable_pixel_canvas */, use_external_begin_frame));
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

  compositor_->SetDelegate(this);
  compositor_->SetRootLayer(root_layer_.get());
  compositor_->AddChildFrameSink(GetFrameSinkId());
#else
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "compositor construct, widget = "
            << static_cast<uint32_t>(browser_impl_->GetAcceleratedWidget());
  ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget());
  accelerate_widget_map_[browser_impl_->GetAcceleratedWidget()]++;
  if (!compositor) {
    compositor = new ui::Compositor(
        context_factory->AllocateFrameSinkId(), context_factory,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        false /* enable_pixel_canvas */, use_external_begin_frame);
    compositor->SetAcceleratedWidget(browser_impl_->GetAcceleratedWidget());
    CefRenderWidgetHostViewOSR::AddCompositor(
        browser_impl_->GetAcceleratedWidget(), compositor);
  }
#endif  // IS_OHOS
#endif

  cursor_manager_.reset(new content::CursorManager(this));

  // This may result in a call to GetFrameSinkId().
  render_widget_host_->SetView(this);

  if (GetTextInputManager()) {
    GetTextInputManager()->AddObserver(this);
  }

  if (render_widget_host_->delegate() &&
      render_widget_host_->delegate()->GetInputEventRouter()) {
    render_widget_host_->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  if (browser_impl_ && !parent_host_view_) {
    // For child/popup views this will be called from the associated InitAs*()
    // method.
    SetRootLayerSize(false /* force */);
    if (!render_widget_host_->is_hidden()) {
      Show();
    }
  }

  selection_controller_client_ =
      std::make_unique<CefTouchSelectionControllerClientOSR>(this);
  CreateSelectionController();

#ifdef OHOS_EX_TOPCONTROLS
  OnTopControlsHeightChanged();
#endif

#if BUILDFLAG(IS_OHOS)
  bool excludable_devices = base::CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDoubleTapSupportForPlatformEnabled);
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
#ifdef DISABLE_GPU
  if (!compositor_) {
    return;  // already released
  }
#else
  if (!browser_impl_) {
    return;
  }
  auto it1 = accelerate_widget_map_.find(browser_impl_->GetAcceleratedWidget());
  if (it1 == accelerate_widget_map_.end()) {
    return;
  }
#endif
  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
#if BUILDFLAG(IS_OHOS)
  if (delegated_frame_host_) {
    if (is_showing_) {
      delegated_frame_host_->WasHidden(
          content::DelegatedFrameHost::HiddenCause::kOther);
    }
    delegated_frame_host_->DetachFromCompositor();
    delegated_frame_host_.reset(nullptr);
  }
#endif

#ifdef DISABLE_GPU
  compositor_.reset(nullptr);
#else
#ifdef BUILDFLAG(IS_OHOS)
  LOG(INFO) << "ReleaseCompositor";
  auto com = compositor_map_.find(browser_impl_->GetAcceleratedWidget());
  if (--accelerate_widget_map_[browser_impl_->GetAcceleratedWidget()] == 0) {
    if (!browser_impl_) {
      return;
    }
    LOG(INFO) << "ReleaseCompositor, widget = "
              << static_cast<uint32_t>(browser_impl_->GetAcceleratedWidget());
    if (com != compositor_map_.end()) {
      if (com->second != nullptr) {
        delete com->second;
      }
      compositor_map_.erase(com);
    }
    accelerate_widget_map_.erase(browser_impl_->GetAcceleratedWidget());
  } else {
    if (com != compositor_map_.end()) {
      if (com->second->delegate() == this) {
        com->second->SetDelegate(nullptr);
      }
    }
  }
#endif  // IS_OHOS
#endif
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
  return gfx::NativeView();
}

gfx::NativeViewAccessible
CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

void CefRenderWidgetHostViewOSR::Focus() {
#if BUILDFLAG(IS_OHOS)
  if (!render_widget_host_) {
    return;
  }
  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  widget->GotFocus();
  widget->SetActive(true);
#endif
}

bool CefRenderWidgetHostViewOSR::HasFocus() {
#if BUILDFLAG(IS_OHOS)
  content::RenderWidgetHostImpl* target_host = render_widget_host_;
  if (render_widget_host_ && render_widget_host_->delegate()) {
    target_host = render_widget_host_->delegate()->GetFocusedRenderWidgetHost(render_widget_host_);
  }
  if (target_host && target_host->GetView()) {
    return target_host->is_focused();
  }
  return false;
#else
  return false;
#endif
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

#ifndef DISABLE_GPU
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget());
  compositor->SetDelegate(this);
  compositor->SetRootLayer(root_layer_.get());
#if !BUILDFLAG(IS_OHOS)
  compositor->AddChildFrameSink(GetFrameSinkId());
#endif
  content::RenderWidgetHostImpl* render_widget_host_impl =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (render_widget_host_impl) {
    render_widget_host_impl->SetCompositorForFlingScheduler(compositor);
  }
#endif

  if (render_widget_host_) {
    render_widget_host_->WasShown(
        /*record_tab_switch_time_request=*/{});

    // Call OnRenderFrameMetadataChangedAfterActivation for every frame.
    auto provider = content::RenderWidgetHostImpl::From(render_widget_host_)
                        ->render_frame_metadata_provider();
    provider->AddObserver(this);
    provider->ReportAllFrameSubmissionsForTesting(true);
  }

#if defined(OHOS_INPUT_EVENTS)
  if (GetTextInputManager() && !GetTextInputManager()->HasObserver(this)) {
    GetTextInputManager()->AddObserver(this);
  }
#endif

  if (delegated_frame_host_) {
#ifdef DISABLE_GPU
    delegated_frame_host_->AttachToCompositor(compositor_.get());
#else
    delegated_frame_host_->AttachToCompositor(compositor);
#endif

    delegated_frame_host_->WasShown(GetLocalSurfaceId(),
#ifdef OHOS_EX_TOPCONTROLS
                                    GetPhysicalViewBounds().size(),
#else
                                    GetViewBounds().size(),
#endif
                                    /*record_tab_switch_time_request=*/{});
  }

  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // and no other visual properties have since been changed. Allocate a new id
  // and start synchronizing.
  if (!GetLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                GetLocalSurfaceId());
  }

  if (!content::GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    // Start generating frames when we're visible and at the correct size.
    if (!video_consumer_) {
#ifdef DISABLE_GPU
      video_consumer_.reset(new CefVideoConsumerOSR(this));
      UpdateFrameRate();
#endif
    } else {
      video_consumer_->SetActive(true);
    }
  }
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

#if defined(OHOS_INPUT_EVENTS)
  if (GetTextInputManager()) {
    GetTextInputManager()->RemoveObserver(this);
  }
#endif
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return is_showing_;
}

#if BUILDFLAG(IS_OHOS)
void CefRenderWidgetHostViewOSR::WasOccluded() {
  Hide();
}

void CefRenderWidgetHostViewOSR::SetEnableLowerFrameRate(bool enabled) {
  if (browser_impl_.get() && browser_impl_->GetAcceleratedWidget()) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
    if (compositor) {
      compositor->SetEnableLowerFrameRate(enabled);
    }
  }
}

void CefRenderWidgetHostViewOSR::UpdateVSyncFrequency() {
  if (browser_impl_ && browser_impl_->GetAcceleratedWidget()) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
    if (compositor) {
      compositor->UpdateVSyncFrequency();
    }
  }
}

void CefRenderWidgetHostViewOSR::ResetVSyncFrequency() {
  if (browser_impl_ && browser_impl_->GetAcceleratedWidget()) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
    if (compositor) {
      compositor->ResetVSyncFrequency();
    }
  }
}

void CefRenderWidgetHostViewOSR::SendTouchEventList(const std::vector<CefTouchEvent>& event_list) {
  TRACE_EVENT1("base", "CefRenderWidgetHostViewOSR::SendTouchEventList", "count", event_list.size());

  for (const auto& event : event_list) {
#if defined(OHOS_PERFORMANCE_JITTER)
    if (event.type == CEF_TET_PRESSED) {
      is_editable_node_ = false;
      auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
          browser_impl_->GetAcceleratedWidget());
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
#ifdef OHOS_CLIPBOARD
    had_no_pointer = had_no_pointer && !pointer_state_.GetPointerCount();
    pointer_state_.SetFromOverlay(event.from_overlay);
    if (event.from_overlay) {
      TriggerVsync();
    }
#endif  // #ifdef OHOS_CLIPBOARD
    if (!pointer_state_.OnTouch(event)) {
      continue;
    }

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
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
      pointer_state_, result.moved_beyond_slop_region, false
#if BUILDFLAG(IS_OHOS)
      , is_fit_content_
#endif
  );

  if (touch_event.GetType() == blink::WebInputEvent::Type::kTouchMove) {
    web_touch_event_count_++;
  }
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::SendTouchEventList web_touch_event_count_:"
                 << web_touch_event_count_;

  for (const auto& event : filtered_event_list) {
    pointer_state_.CleanupRemovedTouchPoints(event);

    // Set unchanged touch point to StateStationary for touchmove and
    // touchcancel to make sure only send one ack per WebTouchEvent.
    if (!result.succeeded)
      pointer_state_.MarkUnchangedTouchPointsAsStationary(&touch_event, event);
  }

  if (!render_widget_host_)
    return;

#if defined(OHOS_PERFORMANCE_JITTER)
  SendTouchGestureEvent(touch_event);
#endif

  bool touch_end =
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}

void CefRenderWidgetHostViewOSR::EvictFrameBackBuffers(bool invisible) {
  TRACE_EVENT1("base", "CefRenderWidgetHostViewOSR::EvictFrameBackBuffers",
               "invisible", invisible);
  if (browser_impl_.get() && browser_impl_->GetAcceleratedWidget()) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget());
    if(compositor) {
        compositor->EvictFrameBackBuffers(invisible);
    }
  }
}

void CefRenderWidgetHostViewOSR::NotifyForNextTouchEvent(bool need_wait_for_touch_move) {
  if (pointer_state_.GetPointerCount() == 0) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR::NotifyForNextTouchEventdo not has touch event:";
    return;
  }

  viz::FrameSinkId id = GetRootFrameSinkId();
  auto host_frame_sink_manager = content::BrowserMainLoop::GetInstance()->host_frame_sink_manager();
  host_frame_sink_manager->SetNeedWaitForInput(id, need_wait_for_touch_move);
}

void CefRenderWidgetHostViewOSR::TriggerVsync() {
  viz::FrameSinkId id = GetRootFrameSinkId();
  auto host_frame_sink_manager = content::BrowserMainLoop::GetInstance()->host_frame_sink_manager();
  TRACE_EVENT0("base", "CefRenderWidgetHostViewOSR::TriggerVsync");
  host_frame_sink_manager->TriggerVsync(id);
}
#endif

void CefRenderWidgetHostViewOSR::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              absl::nullopt);
}

content::TouchSelectionControllerClientManager*
CefRenderWidgetHostViewOSR::GetTouchSelectionControllerClientManager() {
  return selection_controller_client_.get();
}

#ifdef OHOS_EX_TOPCONTROLS
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

#if defined(OHOS_INPUT_EVENTS)
gfx::Size CefRenderWidgetHostViewOSR::GetPhysicalVisibleViewportSize() {
  if (current_visible_view_bounds_.width() == 0 && current_visible_view_bounds_.height() == 0) {
#ifdef OHOS_EX_TOPCONTROLS
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
#ifdef OHOS_EX_TOPCONTROLS
  bounds.set_height(bounds.height() - GetShrinkViewportHeight());
#endif
  return bounds;
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

absl::optional<SkColor> CefRenderWidgetHostViewOSR::GetBackgroundColor() {
  return background_color_;
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColor() {
#if defined(OHOS_BACKGROUND_COLOR)
  if (SkColorGetA(background_color_) != SK_AlphaOPAQUE) {
#ifdef DISABLE_GPU
    if (compositor_) {
      compositor_->SetBackgroundColor(background_color_);
    }
#else
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
    if (compositor) {
      compositor->SetBackgroundColor(background_color_);
    }
#endif
  }
#endif  // defined(OHOS_BACKGROUND_COLOR)
}

absl::optional<content::DisplayFeature>
CefRenderWidgetHostViewOSR::GetDisplayFeature() {
  return absl::nullopt;
}

void CefRenderWidgetHostViewOSR::SetDisplayFeatureForTesting(
    const content::DisplayFeature* display_feature) {
  DCHECK(false);
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::LockMouse(
    bool request_unadjusted_movement) {
#if BUILDFLAG(IS_OHOS)
  if (is_mouse_locked_) {
    return blink::mojom::PointerLockResult::kAlreadyLocked;
  }
  is_mouse_locked_ = true;
  return blink::mojom::PointerLockResult::kSuccess;
#else
  return blink::mojom::PointerLockResult::kPermissionDenied;
#endif
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::ChangeMouseLock(
    bool request_unadjusted_movement) {
#if BUILDFLAG(IS_OHOS)
  if (is_mouse_locked_) {
    return LockMouse(request_unadjusted_movement);
  }

  UnlockMouse();
  return blink::mojom::PointerLockResult::kSuccess;
#else
  return blink::mojom::PointerLockResult::kPermissionDenied;
#endif
}

void CefRenderWidgetHostViewOSR::UnlockMouse() {
#if BUILDFLAG(IS_OHOS)
  if (!is_mouse_locked_) {
    return;
  }
  is_mouse_locked_ = false;
  if (render_widget_host_) {
    render_widget_host_->SendMouseLockLost();
    render_widget_host_->LostMouseLock();
  }
#endif
}

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
    const absl::optional<viz::LocalSurfaceId>&
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

void CefRenderWidgetHostViewOSR::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_) {
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
  }
}

void CefRenderWidgetHostViewOSR::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds,
    const gfx::Rect& anchor_rect) {
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
}

void CefRenderWidgetHostViewOSR::UpdateCursor(const ui::Cursor& cursor) {
#if defined(OHOS_INPUT_EVENTS)
  if (!browser_impl_) {
    LOG(ERROR) << "browser is null when update cursor";
    return;
  }
  cursor_util::OnCursorChange(browser_impl_->GetBrowser(), cursor);
#endif
}

content::CursorManager* CefRenderWidgetHostViewOSR::GetCursorManager() {
  return cursor_manager_.get();
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
  if (!is_loading) {
    return;
  }
#ifndef OHOS_SCROLL_PERFORMANCE
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
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->GetClient()->GetDisplayHandler();
  if (handler.get()) {
    handler->OnTooltip(browser_impl_.get(), tooltip);
  }
}

gfx::Size CefRenderWidgetHostViewOSR::GetCompositorViewportPixelSize() {
#ifdef OHOS_EX_TOPCONTROLS
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
      LOG(DEBUG) << "CefRenderWidgetHostViewOSR::GetScreenInfo orientation:"
                 << screen_info.orientation;
      LOG(DEBUG) << "CefRenderWidgetHostViewOSR::GetScreenInfo angel:"
                 << screen_info.angle;
    }

    display_screen_info = ScreenInfoFrom(screen_info);
  }

  return display::ScreenInfos(display_screen_info);
}

void CefRenderWidgetHostViewOSR::TransformPointToRootSurface(
    gfx::PointF* point) {
#ifdef OHOS_EX_TOPCONTROLS
  *point += gfx::Vector2d(0, GetShrinkViewportHeight());
#endif
}

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

#if BUILDFLAG(IS_OHOS)
void CefRenderWidgetHostViewOSR::SendInternalBeginFrame() {
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
    browser_impl_->GetAcceleratedWidget());
  if (compositor) {
    compositor->SendInternalBeginFrame();
  }
}

void CefRenderWidgetHostViewOSR::SetDrawRect(const gfx::Rect& rect) {
  if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
            browser_impl_->GetAcceleratedWidget())) {
    compositor->SetDrawRect(rect);
    UpdateDrawRect(rect);
  }
}

void CefRenderWidgetHostViewOSR::UpdateDrawRect(const gfx::Rect &rect)
{
  if (!software_compositor_) {
    LOG(ERROR) << "software compositor is null when DrawRect";
    return;
  }
  software_compositor_->DrawRect(rect);
}

void CefRenderWidgetHostViewOSR::SetDrawMode(int mode) {
  if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
            browser_impl_->GetAcceleratedWidget())) {
    compositor->SetDrawMode(mode);
  }
}

void CefRenderWidgetHostViewOSR::SetFitContentMode(int mode) {
  is_fit_content_ = mode;
}

bool CefRenderWidgetHostViewOSR::GetPendingSizeStatus() {
  return false;
}
#endif

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
    web_underlines.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, line.range.from, line.range.to,
        line.thick ? ui::ImeTextSpan::Thickness::kThick
                   : ui::ImeTextSpan::Thickness::kThin,
        GetImeUnderlineStyle(line.style), line.background_color, line.color,
        std::vector<std::string>()));
  }
  gfx::Range range(replacement_range.from, replacement_range.to);

  // Start Monitoring for composition updates before we set.
  RequestImeCompositionUpdate(true);

#if defined(OHOS_INPUT_EVENTS)
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
#if defined(OHOS_INPUT_EVENTS)
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

#if defined(OHOS_INPUT_EVENTS)
  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    text_input_manager_->GetActiveWidget()->ImeFinishComposingText(keep_selection);
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
#if defined(OHOS_INPUT_EVENTS)
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
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  CefRange cef_range(range.start(), range.end());

#if defined(OHOS_INPUT_EVENTS)
  handler->OnSelectionChanged(browser_impl_.get(), text, cef_range);
  if (selection_controller_client_ &&
      selection_controller_client_->IsInsertHandleShow() &&
      range.start() == range.end() &&
      !is_tap_down_in_cursor_update_) {
    handler->StartVibraFeedback("longPress.light");
  }
  is_tap_down_in_cursor_update_ = false;
#endif  // defined(OHOS_INPUT_EVENTS)

  CefString selected_text;
  if (!range.is_empty() && !text.empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n <= text.length()) {
      selected_text = text.substr(pos, n);
    }
#if defined(OHOS_INPUT_EVENTS)
    is_select_text_ = n - pos > 0;
    if (n > 0) {
      handler->StartVibraFeedback("longPress.light");
    }
  } else {
    is_select_text_ = false;
#endif  // defined(OHOS_INPUT_EVENTS)
  }

  handler->OnTextSelectionChanged(browser_impl_.get(), selected_text,
                                  cef_range);
}

const viz::LocalSurfaceId& CefRenderWidgetHostViewOSR::GetLocalSurfaceId()
    const {
  return const_cast<CefRenderWidgetHostViewOSR*>(this)
      ->GetOrCreateLocalSurfaceId();
}

const viz::FrameSinkId& CefRenderWidgetHostViewOSR::GetFrameSinkId() const {
  return delegated_frame_host_
             ? delegated_frame_host_->frame_sink_id()
             : viz::FrameSinkIdAllocator::InvalidFrameSinkId();
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::GetRootFrameSinkId() {
#ifdef DISABLE_GPU
  return compositor_ ? compositor_->frame_sink_id() : viz::FrameSinkId();
#else
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget());
  return compositor ? compositor->frame_sink_id() : viz::FrameSinkId();
#endif
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
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return target_view->TransformPointToLocalCoordSpace(
      point, GetCurrentSurfaceId(), transformed_point);
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
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  absl::nullopt);
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

#if defined(OHOS_SOFTWARE_COMPOSITOR)
void CefRenderWidgetHostViewOSR::OnRendererWidgetCreated() {
  software_compositor_ = std::make_unique<content::SoftwareCompositorHostOhos>(
      render_widget_host_);
}

bool CefRenderWidgetHostViewOSR::WebPageSnapshot(
    const char* id,
    int width,
    int height,
    cef_web_snapshot_callback_t callback) {
  if (!software_compositor_) {
    LOG(ERROR) << "software compositor is null when get snapshot";
    return false;
  }

  if (!browser_impl_) {
    LOG(ERROR) << "browser is null when get snapshot";
    return false;
  }

  gfx::SizeF pageSize{0.0f, 0.0f};
  gfx::PointF pageOffsize{0.0f, 0.0f};
  if (browser_impl_->settings().record_whole_document) {
    pageSize = root_layer_size_;
    pageOffsize = last_scroll_offset_;
  } else {
    pageSize = scrollable_viewport_size_;
  }

  if (width < 0) {
    width = (std::abs(width) * pageSize.width()) / 100;
  }
  if (height < 0) {
    height = (std::abs(height) * pageSize.height()) / 100;
  }

  pageSize.Scale(page_scale_factor_);
  pageOffsize.Scale(page_scale_factor_);

  software_compositor_->DemandDrawSwAsync(id, width, height, pageSize,
                                          pageOffsize, std::move(callback));
  return true;
}
#endif

#if BUILDFLAG(IS_OHOS)
void CefRenderWidgetHostViewOSR::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {
#ifdef OHOS_EX_TOPCONTROLS
  float top_content_offset = metadata.top_controls_height *
                             metadata.top_controls_shown_ratio /
                             metadata.device_scale_factor;
  float top_controls_offset =
      top_content_offset -
      metadata.top_controls_height / metadata.device_scale_factor;

  if (top_content_offset != top_content_offset_ ||
      top_controls_offset != top_controls_offset_) {
    top_content_offset_ = top_content_offset;
    top_controls_offset_ = top_controls_offset;
    OnTopControlsChanged(top_controls_offset_, top_content_offset_);
  }

  if (for_browser_) {
    // Set parameters for adaptive handle orientation.
    gfx::SizeF viewport_size(metadata.scrollable_viewport_size);
    viewport_size.Scale(page_scale_factor_);
    gfx::RectF viewport_rect(0.0f,
                             metadata.top_controls_height *
                                 metadata.top_controls_shown_ratio /
                                 metadata.device_scale_factor,
                             viewport_size.width(), viewport_size.height());
    selection_controller_->OnViewportChanged(viewport_rect);
  }
#endif

  gesture_provider_.SetDoubleTapSupportForPageEnabled(
      !metadata.is_mobile_optimized);
}
#endif

#if defined(OHOS_CLIPBOARD)
void CefRenderWidgetHostViewOSR::MouseSelectMenuShow(bool show) {
  if (selection_controller_client_) {
    selection_controller_client_->MouseSelectMenuShow(show);
  }
}

void CefRenderWidgetHostViewOSR::ChangeVisibilityOfQuickMenu() {
  if (selection_controller_client_) {
    selection_controller_client_->ChangeVisibilityOfQuickMenu();
  }
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

#if BUILDFLAG(IS_OHOS)
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
#if defined(OHOS_SOFTWARE_COMPOSITOR)
  gfx::SizeF scrollable_viewport_size = metadata.scrollable_viewport_size;
  if (scrollable_viewport_size != scrollable_viewport_size_) {
    scrollable_viewport_size_ = scrollable_viewport_size;
    size_changed = true;
  }
#endif

#if defined(OHOS_INPUT_EVENTS)
  if (size_changed && needFocusViewport_ > 0) {
    CefRefPtr<CefRenderHandler> handler =
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

#ifdef OHOS_CLIPBOARD
  if (clipped_selection_bounds_ != metadata.clipped_selection_bounds) {
    clipped_selection_bounds_ = metadata.clipped_selection_bounds;
    selection_controller_client_->UpdateClientClippedSelectionBounds(clipped_selection_bounds_);
  }
#endif  // OHOS_CLIPBOARD
}

std::unique_ptr<viz::HostDisplayClient>
CefRenderWidgetHostViewOSR::CreateHostDisplayClient() {
  host_display_client_ =
      new CefHostDisplayClientOSR(this, gfx::kNullAcceleratedWidget);
  host_display_client_->SetActive(true);
  return base::WrapUnique(host_display_client_);
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
#if defined(OHOS_BACKGROUND_COLOR)
  if (SkColorGetA(background_color_) != SK_AlphaOPAQUE) {
#else
  if (background_color_ == SK_ColorTRANSPARENT) {
#endif  // defined(OHOS_BACKGROUND_COLOR)
    SetBackgroundColor(background_color_);
#ifdef DISABLE_GPU
    if (compositor_) {
      compositor_->SetBackgroundColor(background_color_);
    }
#else
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
    if (compositor) {
      compositor->SetBackgroundColor(background_color_);
    }
#endif
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::WasResized() {
  // Only one resize will be in-flight at a time.
  if (hold_resize_) {
#if defined(OHOS_COMPOSITE_RENDER)
    isKeyboardResized_ = false;
#endif
    if (!pending_resize_) {
      pending_resize_ = true;
    }
    return;
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                              absl::nullopt);
}

#if defined(OHOS_COMPOSITE_RENDER)
void CefRenderWidgetHostViewOSR::SetShouldFrameSubmissionBeforeDraw(
    bool should) {
  TRACE_EVENT0(
      "base", "CefRenderWidgetHostViewOSR::SetShouldFrameSubmissionBeforeDraw");
  should_wait_ = should;
}

void CefRenderWidgetHostViewOSR::WasKeyboardResized() {
  // Only one resize will be in-flight at a time.
  if (hold_resize_) {
    isKeyboardResized_ = true;
    if (!pending_resize_)
      pending_resize_ = true;
    return;
  }

  bool isKeyboardResized = true;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                              absl::nullopt, isKeyboardResized);
}
#endif  // defined(OHOS_COMPOSITE_RENDER)

void CefRenderWidgetHostViewOSR::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
#if defined(OHOS_COMPOSITE_RENDER)
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id,
    bool isKeyboard) {
#else
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id) {
#endif  // defined(OHOS_COMPOSITE_RENDER)
  SetFrameRate();
#if defined(OHOS_INPUT_EVENTS)
  bool visible_changed = false;
  bool resized = ResizeRootLayer(isKeyboard, visible_changed);
  if (!resized) {
    resized = visible_changed;
  }
#else
  const bool resized = ResizeRootLayer();
#endif

#if BUILDFLAG(IS_OHOS)
  if (resized) {
    OHOS::NWeb::ResSchedClientAdapter::ReportScene(
        OHOS::NWeb::ResSchedStatusAdapter::WEB_SCENE_ENTER,
        OHOS::NWeb::ResSchedSceneAdapter::RESIZE);
  }
#endif

#if defined(OHOS_COMPOSITE_RENDER)
  if (resized && should_wait_) {
    if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
            browser_impl_->GetAcceleratedWidget())) {
      compositor->SetShouldFrameSubmissionBeforeDraw(true);
    }
  }
#endif  // defined(OHOS_COMPOSITE_RENDER)
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
#ifdef OHOS_EX_TOPCONTROLS
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
#if defined(OHOS_COMPOSITE_RENDER)
      if (isKeyboard) {
        needFocusViewport_++;
      }
#endif  // defined(OHOS_COMPOSITE_RENDER)
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
                              absl::nullopt);

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
      browser_impl_->GetAcceleratedWidget());
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
    const content::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
#if BUILDFLAG(IS_OHOS)
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    OHOS::NWeb::ResSchedClientAdapter::ReportScene(
        OHOS::NWeb::ResSchedStatusAdapter::WEB_SCENE_ENTER,
        OHOS::NWeb::ResSchedSceneAdapter::KEYBOARD_CLICK);
  }
#endif
  content::RenderWidgetHostImpl* target_host = render_widget_host_;
#ifndef OHOS_CLIPBOARD
  if (selection_controller_client_) {
    selection_controller_client_->CloseQuickMenuAndHideHandles();
  }
#endif  // #ifndef OHOS_CLIPBOARD

#if defined(OHOS_INPUT_EVENTS)
    last_key_code_ = event.windows_key_code;
#endif
  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (render_widget_host_ && render_widget_host_->delegate()) {
    target_host = render_widget_host_->delegate()->GetFocusedRenderWidgetHost(
        render_widget_host_);
  }

  if (target_host && target_host->GetView()) {
    // Direct routing requires that events go directly to the View.
    target_host->ForwardKeyboardEventWithLatencyInfo(
        event,
        ui::LatencyInfo(event.GetType() == blink::WebInputEvent::Type::kChar ||
                                event.GetType() ==
                                    blink::WebInputEvent::Type::kRawKeyDown
                            ? ui::SourceEventType::KEY_PRESS
                            : ui::SourceEventType::OTHER));
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
#ifdef OHOS_CLIPBOARD
    if (selection_controller_client_ && event.GetType() == blink::WebMouseEvent::Type::kMouseDown) {
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
          this, const_cast<blink::WebMouseEvent*>(&event),
          ui::LatencyInfo(ui::SourceEventType::OTHER));
    } else {
      render_widget_host_->GetView()->ProcessMouseEvent(
          event, ui::LatencyInfo(ui::SourceEventType::OTHER));
    }
  }
}

void CefRenderWidgetHostViewOSR::SendTouchpadFlingEvent(
    blink::WebGestureEvent event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchpadFlingEvent");

  if (render_widget_host_ && render_widget_host_->GetView()) {
    if (ShouldRouteEvents()) {
      render_widget_host_->delegate()
          ->GetInputEventRouter()
          ->RouteGestureEvent(
              this, const_cast<blink::WebGestureEvent*>(&event),
              ui::LatencyInfo(ui::SourceEventType::WHEEL));
    } else {
      render_widget_host_->GetView()->ProcessGestureEvent(
          event, ui::LatencyInfo(ui::SourceEventType::WHEEL));
    }
  }
}

void CefRenderWidgetHostViewOSR::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");

  if (!IsPopupWidget()) {
    if (browser_impl_) {
      browser_impl_->CancelContextMenu();
    }
#ifndef OHOS_CLIPBOARD
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
#if defined(OHOS_INPUT_EVENTS)
    bool shouldRoute = ShouldRouteEvents();
    mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(shouldRoute);
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
              ui::LatencyInfo(ui::SourceEventType::WHEEL));
    } else {
      render_widget_host_->GetView()->ProcessMouseWheelEvent(
          mouse_wheel_event, ui::LatencyInfo(ui::SourceEventType::WHEEL));
    }
  }
}

#if defined(OHOS_PERFORMANCE_JITTER)
void CefRenderWidgetHostViewOSR::SendTouchGestureEvent(blink::WebTouchEvent& touch_event) {
  ui::LatencyInfo latency_info = CreateLatencyInfo(touch_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    render_widget_host_->ForwardTouchEventWithLatencyInfo(touch_event,
                                                          latency_info);
  }
}
#endif

void CefRenderWidgetHostViewOSR::SendTouchEvent(const CefTouchEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchEvent");

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
  if (event.type == CEF_TET_PRESSED) {
    is_editable_node_ = false;
    auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
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
#ifdef OHOS_CLIPBOARD
  pointer_state_.SetFromOverlay(event.from_overlay);
  if (event.from_overlay) {
    TriggerVsync();
  }
#endif  // #ifdef OHOS_CLIPBOARD
  if (!pointer_state_.OnTouch(event)) {
    return;
  }

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
  OnTouchDown();
#endif

  if (selection_controller_->WillHandleTouchEvent(pointer_state_)) {
    pointer_state_.CleanupRemovedTouchPoints(event);
    return;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(pointer_state_);

  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, result.moved_beyond_slop_region, false);

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
    render_widget_host_->ForwardTouchEventWithLatencyInfo(touch_event,
                                                          latency_info);
  }

  bool touch_end =
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}

#ifdef OHOS_CLIPBOARD
void CefRenderWidgetHostViewOSR::ResetGestureDetection(bool is_lost_focus) {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  std::unique_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  cancel_event->SetCancelByLostFocus(is_lost_focus);
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
    blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
        *cancel_event, causes_scrolling /* may_cause_scrolling */,
        false /* hovering */);
    if (ShouldRouteEvents()) {
      host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
          this, &web_event, latency_info);
    } else {
      host()->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
    }
  }
}
#endif

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
void CefRenderWidgetHostViewOSR::StopBoosting() {
  if (is_fling_) {
    return;
  }
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
    .CreateSocPerfClientAdapter()
    ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, false);
}

void CefRenderWidgetHostViewOSR::BoostingPreiodly() {
  if(pointer_state_.GetPointerCount() == 0) {
    return;
  }
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
    .CreateSocPerfClientAdapter()
    ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, true);
  CEF_POST_DELAYED_TASK(CEF_UIT,
    base::BindOnce(&CefRenderWidgetHostViewOSR::BoostingPreiodly,
    weak_ptr_factory_.GetWeakPtr()), TOUCH_DOWN_DELAY_TIME);
}

void CefRenderWidgetHostViewOSR::OnTouchDown() {
  if (pointer_state_.GetPointerCount() == 0) {
    has_touch_point_ = false;
    if (isBoosting_) {
      isBoosting_ = false;
      CEF_POST_DELAYED_TASK(CEF_UIT,
        base::BindOnce(&CefRenderWidgetHostViewOSR::StopBoosting,
          weak_ptr_factory_.GetWeakPtr()), TOUCH_UP_DURATION_TIME);
      if (auto* host = content::GpuProcessHost::Get()) {
        if (auto* host_impl = host->gpu_host()) {
          host_impl->SetHasTouchPoint(false);
        }
      }
    }
    return;
  }
  if (isBoosting_) {
    OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GESTURE_ID, true);
    CEF_POST_DELAYED_TASK(CEF_UIT,
      base::BindOnce(&CefRenderWidgetHostViewOSR::OnTouchDown,
      weak_ptr_factory_.GetWeakPtr()), TOUCH_DOWN_DELAY_TIME);
  }

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
  if(pointer_state_.GetPointerCount() == 0) {
    return;
  }
  isBoosting_ = true;
  BoostingPreiodly();
}
#endif

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

  LOG(INFO) << "CefRenderWidgetHostViewOSR::SetFocus:" << focus;
  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (focus) {
    widget->GotFocus();
    widget->SetActive(true);
#ifdef OHOS_CLIPBOARD
    if (selection_controller_client_) {
      selection_controller_client_->SetTemporarilyHidden(false);
    }
#endif  // #ifdef OHOS_CLIPBOARD
  } else {
#if !BUILDFLAG(IS_OHOS)
    if (browser_impl_.get()) {
      browser_impl_->CancelContextMenu();
    }
#endif
    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }

    widget->SetActive(false);
    widget->LostFocus();
  }
}

void CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalledInner(const ui::mojom::TextInputState* state) {
  CefRefPtr<CefRenderHandler> handler =
    browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);
  if (state != nullptr) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: state->show_ime_if_needed " << state->show_ime_if_needed;
    std::u16string whole_text = state->value.value_or(std::u16string());
    CefRange selection_range(state->selection.start(), state->selection.end());
    CefRange compositon_range = CefRange::InvalidRange();
    if (state->composition) {
      gfx::Range range = state->composition.value();
      compositon_range.Set(range.start(), range.end());
    }
    handler->OnUpdateTextInputStateCalled(browser_impl_.get(), whole_text, selection_range, compositon_range);
  } else {
    ImeFinishComposingText(false);
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: status is nullptr, finish compositon";
  }
}

void CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled(
    content::TextInputManager* text_input_manager,
    content::RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled ";
  CefRenderHandler::TextInputMode mode = CEF_TEXT_INPUT_MODE_NONE;
  CefRenderHandler::TextInputType type = CEF_TEXT_INPUT_TYPE_NONE;
  CefRenderHandler::TextInputAction action = CEF_TEXT_INPUT_ACTION_DEFAULT;
  CefRenderHandler::TextInputFlags flags = CEF_TEXT_INPUT_FLAG_NONE;
  bool show_keyboard = false;
  std::map<CefString, CefString> text_input_attributes;
  const auto state = text_input_manager->GetTextInputState();
  if (state) {
    for (const auto& atrribute : state->input_element_attributes) {
      text_input_attributes.insert(std::make_pair<CefString, CefString>(CefString(atrribute.first),
        CefString(atrribute.second)));
    }
  }

#if defined(OHOS_INPUT_EVENTS)

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  if (UpdateEditBounds()) {
    auto processedOffset = HandleCursorOffset();
    handler->OnCursorUpdate(browser_impl_->GetBrowser(),
                            CefRect(processedOffset.first, processedOffset.second,
                                    focus_rect_width_, focus_rect_height_));
  }

  bool is_need_reset_ime_listener = false;
  if (state) {
    int32_t current_node_id = state->node_id;
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: node->ID " << state->node_id <<
      ", node_id_ " << node_id_;
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
    mode = static_cast<CefRenderHandler::TextInputMode>(state->mode);
    type = state->flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD
               ? CEF_TEXT_INPUT_TYPE_PASSWORD
               : static_cast<CefRenderHandler::TextInputType>(state->type);
    action = static_cast<CefRenderHandler::TextInputAction>(state->action);
    flags = static_cast<CefRenderHandler::TextInputFlags>(state->flags);
    show_keyboard = state->show_ime_if_needed;
  }
  if (state && !state->show_ime_if_needed && did_update_state) {
    LOG(INFO) << "Autofocus requires a keyboard to be bound, "
                 "but there is no need to pull up the keyboard";
    // TODO(OHOS) Specific implementation needs to be completed
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested update state";
    CefRenderHandler::TextInputInfo input_info = {
        .node_id = state->node_id,
        .show_keyboard = state->show_ime_if_needed,
        .input_mode = mode,
        .input_type = type,
        .input_action = action,
        .input_flags = flags,
        .always_hide_ime = state->always_hide_ime};
    handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), input_info,
                                        is_need_reset_ime_listener,
                                        text_input_attributes);
    OnUpdateTextInputStateCalledInner(state);
    return;
  }
  if (!show_keyboard) {
    last_key_code_ = -1;
  }

  if (state && state->show_ime_if_needed) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested show_ime_if_needed";
    // TODO(OHOS) Specific implementation needs to be completed
    CefRenderHandler::TextInputInfo input_info = {
        .node_id = state->node_id,
        .show_keyboard = show_keyboard,
        .input_mode = mode,
        .input_type = type,
        .input_action = action,
        .input_flags = flags,
        .always_hide_ime = state->always_hide_ime};
    handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), input_info,
                                        is_need_reset_ime_listener,
                                        text_input_attributes);
  } else if ((!state || mode == CEF_TEXT_INPUT_MODE_NONE) && (HasFocus())) {
    LOG(DEBUG) << "CefRenderWidgetHostViewOSR:: OnVirtualKeyboardRequested not editable";
    CefRenderHandler::TextInputInfo input_info;
    handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), input_info,
                                        is_need_reset_ime_listener,
                                        text_input_attributes);
  }
  OnUpdateTextInputStateCalledInner(state);
#else
  const auto state = text_input_manager->GetTextInputState();
  if (state && !state->show_ime_if_needed)
    return;

  CefRenderHandler::TextInputMode mode = CEF_TEXT_INPUT_MODE_NONE;
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    static_assert(
        static_cast<int>(CEF_TEXT_INPUT_MODE_MAX) ==
            static_cast<int>(ui::TEXT_INPUT_MODE_MAX),
        "Enum values in cef_text_input_mode_t must match ui::TextInputMode");
    mode = static_cast<CefRenderHandler::TextInputMode>(state->mode);
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), mode, type,
                                      show_keyboard);
#endif  // defined(OHOS_INPUT_EVENTS)
}

void CefRenderWidgetHostViewOSR::ProcessAckedTouchEvent(
    const content::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(touch.event.unique_touch_event_id,
                                    event_consumed, false);
}

#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
void CefRenderWidgetHostViewOSR::OnVsync() {
}

void CefRenderWidgetHostViewOSR::OnVsyncReceived() {
}
#endif

void CefRenderWidgetHostViewOSR::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::ET_GESTURE_PINCH_BEGIN ||
       gesture.type() == ui::ET_GESTURE_PINCH_UPDATE ||
       gesture.type() == ui::ET_GESTURE_PINCH_END) &&
      !pinch_zoom_enabled_) {
    return;
  }
#if defined(OHOS_INPUT_EVENTS)
  FilterScrollEventImpl(gesture);
#endif
#if BUILDFLAG(IS_OHOS) && defined(OHOS_PERFORMANCE_JITTER)
  SendGestureEvent(gesture);
}

void CefRenderWidgetHostViewOSR::ScaleGestureChangeV2(int type,
                                                      float scale,
                                                      float originScale,
                                                      float centerX,
                                                      float centerY)
{
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::ScaleGestureChangeV2 type: " << type
             << " scale: " << scale << " originScale: " << originScale;
  auto event_type = ui::ET_UNKNOWN;
  switch (type)
  {
  case PINCH_START_TYPE:
    event_type = ui::ET_GESTURE_PINCH_BEGIN;
    break;
  case PINCH_UPDATE_TYPE:
    event_type = ui::ET_GESTURE_PINCH_UPDATE;
    break;
  case PINCH_END_TYPE:
    event_type = ui::ET_GESTURE_PINCH_END;
    break;
  default:
    LOG(ERROR) << "CefRenderWidgetHostViewOSR::ScaleGestureChangeV2 type invalid.";
    return;
  }

    ui::GestureEventDetails details(event_type);
    details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
    details.set_scale(scale);
    OnGestureEvent(ui::GestureEventData(details,
                                          0,
                                          ui::MotionEvent::ToolType::FINGER,
                                          base::TimeTicks::Now(),
                                          centerX,
                                          centerY,
                                          centerX,
                                          centerY,
                                          DEFAULT_PINCH_FINGER,
                                          gfx::RectF(),
                                          0,
                                          0U));
}

void CefRenderWidgetHostViewOSR::SendGestureEvent(
    const ui::GestureEventData& gesture) {
#endif

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  // without this check, forwarding gestures does not work!
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

#ifdef OHOS_CLIPBOARD
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
    render_widget_host_->ForwardGestureEventWithLatencyInfo(web_event,
                                                            latency_info);
  }
}

#if BUILDFLAG(IS_OHOS)
bool CefRenderWidgetHostViewOSR::RequiresDoubleTapGestureEvents() const {
  return base::CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDoubleTapSupportForPlatformEnabled);
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
  if (IsPopupWidget()) {
    return gfx::ScaleToCeiledSize(popup_position_.size(),
                                  GetDeviceScaleFactor());
  }

  CefSize size {};
  if (browser_impl_ && browser_impl_->GetClient()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);
    handler->GetDevicePixelSize(browser_impl_.get(), size);
  } else {
    LOG(WARNING) << "cannot get device pixel size, return zero";
  }
  return gfx::Size(size.width, size.height);
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
#endif  // BUILDFLAG(IS_MAC)

void CefRenderWidgetHostViewOSR::OnPaint(const gfx::Rect& damage_rect,
                                         const gfx::Size& pixel_size,
                                         const void* pixels) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnPaint");

  // Workaround for https://github.com/chromiumembedded/cef/issues/2817
  if (!is_showing_) {
    return;
  }

  if (!pixels) {
    return;
  }

#if defined(OHOS_INPUT_EVENTS)
  if (!browser_impl_ || !browser_impl_->client()) {
    LOG(ERROR) << "get client failed.";
    return;
  }
#endif  // defined(OHOS_INPUT_EVENTS)

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);

  gfx::Rect rect_in_pixels(0, 0, pixel_size.width(), pixel_size.height());
  rect_in_pixels.Intersect(damage_rect);

  CefRenderHandler::RectList rcList;
  rcList.push_back(CefRect(rect_in_pixels.x(), rect_in_pixels.y(),
                           rect_in_pixels.width(), rect_in_pixels.height()));

  handler->OnPaint(browser_impl_.get(), IsPopupWidget() ? PET_POPUP : PET_VIEW,
                   rcList, pixels, pixel_size.width(), pixel_size.height());

  // Release the resize hold when we reach the desired size.
  if (hold_resize_) {
    DCHECK_GT(cached_scale_factor_, 0);
#ifdef OHOS_EX_TOPCONTROLS
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
      browser_impl_->GetAcceleratedWidget());
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

#if defined(OHOS_INPUT_EVENTS)
bool CefRenderWidgetHostViewOSR::SetVisibleViewportSize() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  // Popup bounds are set in InitAsPopup.
  if (IsPopupWidget()) {
    return false;
  }

  const gfx::Size& visible_view_bounds = ::GetVisibleViewportSize(browser_impl_.get());
  if (visible_view_bounds == current_visible_view_bounds_) {
    return false;
  }

  current_visible_view_bounds_ = visible_view_bounds;
  return true;
}
#endif

#if defined(OHOS_INPUT_EVENTS)
bool CefRenderWidgetHostViewOSR::SetRootLayerSize(bool force, bool* visible_changed) {
#else
bool CefRenderWidgetHostViewOSR::SetRootLayerSize(bool force) {
#endif
  const bool screen_info_changed = SetScreenInfo();
  const bool view_bounds_changed = SetViewBounds();
#if defined(OHOS_INPUT_EVENTS)
  const bool visible_view_bounds_changed = SetVisibleViewportSize();
  if (visible_changed) {
    *visible_changed = visible_view_bounds_changed;
  }
#endif
  if (!force && !screen_info_changed && !view_bounds_changed
#if defined(OHOS_INPUT_EVENTS)
      && !visible_view_bounds_changed
#endif
  ) {
    return false;
  }

#ifdef OHOS_EX_TOPCONTROLS
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
      browser_impl_->GetAcceleratedWidget());
  if (compositor) {
    compositor_local_surface_id_allocator_.GenerateId();
    compositor->SetScaleAndSize(
#endif
        GetDeviceScaleFactor(), SizeInPixels(),
        compositor_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }

#if BUILDFLAG(IS_OHOS)
  // We only need to notify the screen information change, the visual window
  // adjustment is done by CefRenderWidgetHostViewOSR::WasResized.
  return view_bounds_changed;
#else
  return (screen_info_changed || view_bounds_changed);
#endif
}

#if defined(OHOS_INPUT_EVENTS)
bool CefRenderWidgetHostViewOSR::ResizeRootLayer(bool isKeyboard, bool& visible_changed) {
#else
bool CefRenderWidgetHostViewOSR::ResizeRootLayer() {
#endif
  if (!hold_resize_) {
    bool reseize = SetRootLayerSize(false /* force */,  &visible_changed);
    // The resize hold is not currently active.
    if (reseize) {
      // The size has changed. Avoid resizing again until ReleaseResizeHold() is
      // called.
      hold_resize_ = true;
      cached_scale_factor_ = GetDeviceScaleFactor();
      return true;
    }
  } else if (!pending_resize_) {
    // The resize hold is currently active. Another resize will be triggered
    // from ReleaseResizeHold().
#if defined(OHOS_COMPOSITE_RENDER)
    isKeyboardResized_ = isKeyboard;
#endif
    pending_resize_ = true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::ReleaseResizeHold() {
  DCHECK(hold_resize_);
  hold_resize_ = false;
  cached_scale_factor_ = -1;
  if (pending_resize_) {
    pending_resize_ = false;
#if defined(OHOS_COMPOSITE_RENDER)
    if (isKeyboardResized_) {
      isKeyboardResized_ = false;
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRenderWidgetHostViewOSR::WasKeyboardResized,
                                 weak_ptr_factory_.GetWeakPtr()));
    } else {
#endif
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRenderWidgetHostViewOSR::WasResized,
                                 weak_ptr_factory_.GetWeakPtr()));
#if defined(OHOS_COMPOSITE_RENDER)
    }
#endif
  }
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
  }
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
#if defined(OHOS_INPUT_EVENTS)
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
#endif  // defined(OHOS_INPUT_EVENTS)
  }
  is_scroll_offset_changed_pending_ = false;
}

#if BUILDFLAG(IS_OHOS) && defined(OHOS_SCROLL_PERFORMANCE)
void CefRenderWidgetHostViewOSR::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result,
    blink::mojom::ScrollResultDataPtr scroll_result_data) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::GestureEventAck ack_result:" << (int)ack_result;
  StopFlingingIfNecessary(event, ack_result);

  ForwardTouchpadZoomEventIfNecessary(event, ack_result);
}
#endif

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
    const std::vector<gfx::Rect>& character_bounds) {
  if (browser_impl_.get()) {
    CefRange cef_range(range.start(), range.end());
    CefRenderHandler::RectList rcList;

    for (size_t i = 0; i < character_bounds.size(); ++i) {
      rcList.push_back(CefRect(character_bounds[i].x(), character_bounds[i].y(),
                               character_bounds[i].width(),
                               character_bounds[i].height()));
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

#ifdef OHOS_CLIPBOARD
std::u16string CefRenderWidgetHostViewOSR::GetSelectedText() {
  if (text_input_manager_) {
    return text_input_manager_->GetTextSelection(this)->selected_text();
  }
  return std::u16string();
}

std::u16string CefRenderWidgetHostViewOSR::GetText() {
  if (text_input_manager_)
    return text_input_manager_->GetTextSelection(this)->text();
  return std::u16string();
}

void CefRenderWidgetHostViewOSR::OnTextSelectionChanged(
    content::TextInputManager *text_input_manager,
    RenderWidgetHostViewBase *updated_view) {
  if (!text_input_manager || !updated_view) {
    LOG(ERROR) << "OnTextSelectionChanged text is null";
    return;
  }
  const content::TextInputManager::TextSelection &selection =
      *text_input_manager->GetTextSelection(updated_view);
  if (!browser_impl_ || !browser_impl_->GetClient()) {
    LOG(ERROR) << "OnTextSelectionChanged get client failed";
    return;
  }
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->OnTextSelectionChanged(
      browser_impl_.get(), selection.selected_text(),
      CefRange(selection.range().start(), selection.range().end()));
}
#endif  // #ifdef OHOS_CLIPBOARD

#ifdef OHOS_EX_FREE_COPY
std::vector<int8_t> CefRenderWidgetHostViewOSR::GetWordSelection(const std::string& text, int8_t offset) {
  CefPoint temp(-1, -1);
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->GetWordSelection(browser_impl_.get(), text, offset, temp);
  }
  std::vector<int8_t> select = { temp.x, temp.y };
  return select;
}
#endif

#if BUILDFLAG(IS_OHOS)
bool CefRenderWidgetHostViewOSR::IsMouseLocked() {
  return is_mouse_locked_;
}

void CefRenderWidgetHostViewOSR::OnScaleChanged(float old_page_scale_factor,
                                                float nwe_page_scale_factor) {
  if (browser_impl_.get()) {
    CefRefPtr<CefDisplayHandler> handler =
        browser_impl_->client()->GetDisplayHandler();
    CHECK(handler);
    handler->OnScaleChanged(browser_impl_.get(),
                            std::round(old_page_scale_factor * 100),
                            std::round(nwe_page_scale_factor * 100));
  }
}

void CefRenderWidgetHostViewOSR::OnTouchSelectionChanged(
    const CefTouchHandleState& insert_handle,
    const CefTouchHandleState& start_selection_handle,
    const CefTouchHandleState& end_selection_handle,
    bool need_report) {
  if (!browser_impl_) {
    return;
  }
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  if (handler) {
    handler->OnTouchSelectionChanged(insert_handle, start_selection_handle,
                                     end_selection_handle, need_report);
  }
}

bool CefRenderWidgetHostViewOSR::NeedPopupInsertTouchHandleQuickMenu() {
  if (selection_controller_client_) {
    selection_controller_client_->NeedPopupInsertTouchHandleQuickMenu();
  }
  return false;
}

#ifdef OHOS_DRAG_DROP
void CefRenderWidgetHostViewOSR::SetTextHandlesTemporarilyHiddenByDrag(bool hide_handles, bool dragging) {
  LOG(INFO) << "SetTextHandlesTemporarilyHiddenByDrag hide_handles:" << hide_handles << ", dragging:" << dragging;
  if (!dragging && selection_controller_) {
    selection_controller_->ResetResponsePendingInputEvent();
  }

  if (selection_controller_client_) {
    selection_controller_client_->HideHandleAndQuickMenuIfNecessary(hide_handles);
  }
}
#endif

void CefRenderWidgetHostViewOSR::OnRootLayerChanged() {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
  }
}

void CefRenderWidgetHostViewOSR::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void CefRenderWidgetHostViewOSR::SetMultiTouchZoomSupportEnabled(bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

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

#if defined(OHOS_INPUT_EVENTS)
void CefRenderWidgetHostViewOSR::SelectionBoundsChanged(
    const gfx::Rect& anchor_rect,
    base::i18n::TextDirection anchor_dir,
    const gfx::Rect& focus_rect,
    base::i18n::TextDirection focus_dir,
    const gfx::Rect& bounding_box,
    bool is_anchor_first) {
  if (!browser_impl_) {
    LOG(ERROR) << "browser_impl_ is nullptr";
    return;
  }

  focus_rect_x_ = focus_rect.x();
  focus_rect_y_ = focus_rect.y();
  focus_rect_width_ = focus_rect.width();
  focus_rect_height_ = focus_rect.height();

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  if (!is_select_text_) {
    handler->OnCursorUpdate(browser_impl_->GetBrowser(),
                            CefRect(focus_rect_x_, focus_rect_y_,
                                    focus_rect_width_, focus_rect_height_));
    return;
  }

  if (UpdateEditBounds()) {
    auto processedOffset = HandleCursorOffset();
    handler->OnCursorUpdate(browser_impl_->GetBrowser(),
                            CefRect(processedOffset.first, processedOffset.second,
                                    focus_rect_width_, focus_rect_height_));
  }
}

bool CefRenderWidgetHostViewOSR::UpdateEditBounds() {
  if (text_input_manager_ && text_input_manager_->GetTextInputState()) {
    auto state = text_input_manager_->GetTextInputState();
    if (!state || !state->edit_context_control_bounds) {
      return false;
    }
    edit_bounds_x_ = state->edit_context_control_bounds->x();
    edit_bounds_y_ = state->edit_context_control_bounds->y();
    edit_bounds_width_ = state->edit_context_control_bounds->width();
    edit_bounds_height_ = state->edit_context_control_bounds->height();
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::FocusedNodeChanged, editable="
             << is_editable_node;
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);
  handler->OnEditableChanged(browser_impl_.get(), is_editable_node);

  is_editable_node_ = is_editable_node;
}

std::pair<int, int> CefRenderWidgetHostViewOSR::HandleCursorOffset() {
  int x = focus_rect_x_;
  int y = focus_rect_y_;
  x = x < edit_bounds_x_ ? edit_bounds_x_ : x;
  x = x > (edit_bounds_x_ + edit_bounds_width_)
          ? (edit_bounds_x_ + edit_bounds_width_)
          : x;
  y = y < edit_bounds_y_ ? edit_bounds_y_ : y;
  y = y > (edit_bounds_y_ + edit_bounds_height_)
          ? (edit_bounds_y_ + edit_bounds_height_)
          : y;
  return std::make_pair(x, y);
}

void CefRenderWidgetHostViewOSR::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    float x = params.latest_overscroll_delta.x();
    float y = params.latest_overscroll_delta.y();
    handler->OnOverscroll(browser_impl_.get(), x, y);

    float fling_velocity_x = params.current_fling_velocity.x();
    float fling_velocity_y = params.current_fling_velocity.y();
    bool is_fling = true;
    if (fling_velocity_x == 0 && fling_velocity_y == 0) {
      fling_velocity_x = x;
      fling_velocity_y = y;
      is_fling = false;
    }
    fling_velocity_x =
        params.accumulated_overscroll.x() == 0 ? 0 : fling_velocity_x;
    fling_velocity_y =
        params.accumulated_overscroll.y() == 0 ? 0 : fling_velocity_y;
    handler->OnOverScrollFlingVelocity(browser_impl_.get(), fling_velocity_x,
                                       fling_velocity_y, is_fling);
  }
}

void CefRenderWidgetHostViewOSR::DidStopFlinging() {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::DidStopFlinging";
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnOverScrollFlingEnd(browser_impl_.get());
    is_fling_ = false;
  }
}

blink::mojom::InputEventResultState
CefRenderWidgetHostViewOSR::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::FilterInputEvent";

  if (!scroll_enabled_ &&
      input_event.GetType() ==
          blink::WebInputEvent::Type::kGestureScrollUpdate) {
    LOG(DEBUG) << "can not GestureScroll, scroll is disabled";
    return blink::mojom::InputEventResultState::kConsumed;
  }

  if (input_event.GetType() ==
        blink::WebInputEvent::Type::kMouseWheel) {
    is_mouse_wheel_scroll_ = true;
  }

  if (browser_impl_.get() && input_event.IsGestureScroll()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    if (input_event.GetType() ==
        blink::WebInputEvent::Type::kGestureScrollBegin) {
#ifdef OHOS_AI
        is_scrolling_ = true;
#endif
      is_scroll_consumed_ = false;
      selection_controller_client_->OnScrollStarted();
      handler->OnScrollStart(browser_impl_.get(), 
                            gesture_event.data.scroll_begin.delta_x_hint,
                            gesture_event.data.scroll_begin.delta_y_hint);
    } else if (input_event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
#ifdef OHOS_AI
      is_scrolling_ = false;
#endif
      is_scroll_consumed_ = false;
      selection_controller_client_->OnScrollCompleted();
      handler->OnScrollState(browser_impl_.get(), false);
#ifdef OHOS_AI
      if (render_widget_host_ && overlay_in_progress_) {
        gfx::Rect image_rect = render_widget_host_->GetImageRect();
        CefRect cef_image_rect(image_rect.x(), image_rect.y(), image_rect.width(), image_rect.height());
        handler->OnOverlayStateChanged(browser_impl_.get(), cef_image_rect);
      }
#endif
    } else if (input_event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollUpdate &&
               is_mouse_wheel_scroll_) {
#ifdef OHOS_AI
      is_scrolling_ = true;
#endif
      is_scroll_consumed_ =
        handler->FilterScrollEvent(browser_impl_.get(),
                                  gesture_event.data.scroll_update.delta_x,
                                  gesture_event.data.scroll_update.delta_y, 0, 0);
      is_mouse_wheel_scroll_ = false;
    }
    return is_scroll_consumed_ ? blink::mojom::InputEventResultState::kConsumed
                               : blink::mojom::InputEventResultState::kNotConsumed;
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
}

void CefRenderWidgetHostViewOSR::FilterScrollEventImpl(
  const ui::GestureEventData& gesture) {
  blink::WebGestureEvent web_event =
    ui::CreateWebGestureEventFromGestureEventData(gesture);
  if (!browser_impl_.get()) {
    LOG(ERROR) << "FilterScrollEventImpl get browser_impl_ is nullptr.";
    return;
  }
  CefRefPtr<CefRenderHandler> handler =
    browser_impl_->client()->GetRenderHandler();
  if (!handler) {
    LOG(ERROR) << "FilterScrollEventImpl get handler is nullptr.";
    return;
  }
  if (web_event.GetType() ==
      blink::WebInputEvent::Type::kGestureScrollUpdate) {
    is_scroll_consumed_ =
      handler->FilterScrollEvent(browser_impl_.get(),
                                  web_event.data.scroll_update.delta_x,
                                  web_event.data.scroll_update.delta_y, 0, 0);
  } else if (web_event.GetType() ==
              blink::WebInputEvent::Type::kGestureFlingStart) {
    is_scroll_consumed_ =
      handler->FilterScrollEvent(browser_impl_.get(),
                                  0, 0, web_event.data.fling_start.velocity_x,
                                  web_event.data.fling_start.velocity_y);
    is_fling_ = true;
  }
}
#endif  // defined(OHOS_INPUT_EVENTS)

#ifdef OHOS_EX_TOPCONTROLS
int CefRenderWidgetHostViewOSR::GetTopControlsOffset() const {
  return (for_browser_ ? top_controls_offset_ : 0);
}

int CefRenderWidgetHostViewOSR::GetShrinkViewportHeight() {
  int shrink_viewport_height = 0;
  if (!for_browser_ || !host()->delegate()) {
    return shrink_viewport_height;
  }

  auto rvh_delegate_view = host()->delegate()->GetDelegateView();
  if (rvh_delegate_view->DoBrowserControlsShrinkRendererSize()) {
    gfx::Size shrink_size(0, rvh_delegate_view->GetTopControlsHeight());
    shrink_viewport_height =
        gfx::ScaleToFlooredSize(shrink_size, 1.f / GetDeviceScaleFactor())
            .height();
  }

  return shrink_viewport_height;
}

void CefRenderWidgetHostViewOSR::OnTopControlsChanged(
    float top_controls_offset,
    float top_content_offset) {
  if (!for_browser_ || !browser_impl_.get() ||
      !browser_impl_->GetClient().get()) {
    return;
  }
  browser_impl_->GetClient()->OnTopControlsChanged(top_controls_offset,
                                                   top_content_offset);
  gfx::Transform root_layer_transform;
  root_layer_transform.Translate(gfx::Vector2dF(0, top_content_offset));
  GetRootLayer()->SetTransform(root_layer_transform);
}

void CefRenderWidgetHostViewOSR::OnTopControlsHeightChanged() {
  if (auto rvh_delegate_view = host()->delegate()->GetDelegateView()) {
    GetRootLayer()->SetTopControlsHeight(
        rvh_delegate_view->GetTopControlsHeight() / GetDeviceScaleFactor());
  }
}
#endif  // defined(OHOS_EX_TOPCONTROLS)

#if defined(OHOS_INPUT_EVENTS)
void CefRenderWidgetHostViewOSR::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  content::RenderFrameHostImpl* frame_host = render_widget_host()->frame_tree()->GetMainFrame();
  if(!frame_host){
    return;
  }
  frame_host->GetPage().NotifyVirtualKeyboardOverlayRect(keyboard_rect);
}

ui::mojom::VirtualKeyboardMode
CefRenderWidgetHostViewOSR::GetVirtualKeyboardMode() {
  // overlaycontent flag can only be set from main frame.
  content::RenderFrameHostImpl* frame_host = render_widget_host()->frame_tree()->GetMainFrame();
  if (!frame_host)
    return ui::mojom::VirtualKeyboardMode::kUnset;

  return frame_host->GetPage().virtual_keyboard_mode();
}

void CefRenderWidgetHostViewOSR::SetVirtualKeyBoardArg(int32_t width, int32_t height, double keyboard) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::SetVirtualKeyBoardArg ; width = "
             << width << "height = " << height << "keyboard = " << keyboard;
  gfx::Rect keyboard_rect;
  keyboard_rect.set_x(0);
  keyboard_rect.set_y(0);
  keyboard_rect.set_width(width);
  keyboard_rect.set_height(keyboard);

  if (GetVirtualKeyboardMode() !=
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    return;
  }

  gfx::Rect keyboard_rect_with_scale;
  if (!keyboard_rect.IsEmpty()) {
    auto browser = browser_impl_->GetBrowser();
    if (browser != nullptr && browser->GetHost() != nullptr) {
      float ratio = browser->GetHost()->GetVirtualPixelRatio();
      if (ratio <= 0) {
        LOG(ERROR) << __func__ << " get ratio invalid: " << ratio;
        return;
      }
      keyboard_rect_with_scale = ScaleToEnclosedRect(keyboard_rect, 1 / ratio);
      keyboard_rect_with_scale.Intersect(GetViewBounds());
      LOG(DEBUG) << "CefRenderWidgetHostViewOSR::SetVirtualKeyBoardArg"
                 << ",keyboard_rect_with_scale.width" << keyboard_rect_with_scale.width()
                 << ",keyboard_rect_with_scale.height" << keyboard_rect_with_scale.height();
    }
  }
  NotifyVirtualKeyboardOverlayRect(keyboard_rect_with_scale);
}

void CefRenderWidgetHostViewOSR::DidNativeEmbedEvent(const blink::mojom::NativeEmbedTouchEventPtr& touchEvent) {

  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    CefRenderHandler::CefEmbedTouchEvent event{touchEvent->embedId, touchEvent->id, touchEvent->x, touchEvent->y,
      touchEvent->screenX, touchEvent->screenY, static_cast<CefRenderHandler::CefEmbedTouchType>(touchEvent->type),
      touchEvent->offsetX, touchEvent->offsetY};
    auto new_callback = base::BindOnce(
      &CefRenderWidgetHostViewOSR::SetGestureEventResult, weak_ptr_factory_.GetWeakPtr());
    CefRefPtr<CefGestureEventCallbackImpl> callbackPtr(
      new CefGestureEventCallbackImpl(std::move(new_callback)));
    handler->OnNativeEmbedGestureEvent(browser_impl_.get(), event, callbackPtr.get());
  }
}

void CefRenderWidgetHostViewOSR::OnNativeEmbedLifecycleChange(const CefRenderHandler::CefNativeEmbedData& info){
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnNativeEmbedLifecycleChange(browser_impl_.get(), info);

  }
}

void CefRenderWidgetHostViewOSR::OnNativeEmbedVisibilityChange(const std::string& embed_id, bool visibility){
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    
    handler->OnNativeEmbedVisibilityChange(embed_id, visibility);
  }
}

void CefRenderWidgetHostViewOSR::SetGestureEventResult(bool result, bool stopPropagation) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->SetGestureEventResult(result, stopPropagation);
}

void CefRenderWidgetHostViewOSR::SetNativeEmbedMode(bool flag) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->SetNativeEmbedMode(flag);
}

void CefRenderWidgetHostViewOSR::SetScrollable(bool enable) {
  scroll_enabled_ = enable;
}

bool CefRenderWidgetHostViewOSR::GetScrollable() {
  return scroll_enabled_;
}

void CefRenderWidgetHostViewOSR::ScrollBy(float delta_x, float delta_y) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->ScrollBy(delta_x, delta_y);
}

void CefRenderWidgetHostViewOSR::OnDidNavigateMainFrameToNewPage() {
  ResetGestureDetection(false);
}

void CefRenderWidgetHostViewOSR::AdvanceFocusForIME(int focusType) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::AdvanceFocusForIME focusType = "
            << focusType;
  content::RenderFrameHostImpl* frame_host = nullptr;
  if (render_widget_host() && render_widget_host()->frame_tree() &&
      render_widget_host()->frame_tree()->GetFocusedFrame()) {
    frame_host = render_widget_host()
                     ->frame_tree()
                     ->GetFocusedFrame()
                     ->current_frame_host();
  }

  if (frame_host && frame_host->GetAssociatedLocalFrame()) {
    frame_host->GetAssociatedLocalFrame()->AdvanceFocusForIME(
        static_cast<blink::mojom::FocusType>(focusType));
  }
}
#endif

#if BUILDFLAG(IS_OHOS)
ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor() {
#ifdef DISABLE_GPU
  return nullptr;
#else
  if (browser_impl_) {
    return CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget());
  }
  return nullptr;
#endif
}
#endif

#ifdef OHOS_DISPLAY_CUTOUT
void CefRenderWidgetHostViewOSR::OnSafeInsetsChange(
    const gfx::Insets& safe_insets) {
  float ratio = 1.f;
  auto browser = browser_impl_->GetBrowser();
  if (browser != nullptr && browser->GetHost() != nullptr) {
    ratio = browser->GetHost()->GetVirtualPixelRatio();
    if (ratio <= 0) {
      LOG(ERROR) << __func__ << " get ratio invalid: " << ratio;
      return;
    }
  }
  if (auto rvh_delegate_view = host()->delegate()->GetDelegateView()) {
    rvh_delegate_view->OnSafeInsetsChange(
        gfx::ScaleToCeiledInsets(safe_insets, 1.f / ratio));
  }
}
#endif

#ifdef OHOS_AI
bool CefRenderWidgetHostViewOSR::IsScrolling() {
  return is_scrolling_;
}

void CefRenderWidgetHostViewOSR::OnTextSelected(bool flag) {
  if (render_widget_host_) {
    render_widget_host_->OnTextSelected(flag);
    overlay_in_progress_ = flag;
  }
}

void CefRenderWidgetHostViewOSR::OnDestroyImageAnalyzerOverlay() {
  if (render_widget_host_) {
    render_widget_host_->OnDestroyImageAnalyzerOverlay();
  }
}

float CefRenderWidgetHostViewOSR::GetPageScaleFactor() {
  return page_scale_factor_;
}
#endif

#if BUILDFLAG(IS_OHOS)
void CefRenderWidgetHostViewOSR::MaximizeResize() {
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor();
  if (compositor) {
    compositor->DisableSwapUntilMaximized();
  }
}

void CefRenderWidgetHostViewOSR::RestoreRenderFit() {
  if (!browser_impl_ || !browser_impl_->client()) {
    LOG(ERROR) << "RestoreRenderFit get client failed.";
    return;
  }
  CefRefPtr<CefRenderHandler> handler = browser_impl_->client()->GetRenderHandler();
  CHECK(handler);
  handler->RestoreRenderFit();
}
#endif