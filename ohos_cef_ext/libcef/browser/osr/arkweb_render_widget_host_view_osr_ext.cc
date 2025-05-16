/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"

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
#include "cef/libcef/browser/osr/osr_util.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "cef/libcef/browser/osr/synthetic_gesture_target_osr.h"
#include "cef/libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "cef/libcef/browser/osr/video_consumer_osr.h"
#include "cef/libcef/browser/thread_util.h"
#include "components/input/input_event_ack_state.h"
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
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "base/ohos/sys_info_utils_ext.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "cef/ohos_cef_ext/include/arkweb_render_handler_ext.h"
#include "cef/ohos_cef_ext/include/cef_web_client_extension_handler.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_touch_selection_controller_client_osr_ext.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_switches_internal.h"
#include "content/common/native_embed_first_paint_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "libcef/browser/web_client_extension/alloy_native_embed_first_frame_paint_event.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"
#include "ui/compositor/compositor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
#include "cef/ohos_cef_ext/include/arkweb_client_ext.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#endif

#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
const size_t kMaxGestureQueueSize = 10;
const size_t KFirstRecordingTimes = 3;
#endif
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
constexpr int32_t PINCH_START_TYPE = 1;
constexpr int32_t PINCH_UPDATE_TYPE = 3;
constexpr int32_t PINCH_END_TYPE = 2;
const int32_t DEFAULT_PINCH_FINGER = 2;
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
#include "cef/ohos_cef_ext/include/arkweb_render_handler_ext.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/types/event_type.h"
#include "ui/latency/latency_info.h"
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
#if BUILDFLAG(ARKWEB_AI)
#include "cef/libcef/browser/image_impl.h"
#endif

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
#include "arkweb/chromium_ext/content/browser/ohos/overscroll_controller_ohos.h"
#include "arkweb/chromium_ext/ui/ohos/overscroll_refresh.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#endif

#if BUILDFLAG(IS_ARKWEB)
const int SCALE_FACTOR_CONVERT_RATIO = 100;
#endif

#if BUILDFLAG(ARKWEB_SAME_LAYER)
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
                    base::BindOnce(&CefGestureEventCallbackImpl::ContinueTask,
                                   this, user_input, stopPropagation));
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

ArkWebRenderWidgetHostViewOSRExt::ArkWebRenderWidgetHostViewOSRExt(
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame,
    content::RenderWidgetHost* widget,
    CefRenderWidgetHostViewOSR* parent_host_view)
    : CefRenderWidgetHostViewOSR(background_color,
                                 use_shared_texture,
                                 use_external_begin_frame,
                                 widget,
                                 parent_host_view),
#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
      enable_nweb_ex_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExTopControls)),
#endif
      weak_ptr_factory_(this) {
#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  OnTopControlsHeightChanged();
#endif

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
  CreateOverscrollControllerIfPossible();
#endif
}

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
void ArkWebRenderWidgetHostViewOSRExt::SetDrawRect(const gfx::Rect& rect) {
  if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
          browser_impl_->GetAcceleratedWidget(is_popup_))) {
    compositor->SetDrawRect(rect);
    UpdateDrawRect(rect);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::UpdateDrawRect(const gfx::Rect& rect) {
  if (!software_compositor_) {
    LOG(ERROR) << "software compositor is null when DrawRect";
    return;
  }
  software_compositor_->DrawRect(rect);
}

void ArkWebRenderWidgetHostViewOSRExt::SetDrawMode(int mode) {
  if (auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
          browser_impl_->GetAcceleratedWidget(is_popup_))) {
    compositor->SetDrawMode(mode);
  }
}
#endif
void ArkWebRenderWidgetHostViewOSRExt::SetFitContentMode(int mode) {
  is_fit_content_ = mode;
}

bool ArkWebRenderWidgetHostViewOSRExt::GetPendingSizeStatus() {
  return false;
}

void ArkWebRenderWidgetHostViewOSRExt::SetShouldFrameSubmissionBeforeDraw(
    bool should) {
  TRACE_EVENT0(
      "base",
      "ArkWebRenderWidgetHostViewOSRExt::SetShouldFrameSubmissionBeforeDraw");
  should_wait_ = should;
}

void ArkWebRenderWidgetHostViewOSRExt::SendCurrentLanguage(
    const std::string& ans) {
  LOG(DEBUG) << "SendCurrentLanguage language is " << ans.c_str();
  language_ = ans;
}
#endif

#if BUILDFLAG(IS_ARKWEB)
void ArkWebRenderWidgetHostViewOSRExt::SetDoubleTapSupportEnabled(
    bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void ArkWebRenderWidgetHostViewOSRExt::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}
#endif

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
void ArkWebRenderWidgetHostViewOSRExt::WasOccluded() {
  Hide();
}
 
void ArkWebRenderWidgetHostViewOSRExt::SetEnableLowerFrameRate(bool enabled) {}
#endif

#if BUILDFLAG(ARKWEB_DISPLAY_CUTOUT)
void ArkWebRenderWidgetHostViewOSRExt::OnSafeInsetsChange(
    const gfx::Insets& safe_insets) {
  float ratio = 1.f;
  auto browser = browser_impl()->GetBrowser();
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

#if BUILDFLAG(ARKWEB_AI)
std::vector<int8_t> ArkWebRenderWidgetHostViewOSRExt::GetWordSelection(
    const std::string& text,
    int8_t offset) {
  CefPoint temp(-1, -1);
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->GetWordSelection(browser_impl_.get(), text, offset, temp);
  }
  std::vector<int8_t> select = {temp.x, temp.y};
  return select;
}

void ArkWebRenderWidgetHostViewOSRExt::CreateOverlay(
    const gfx::ImageSkia& image,
    const gfx::Rect& image_rect,
    const gfx::Point& touch_point) {
  CefRefPtr<ArkWebRenderHandlerExt> handler =
      browser_impl_->client()->GetRenderHandler();

  if (handler.get()) {
    CefRefPtr<CefImage> cef_image(new CefImageImpl(image));
    CefRect cef_image_rect(image_rect.x(), image_rect.y(), image_rect.width(),
                           image_rect.height());
    CefPoint cef_touch_point(touch_point.x(), touch_point.y());
    LOG(INFO) << "ArkWebRenderWidgetHostViewOSRExt::CreateOverlay";
    handler->CreateOverlay(browser_impl_, cef_image, cef_image_rect,
                           cef_touch_point);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnTextSelected(bool flag) {
  if (flag) {
    gesture_provider_.OnAITextSelected();
  }
  if (render_widget_host_) {
    render_widget_host_->OnTextSelected(flag);
    overlay_in_progress_ = flag;
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnOverlayStateChanged() {
  CefRefPtr<ArkWebRenderHandlerExt> handler =
      browser_impl_->client()->GetRenderHandler();
  if (handler) {
    if (render_widget_host_ && overlay_in_progress_) {
      gfx::Rect image_rect = render_widget_host_->GetImageRect();
      CefRect cef_image_rect(image_rect.x(), image_rect.y(), image_rect.width(),
                             image_rect.height());
      handler->OnOverlayStateChanged(browser_impl_.get(), cef_image_rect);
    }
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnDestroyImageAnalyzerOverlay() {
  overlay_in_progress_ = false;
  if (render_widget_host_) {
    render_widget_host_->OnDestroyImageAnalyzerOverlay();
  }
}

float ArkWebRenderWidgetHostViewOSRExt::GetPageScaleFactor() {
  return page_scale_factor_;
}

void ArkWebRenderWidgetHostViewOSRExt::OnFoldStatusChanged(
    uint32_t foldstatus) {
  if (render_widget_host_) {
    render_widget_host_->OnFoldStatusChanged(foldstatus);
  }
}

bool ArkWebRenderWidgetHostViewOSRExt::CloseImageOverlaySelection() {
  if (browser_impl_ && browser_impl_->GetClient()) {
    CefRefPtr<CefContextMenuHandlerExt> handler =
        browser_impl_->GetClient()->GetContextMenuHandler();
    if (handler) {
      return handler->CloseImageOverlaySelection();
    }
  }
  return false;
}
#endif

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
void ArkWebRenderWidgetHostViewOSRExt::UpdateVSyncFrequency() {
  if (browser_impl_ && browser_impl_->GetAcceleratedWidget(is_popup)) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup));
    if (compositor) {
      compositor->UpdateVSyncFrequency();
    }
  }
}

void ArkWebRenderWidgetHostViewOSRExt::ResetVSyncFrequency() {
  if (browser_impl_ && browser_impl_->GetAcceleratedWidget(is_popup)) {
    ui::Compositor* compositor = CefRenderWidgetHostViewOSR::GetCompositor(
        browser_impl_->GetAcceleratedWidget(is_popup));
    if (compositor) {
      compositor->ResetVSyncFrequency();
    }
  }
}
#endif

#if BUILDFLAG(ARKWEB_MENU)
void ArkWebRenderWidgetHostViewOSRExt::OnTouchSelectionChanged(
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

bool ArkWebRenderWidgetHostViewOSRExt::NeedPopupInsertTouchHandleQuickMenu() {
  if (selection_controller_client_) {
    selection_controller_client_->AsArkWebTouchSelectionControllerClientOSRExt()
        ->NeedPopupInsertTouchHandleQuickMenu();
  }
  return false;
}

std::u16string ArkWebRenderWidgetHostViewOSRExt::GetSelectedText() {
  if (text_input_manager_) {
    return text_input_manager_->GetTextSelection(this)->selected_text();
  }
  return std::u16string();
}

std::u16string ArkWebRenderWidgetHostViewOSRExt::GetText() {
  if (text_input_manager_) {
    return text_input_manager_->GetTextSelection(this)->text();
  }
  return std::u16string();
}

void ArkWebRenderWidgetHostViewOSRExt::ResetGestureDetection(
    bool is_lost_focus) {
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
    // 114 ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    ui::LatencyInfo latency_info{};
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

void ArkWebRenderWidgetHostViewOSRExt::OnTextSelectionChanged(
    content::TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  if (!text_input_manager || !updated_view) {
    LOG(ERROR) << "OnTextSelectionChanged text is null";
    return;
  }
  const content::TextInputManager::TextSelection& selection =
      *text_input_manager->GetTextSelection(updated_view);
  if (!browser_impl_ || !browser_impl_->GetClient()) {
    LOG(ERROR) << "OnTextSelectionChanged get client failed";
    return;
  }
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);
  if (!selection.selected_text().empty() && !selection.range().is_empty()) {
    handler->OnTextSelectionChanged(
        browser_impl_.get(), selection.selected_text(),
        CefRange(selection.range().start(), selection.range().end()));
  } else {
    LOG(INFO) << "OnTextSelectionChanged selected_text is null";
  }
}
#endif

#if BUILDFLAG(IS_ARKWEB)
void ArkWebRenderWidgetHostViewOSRExt::
    OnRenderFrameMetadataChangedBeforeActivation(
        const cc::RenderFrameMetadata& metadata) {
#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
  if (overscroll_controller_) {
    overscroll_controller_->OnFrameMetadataUpdated(
        metadata.page_scale_factor, metadata.device_scale_factor,
        metadata.scrollable_viewport_size, metadata.root_layer_size,
        metadata.root_scroll_offset.value_or(gfx::PointF()),
        metadata.root_overflow_y_hidden);
  }
#endif

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  float top_content_offset = metadata.top_controls_height *
                             metadata.top_controls_shown_ratio /
                             metadata.device_scale_factor;
  float top_controls_offset =
      top_content_offset -
      metadata.top_controls_height / metadata.device_scale_factor;

  if (!pull_to_refreshing_ && (top_content_offset != top_content_offset_ ||
                               top_controls_offset != top_controls_offset_)) {
    top_content_offset_ = top_content_offset;
    top_controls_offset_ = top_controls_offset;
    OnTopControlsChanged(top_controls_offset_, top_content_offset_);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExTopControls)) {
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

void ArkWebRenderWidgetHostViewOSRExt::
    OnRenderFrameMetadataChangedAfterActivation(
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
    if (browser_impl_.get()) {
      CefRefPtr<CefDisplayHandler> handler =
          browser_impl_->client()->GetDisplayHandler();
      CHECK(handler);
      handler->OnScaleInited(
          browser_impl_.get(),
          std::round(page_scale_factor_ * SCALE_FACTOR_CONVERT_RATIO));
    }
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

#if BUILDFLAG(ARKWEB_MENU)
  if (clipped_selection_bounds_ != metadata.clipped_selection_bounds) {
    clipped_selection_bounds_ = metadata.clipped_selection_bounds;
    selection_controller_client_->UpdateClientClippedSelectionBounds(
        clipped_selection_bounds_);
  }
#endif
}
#endif  // BUILDFLAG(IS_ARKWEB)

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkWebRenderWidgetHostViewOSRExt::SelectionChanged(
    const std::u16string& text,
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

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  handler->AsArkWebRenderHandler()->OnSelectionChanged(browser_impl_.get(),
                                                       text, cef_range);
  if (selection_controller_client_ &&
      selection_controller_client_->IsInsertHandleShow() &&
      range.start() == range.end() && !is_tap_down_in_cursor_update_) {
    handler->AsArkWebRenderHandler()->StartVibraFeedback("longPress.light");
  }
  is_tap_down_in_cursor_update_ = false;
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

  CefString selected_text;
  if (!range.is_empty() && !text.empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n <= text.length()) {
      selected_text = text.substr(pos, n);
    }
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    is_select_text_ = n - pos > 0;
    if (n > 0) {
      handler->AsArkWebRenderHandler()->StartVibraFeedback("longPress.light");
    }
  } else {
    is_select_text_ = false;
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
  }

  handler->OnTextSelectionChanged(browser_impl_.get(), selected_text,
                                  cef_range);
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
blink::mojom::PointerLockResult ArkWebRenderWidgetHostViewOSRExt::LockPointer(
    bool request_unadjusted_movement) {
  if (is_pointer_locked_) {
    return blink::mojom::PointerLockResult::kAlreadyLocked;
  }
  is_pointer_locked_ = true;
  is_request_unadjusted_movement_ = request_unadjusted_movement;
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->client()->GetDisplayHandler();
  LOG(INFO) << "SetMouseLock unadjust mouse movement is "
            << (request_unadjusted_movement ? "on" : "off");
  if (handler) {
    CefCursorInfo cursor_info;
    handler->OnCursorChange(browser_impl_->GetBrowser(), nullptr, CT_LOCK,
                            cursor_info);
  }
  return blink::mojom::PointerLockResult::kSuccess;
}

blink::mojom::PointerLockResult
ArkWebRenderWidgetHostViewOSRExt::ChangePointerLock(
    bool request_unadjusted_movement) {
  if (is_pointer_locked_) {
    return LockPointer(request_unadjusted_movement);
  }

  UnlockPointer();
  return blink::mojom::PointerLockResult::kSuccess;
}

void ArkWebRenderWidgetHostViewOSRExt::UnlockPointer() {
  if (!is_pointer_locked_) {
    return;
  }
  is_pointer_locked_ = false;
  is_request_unadjusted_movement_ = false;
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->client()->GetDisplayHandler();
  LOG(INFO) << "SetMouseLock off";
  if (handler) {
    CefCursorInfo cursor_info;
    handler->OnCursorChange(browser_impl_->GetBrowser(), nullptr, CT_UNLOCK,
                            cursor_info);
  }
  if (render_widget_host_) {
    render_widget_host_->SendPointerLockLost();
    render_widget_host_->LostPointerLock();
  }
}

bool ArkWebRenderWidgetHostViewOSRExt::IsPointerLocked() {
  return is_pointer_locked_;
}
#endif

void ArkWebRenderWidgetHostViewOSRExt::SelectionBoundsChanged(
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
    handler->AsArkWebRenderHandler()->OnCursorUpdate(
        browser_impl_->GetBrowser(),
        CefRect(focus_rect_x_, focus_rect_y_, focus_rect_width_,
                focus_rect_height_));
    return;
  }

  if (UpdateEditBounds()) {
    auto processedOffset = HandleCursorOffset();
    handler->AsArkWebRenderHandler()->OnCursorUpdate(
        browser_impl_->GetBrowser(),
        CefRect(processedOffset.first, processedOffset.second,
                focus_rect_width_, focus_rect_height_));
  }
}

bool ArkWebRenderWidgetHostViewOSRExt::UpdateEditBounds() {
  if (text_input_manager_ && text_input_manager_->GetTextInputState()) {
    auto state = text_input_manager_->GetTextInputState();
    if (!state || !state->edit_context_control_bounds) {
      return false;
    }
    CHECK(state);
    CHECK(state->edit_context_control_bounds);
    edit_bounds_x_ = state->edit_context_control_bounds->x();
    edit_bounds_y_ = state->edit_context_control_bounds->y();
    edit_bounds_width_ = state->edit_context_control_bounds->width();
    edit_bounds_height_ = state->edit_context_control_bounds->height();
    return true;
  }
  return false;
}
void ArkWebRenderWidgetHostViewOSRExt::SendInternalBeginFrame() {
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (compositor) {
    compositor->SendInternalBeginFrame();
  }
}

void ArkWebRenderWidgetHostViewOSRExt::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);
  handler->AsArkWebRenderHandler()->OnEditableChanged(browser_impl_.get(),
                                                      is_editable_node);
  is_editable_node_ = is_editable_node;
}

void ArkWebRenderWidgetHostViewOSRExt::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    float x = params.latest_overscroll_delta.x();
    float y = params.latest_overscroll_delta.y();
    handler->AsArkWebRenderHandler()->OnOverscroll(browser_impl_.get(), x, y);

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
    handler->AsArkWebRenderHandler()->OnOverScrollFlingVelocity(
        browser_impl_.get(), fling_velocity_x, fling_velocity_y, is_fling);

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
    if (overscroll_controller_) {
      overscroll_controller_->OnOverscrolled(params);
    }
#endif
  }
}

void ArkWebRenderWidgetHostViewOSRExt::DidStopFlinging() {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::DidStopFlinging";
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->AsArkWebRenderHandler()->OnOverScrollFlingEnd(browser_impl_.get());
#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
    is_fling_ = false;
#endif
  }
}
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkWebRenderWidgetHostViewOSRExt::OnUpdateTextInputStateCalledInner(
    const ui::mojom::TextInputState* state) {
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);
    if (state != nullptr) {
      LOG(DEBUG)
          << "ArkWebRenderWidgetHostViewOSRExt:: state->show_ime_if_needed "
          << state->show_ime_if_needed;
      std::u16string whole_text = state->value.value_or(std::u16string());
      CefRange selection_range(state->selection.start(),
                               state->selection.end());
      CefRange compositon_range = CefRange::InvalidRange();
      if (state->composition) {
        gfx::Range range = state->composition.value();
        compositon_range.Set(range.start(), range.end());
      }
      handler->OnUpdateTextInputStateCalled(browser_impl_.get(), whole_text,
                                            selection_range, compositon_range);
    } else {
      CefRenderWidgetHostViewOSR::ImeFinishComposingText(false);
      LOG(DEBUG) << "ArkWebRenderWidgetHostViewOSRExt:: status is nullptr, "
                    "finish compositon";
    }
  }
}

void ArkWebRenderWidgetHostViewOSRExt::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  if (event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
      event.primary_pointer_type == ui::EventPointerType::kMouse) {
    if (ack_result != blink::mojom::InputEventResultState::kConsumed) {
      while (pending_touchpad_pinch_events_.size()) {
        SendGestureEvent(pending_touchpad_pinch_events_.front());
        pending_touchpad_pinch_events_.pop();
      }
    } else {
      std::queue<ui::GestureEventData> tmp;
      pending_touchpad_pinch_events_.swap(tmp);
    }
  }

  StopFlingingIfNecessary(event, ack_result);

  if (overscroll_controller_) {
    overscroll_controller_->OnGestureEventAck(event, ack_result);
  }
}
#endif

#if BUILDFLAG(ARKWEB_SCROLLBAR)
blink::mojom::InputEventResultState
ArkWebRenderWidgetHostViewOSRExt::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  LOG(DEBUG) << "CefRenderWidgetHostViewOSR::FilterInputEvent";

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
  if (FilterInputEventForPullToRefresh(input_event)) {
    return blink::mojom::InputEventResultState::kConsumed;
  }
#endif

  if (!scroll_enabled_ &&
      input_event.GetType() ==
          blink::WebInputEvent::Type::kGestureScrollUpdate) {
    LOG(DEBUG) << "can not GestureScroll, scroll is disabled";
    return blink::mojom::InputEventResultState::kConsumed;
  }

  if (input_event.GetType() == blink::WebInputEvent::Type::kMouseWheel) {
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
      is_scroll_consumed_ = false;
      selection_controller_client_->OnScrollStarted();
      handler->AsArkWebRenderHandler()->OnScrollState(browser_impl_.get(),
                                                      true);
      handler->AsArkWebRenderHandler()->OnScrollStart(
          browser_impl_.get(), gesture_event.data.scroll_begin.delta_x_hint,
          gesture_event.data.scroll_begin.delta_y_hint);
    } else if (input_event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      is_scroll_consumed_ = false;
      selection_controller_client_->OnScrollCompleted();
      handler->AsArkWebRenderHandler()->OnScrollState(browser_impl_.get(),
                                                      false);
#if BUILDFLAG(ARKWEB_AI)
      if (render_widget_host_ && overlay_in_progress_) {
        gfx::Rect image_rect = render_widget_host_->GetImageRect();
        CefRect cef_image_rect(image_rect.x(), image_rect.y(),
                               image_rect.width(), image_rect.height());
        handler->AsArkWebRenderHandler()->OnOverlayStateChanged(
            browser_impl_.get(), cef_image_rect);
      }
#endif
    } else if (input_event.GetType() ==
                   blink::WebInputEvent::Type::kGestureScrollUpdate &&
               is_mouse_wheel_scroll_) {
      is_scroll_consumed_ = handler->AsArkWebRenderHandler()->FilterScrollEvent(
          browser_impl_.get(), gesture_event.data.scroll_update.delta_x,
          gesture_event.data.scroll_update.delta_y, 0, 0);
      is_mouse_wheel_scroll_ = false;
    }
    return is_scroll_consumed_
               ? blink::mojom::InputEventResultState::kConsumed
               : blink::mojom::InputEventResultState::kNotConsumed;
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
}
#endif

void ArkWebRenderWidgetHostViewOSRExt::OnScrollState(bool scroll_state) {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->AsArkWebRenderHandler()->OnScrollState(browser_impl_.get(),
                                                    scroll_state);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::AdvanceFocusForIME(int focusType) {
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

std::pair<int, int> ArkWebRenderWidgetHostViewOSRExt::HandleCursorOffset() {
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

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
bool ArkWebRenderWidgetHostViewOSRExt::IsRequestUnadjustedMovement() {
  return is_request_unadjusted_movement_;
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

void CefRenderWidgetHostViewOSR::SetScrollable(bool enable) {
  scroll_enabled_ = enable;
}

void ArkWebRenderWidgetHostViewOSRExt::ScrollBy(float delta_x, float delta_y) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->ScrollBy(delta_x, delta_y);
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_REPORT_LOSS_FRAME)
void ArkWebRenderWidgetHostViewOSRExt::DynamicFrameLossEvent(
    const std::string& sceneId,
    bool isStart) {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->AsArkWebRenderHandler()->SendDynamicFrameLossEvent(
        browser_impl_.get(), sceneId, isStart);
  }
}
#endif

#if BUILDFLAG(ARKWEB_DSS)
gfx::Size ArkWebRenderWidgetHostViewOSRExt::SizeInPixels() {
  if (IsPopupWidget()) {
    return gfx::ScaleToCeiledSize(popup_position_.size(),
                                  GetDeviceScaleFactor());
  }

  CefSize size{};
  if (browser_impl_ && browser_impl_->GetClient() &&
      browser_impl_->GetClient()->GetRenderHandler()) {
    auto handler =
        browser_impl_->GetClient()->GetRenderHandler()->AsArkWebRenderHandler();
    CHECK(handler);
    handler->GetDevicePixelSize(browser_impl_.get(), size);
  } else {
    LOG(WARNING) << "cannot get device pixel size, return zero";
  }
  return gfx::Size(size.width, size.height);
}
#endif

#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
bool ArkWebRenderWidgetHostViewOSRExt::WebPageSnapshot(
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

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkWebRenderWidgetHostViewOSRExt::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::EventType::kGesturePinchBegin ||
       gesture.type() == ui::EventType::kGesturePinchUpdate ||
       gesture.type() == ui::EventType::kGesturePinchEnd) &&
      !pinch_zoom_enabled_) {
    return;
  }
  FilterScrollEventImpl(gesture);
  SendGestureEvent(gesture);
}

void ArkWebRenderWidgetHostViewOSRExt::ScaleGestureChangeV2(int type,
                                                            float scale,
                                                            float originScale,
                                                            float centerX,
                                                            float centerY) {
  LOG(DEBUG) << "ArkWebRenderWidgetHostViewOSRExt::ScaleGestureChangeV2 type: "
             << type << " scale: " << scale << " originScale: " << originScale;
  auto event_type = ui::EventType::kUnknown;
  switch (type) {
    case PINCH_START_TYPE:
      event_type = ui::EventType::kGesturePinchBegin;
      break;
    case PINCH_UPDATE_TYPE:
      event_type = ui::EventType::kGesturePinchUpdate;
      break;
    case PINCH_END_TYPE:
      event_type = ui::EventType::kGesturePinchEnd;
      break;
    default:
      LOG(ERROR) << "ArkWebRenderWidgetHostViewOSRExt::ScaleGestureChangeV2 "
                    "type invalid.";
      return;
  }

  ui::GestureEventDetails details(event_type);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  details.set_scale(originScale);
  SendGestureEvent(
      ui::GestureEventData(details, 0, ui::MotionEvent::ToolType::MOUSE,
                           base::TimeTicks::Now(), centerX, centerY, centerX,
                           centerY, DEFAULT_PINCH_FINGER, gfx::RectF(), 0, 0U));

  ui::GestureEventDetails details2(event_type);
  details2.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  details2.set_scale(scale);
  pending_touchpad_pinch_events_.push(
      ui::GestureEventData(details2, 0, ui::MotionEvent::ToolType::FINGER,
                           base::TimeTicks::Now(), centerX, centerY, centerX,
                           centerY, DEFAULT_PINCH_FINGER, gfx::RectF(), 0, 0U));
}

void ArkWebRenderWidgetHostViewOSRExt::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  content::RenderFrameHostImpl* frame_host =
      render_widget_host()->frame_tree()->GetMainFrame();
  if (!frame_host) {
    return;
  }
  frame_host->GetPage().NotifyVirtualKeyboardOverlayRect(keyboard_rect);
}

ui::mojom::VirtualKeyboardMode
ArkWebRenderWidgetHostViewOSRExt::GetVirtualKeyboardMode() {
  // overlaycontent flag can only be set from main frame.
  content::RenderFrameHostImpl* frame_host =
      render_widget_host()->frame_tree()->GetMainFrame();
  if (!frame_host) {
    return ui::mojom::VirtualKeyboardMode::kUnset;
  }
  return frame_host->GetPage().virtual_keyboard_mode();
}

void ArkWebRenderWidgetHostViewOSRExt::SetVirtualKeyBoardArg(int32_t width,
                                                             int32_t height,
                                                             double keyboard) {
  LOG(DEBUG)
      << "ArkWebRenderWidgetHostViewOSRExt::SetVirtualKeyBoardArg ; width = "
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
                 << ",keyboard_rect_with_scale.width"
                 << keyboard_rect_with_scale.width()
                 << ",keyboard_rect_with_scale.height"
                 << keyboard_rect_with_scale.height();
    }
  }
  NotifyVirtualKeyboardOverlayRect(keyboard_rect_with_scale);
}
#endif  // #if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_SAME_LAYER)
void ArkWebRenderWidgetHostViewOSRExt::DidNativeEmbedEvent(
    const blink::mojom::NativeEmbedTouchEventPtr& touchEvent) {
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    ArkWebRenderHandlerExt::CefEmbedTouchEvent event{
        touchEvent->embedId,
        touchEvent->id,
        touchEvent->x,
        touchEvent->y,
        touchEvent->screenX,
        touchEvent->screenY,
        static_cast<ArkWebRenderHandlerExt::CefEmbedTouchType>(
            touchEvent->type),
        touchEvent->offsetX,
        touchEvent->offsetY};
    auto new_callback =
        base::BindOnce(&ArkWebRenderWidgetHostViewOSRExt::SetGestureEventResult,
                       weak_ptr_factory_.GetWeakPtr());
    CefRefPtr<CefGestureEventCallbackImpl> callbackPtr(
        new CefGestureEventCallbackImpl(std::move(new_callback)));
    handler->OnNativeEmbedGestureEvent(browser_impl_.get(), event,
                                       callbackPtr.get());
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnNativeEmbedLifecycleChange(
    const ArkWebRenderHandlerExt::CefNativeEmbedData& info) {
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnNativeEmbedLifecycleChange(browser_impl_.get(), info);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::SetGestureEventResult(
    bool result,
    bool stopPropagation) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->SetGestureEventResult(result,
                                                             stopPropagation);
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  if (handler) {
    handler->SetGestureEventResult(result);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::SetNativeEmbedMode(bool flag) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->input_router()->SetNativeEmbedMode(flag);
}

void ArkWebRenderWidgetHostViewOSRExt::OnNativeEmbedFirstFramePaint(
    const content::NativeEmbedFirstPaintEvent& event) {
  if (!browser_impl_) {
    return;
  }
  CefRefPtr<CefWebClientExtensionHandler> handler =
      browser_impl_->client()->AsArkWebClient()->GetWebClientExtensionHandler();
  CefRefPtr<CefNativeEmbedFirstFramePaintEvent> request(
      new AlloyNativeEmbedFirstFramePaintEvent(event));
  if (handler) {
    handler->OnNativeEmbedFirstFramePaint(request);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnNativeEmbedVisibilityChange(
    const std::string& embed_id,
    bool visibility) {
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebRenderHandlerExt> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnNativeEmbedVisibilityChange(embed_id, visibility);
  }
}
#endif

#if BUILDFLAG(ARKWEB_TOUCHPAD_FLING)
void ArkWebRenderWidgetHostViewOSRExt::SendTouchpadFlingEvent(
    blink::WebGestureEvent event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchpadFlingEvent");

  if (render_widget_host_ && render_widget_host_->GetView()) {
    if (ShouldRouteEvents()) {
      render_widget_host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          this, const_cast<blink::WebGestureEvent*>(&event), ui::LatencyInfo());
    } else {
      render_widget_host_->GetView()->ProcessGestureEvent(event,
                                                          ui::LatencyInfo());
    }
  }
}
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkWebRenderWidgetHostViewOSRExt::FilterScrollEventImpl(
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
  if (web_event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
    is_scroll_consumed_ = handler->AsArkWebRenderHandler()->FilterScrollEvent(
        browser_impl_.get(), web_event.data.scroll_update.delta_x,
        web_event.data.scroll_update.delta_y, 0, 0);
  } else if (web_event.GetType() ==
             blink::WebInputEvent::Type::kGestureFlingStart) {
    is_scroll_consumed_ = handler->AsArkWebRenderHandler()->FilterScrollEvent(
        browser_impl_.get(), 0, 0, web_event.data.fling_start.velocity_x,
        web_event.data.fling_start.velocity_y);
#if BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
    is_fling_ = true;
#endif
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnDidNavigateMainFrameToNewPage() {
  ResetGestureDetection(false);
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(IS_ARKWEB)
void ArkWebRenderWidgetHostViewOSRExt::OnRootLayerChanged() {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::OnScaleChanged(
    float old_page_scale_factor,
    float new_page_scale_factor) {
  if (browser_impl_.get()) {
    CefRefPtr<ArkWebDisplayHandlerExt> handler =
        browser_impl_->client()->GetDisplayHandler();
    CHECK(handler);
    handler->OnScaleChanged(
        browser_impl_.get(),
        std::round(old_page_scale_factor * SCALE_FACTOR_CONVERT_RATIO),
        std::round(new_page_scale_factor * SCALE_FACTOR_CONVERT_RATIO));
  }
}
#endif
#if BUILDFLAG(ARKWEB_MENU)
void ArkWebRenderWidgetHostViewOSRExt::MouseSelectMenuShow(bool show) {
  if (selection_controller_client_) {
    selection_controller_client_->MouseSelectMenuShow(show);
  }
}

void ArkWebRenderWidgetHostViewOSRExt::ChangeVisibilityOfQuickMenu() {
  if (selection_controller_client_) {
    selection_controller_client_->ChangeVisibilityOfQuickMenu();
  }
}
#endif

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
bool ArkWebRenderWidgetHostViewOSRExt::FilterInputEventForPullToRefresh(
    const blink::WebInputEvent& input_event) {
  if (overscroll_controller_ &&
      blink::WebInputEvent::IsGestureEventType(input_event.GetType())) {
    blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    if (overscroll_controller_->WillHandleGestureEvent(gesture_event)) {
      // Terminate an active fling when a GSU generated from the fling progress
      // (GSU with inertial state) is consumed by the overscroll_controller_ and
      // overscrolling mode is not |OVERSCROLL_NONE|. The early fling
      // termination generates a GSE which completes the overscroll action.
      if (gesture_event.GetType() ==
              blink::WebInputEvent::Type::kGestureScrollUpdate &&
          gesture_event.data.scroll_update.inertial_phase ==
              blink::WebGestureEvent::InertialPhaseState::kMomentum) {
        host_->StopFling();
      }
      return true;
    }
  }
  return false;
}

void ArkWebRenderWidgetHostViewOSRExt::CreateOverscrollControllerIfPossible() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExPullToRefresh)) {
    return;
  }

  // an OverscrollController is already set
  if (overscroll_controller_) {
    return;
  }

  content::RenderWidgetHostDelegate* delegate = host()->delegate();
  if (!delegate) {
    return;
  }

  content::RenderViewHostDelegateView* delegate_view =
      delegate->GetDelegateView();
  // render_widget_host_unittest.cc uses an object called
  // MockRenderWidgetHostDelegate that does not have a DelegateView
  if (!delegate_view) {
    return;
  }

  if (has_parent_) {
    return;
  }

#ifdef DISABLE_GPU
  if (!compositor_) {
    return;
  }
#else
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor(
      browser_impl_->GetAcceleratedWidget(is_popup_));
  if (!compositor) {
    return;
  }
#endif

  overscroll_controller_ =
      std::make_unique<content::OverscrollControllerOHOS>(this);
}

void ArkWebRenderWidgetHostViewOSRExt::OnFocusInternal() {
  if (overscroll_controller_) {
    overscroll_controller_->Enable();
  }
}

void ArkWebRenderWidgetHostViewOSRExt::LostFocusInternal() {
  if (overscroll_controller_) {
    overscroll_controller_->Disable();
  }
}

bool ArkWebRenderWidgetHostViewOSRExt::IsDisplayingInterstitial() {
  if (!host() || !host()->delegate()) {
    return false;
  }

  auto rvh_delegate_view = host()->delegate()->GetDelegateView();
  if (!rvh_delegate_view || !rvh_delegate_view->GetWebContents()) {
    return false;
  }

  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper =
          security_interstitials::SecurityInterstitialTabHelper::
              FromWebContents(rvh_delegate_view->GetWebContents());
  return security_interstitial_tab_helper &&
         security_interstitial_tab_helper->IsDisplayingInterstitial();
}

bool ArkWebRenderWidgetHostViewOSRExt::PullToRefreshAction(
    ui::PullToRefreshAction action) {
  if (!browser_impl_.get() || !browser_impl_->GetClient().get() ||
      IsDisplayingInterstitial()) {
    return false;
  }

  if (action == ui::PullToRefreshAction::PULL_START ? pull_to_refreshing_
                                                    : !pull_to_refreshing_) {
    return false;
  }

  bool result =
      browser_impl_->GetClient()->AsArkWebClient()->OnPullToRefreshAction(
          static_cast<int>(action));
  switch (action) {
    case ui::PullToRefreshAction::PULL_START: {
      pull_to_refreshing_ = result;
      root_layer_transform_ = GetRootLayer()->transform();
      pull_to_refresh_offset_x_ = pull_to_refresh_offset_y_ = 0;
      break;
    }
    case ui::PullToRefreshAction::PULL_REFRESH: {
      if (browser_impl_ && result) {
        browser_impl_->Reload();
      }
      break;
    }
    case ui::PullToRefreshAction::PULL_RESET: {
      pull_to_refreshing_ = false;
      pull_to_refresh_offset_x_ = pull_to_refresh_offset_y_ = 0;
      browser_impl_->GetClient()->AsArkWebClient()->OnPullToRefreshPull(0, 0);
      GetRootLayer()->SetTransform(root_layer_transform_);
      break;
    }
    case ui::PullToRefreshAction::UNKNOWN:
    case ui::PullToRefreshAction::PULL_CANCEL:
    case ui::PullToRefreshAction::PULL_RELEASE:
      break;
  }

  LOG(INFO) << __func__ << " [pulltorefresh] " << action << " " << result;
  return result;
}

void ArkWebRenderWidgetHostViewOSRExt::PullToRefreshUpdate(float x_delta,
                                                           float y_delta) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&ArkWebRenderWidgetHostViewOSRExt::PullToRefreshUpdate,
                       weak_ptr_factory_.GetWeakPtr(), x_delta, y_delta));
    return;
  }
  if (!enable_nweb_ex_ || !browser_impl_.get() ||
      !browser_impl_->GetClient().get()) {
    return;
  }
  pull_to_refresh_offset_x_ += x_delta;
  pull_to_refresh_offset_y_ += y_delta;
  browser_impl_->GetClient()->AsArkWebClient()->OnPullToRefreshPull(
      pull_to_refresh_offset_x_, pull_to_refresh_offset_y_);

  gfx::Transform root_layer_transform = GetRootLayer()->transform();
  root_layer_transform.PostTranslate(gfx::Vector2dF(0, y_delta));
  GetRootLayer()->SetTransform(root_layer_transform);
  LOG(DEBUG) << __func__
             << " [pulltorefresh] y_offset:" << pull_to_refresh_offset_y_;
}

void ArkWebRenderWidgetHostViewOSRExt::DidStopRefresh() {
  if (overscroll_controller_) {
    overscroll_controller_->DidStopRefresh();
  }
}
#endif  // BUILDFLAG(ARKWEB_PULL_TO_REFRESH)

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
void ArkWebRenderWidgetHostViewOSRExt::TransformPointToRootSurface(
    gfx::PointF* point) {
  *point += gfx::Vector2d(0, GetShrinkViewportHeight());
}

int ArkWebRenderWidgetHostViewOSRExt::GetTopControlsOffset() const {
  return (enable_nweb_ex_ ? top_controls_offset_ : 0);
}

int ArkWebRenderWidgetHostViewOSRExt::GetShrinkViewportHeight() {
  int shrink_viewport_height = 0;
  if (!enable_nweb_ex_ || !host()->delegate()) {
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

void ArkWebRenderWidgetHostViewOSRExt::OnTopControlsChanged(
    float top_controls_offset,
    float top_content_offset) {
  if (!enable_nweb_ex_ || !browser_impl_.get() ||
      !browser_impl_->GetClient().get()) {
    return;
  }
  browser_impl_->GetClient()->AsArkWebClient()->OnTopControlsChanged(
      top_controls_offset, top_content_offset);
  gfx::Transform root_layer_transform;
  root_layer_transform.Translate(gfx::Vector2dF(0, top_content_offset));
  GetRootLayer()->SetTransform(root_layer_transform);
}

void ArkWebRenderWidgetHostViewOSRExt::OnTopControlsHeightChanged() {
  if (auto rvh_delegate_view = host()->delegate()->GetDelegateView()) {
    GetRootLayer()->SetTopControlsHeight(
        rvh_delegate_view->GetTopControlsHeight() / GetDeviceScaleFactor());
  }
}
#endif  // BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void ArkWebRenderWidgetHostViewOSRExt::SetTextHandlesTemporarilyHiddenByDrag(
    bool hide_handles,
    bool dragging) {
  LOG(INFO) << "SetTextHandlesTemporarilyHiddenByDrag hide_handles:"
            << hide_handles << ", dragging:" << dragging;
  if (!dragging && selection_controller_) {
    selection_controller_->ResetResponsePendingInputEvent();
  }

  if (selection_controller_client_) {
    selection_controller_client_->AsArkWebTouchSelectionControllerClientOSRExt()
        ->HideHandleAndQuickMenuIfNecessary(hide_handles);
  }
}
#endif

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
void ArkWebRenderWidgetHostViewOSRExt::MaximizeResize() {
  auto compositor = CefRenderWidgetHostViewOSR::GetCompositor();
  if (compositor) {
    compositor->DisableSwapUntilMaximized();
  }
}

void ArkWebRenderWidgetHostViewOSRExt::RestoreRenderFit() {
  if (!browser_impl_ || !browser_impl_->client()) {
    LOG(ERROR) << "RestoreRenderFit get client failed.";
    return;
  }
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  if (handler && handler->AsArkWebRenderHandler()) {
    handler->AsArkWebRenderHandler()->RestoreRenderFit();
  }
}
#endif  // ARKWEB_MAXIMIZE_RESIZE
