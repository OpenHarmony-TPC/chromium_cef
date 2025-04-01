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

#include <signal.h>

#include "arkweb/build/features/features.h"
#include "build/build_config.h"
#include "cef/libcef/renderer/frame_impl.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#if BUILDFLAG(ARKWEB_TERMINATE_RENDER)
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#endif
#include "cef/include/cef_urlrequest.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/http_header_utils.h"
#include "cef/libcef/common/process_message_impl.h"
#include "cef/libcef/common/process_message_smr_impl.h"
#include "cef/libcef/common/request_impl.h"
#include "cef/libcef/common/string_util.h"
#include "cef/libcef/renderer/blink_glue.h"
#include "cef/libcef/renderer/browser_impl.h"
#include "cef/libcef/renderer/dom_document_impl.h"
#include "cef/libcef/renderer/render_frame_util.h"
#include "cef/libcef/renderer/thread_util.h"
#include "cef/libcef/renderer/v8_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#if BUILDFLAG(IS_ARKWEB)
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#endif
#if BUILDFLAG(ARKWEB_CLIPBOARD)
#include "base/process/process_metrics.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
#include "cef/ohos_cef_ext/libcef/renderer/arkweb_frame_impl_ext.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#if BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#endif  // BUILDFLAG(ARKWEB_NETWORK_CONNINFO)

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
static int DEFAULT_SCROLL_ANIMATION_DURATION_MILLISEC = 600;
static double POSITION_RATIO = 138.9;
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
#include "third_party/blink/public/platform/web_cache.h"
#endif

#if BUILDFLAG(IS_OHOS)
void ArkwebFrameExtImpl::GetImages(CefRefPtr<CefGetImagesCallback> callback) {
  NOTREACHED() << "GetImages cannot be called from the renderer process";
}

void ArkwebFrameExtImpl::LoadHeaderUrl(const CefString& url,
                                       const CefString& additionalHttpHeaders) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_) {
    return;
  }

  auto params = cef::mojom::RequestParams::New();
  params->url = GURL(url.ToString());
  params->method = "GET";
  params->headers = additionalHttpHeaders;
  LoadRequest(std::move(params));
}

void ArkwebFrameExtImpl::PostURL(const CefString& url,
                                 const std::vector<char>& post_data) {
  // todo(ohos) : impl this function then remove todo
}

void ArkwebFrameExtImpl::LoadURLWithUserGesture(const CefString& url,
                                                bool user_gesture) {
  // todo(ohos) : impl this function then remove todo
}
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
float computeSlidePosition(float v) {
  return (v * POSITION_RATIO / 1000);
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

void ArkwebFrameExtImpl::SetInitialScale(float initialScale) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(INFO) << "SetInitialScale webview is NULL";
    return;
  }
  webview->SetInitialPageScaleOverride(initialScale);
}

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void ArkwebFrameExtImpl::PutZoomingForTextFactor(float factor) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();

  if (!webview) {
    return;
  }
  // Hide selection and autofill popups.
  webview->CancelPagePopup();

  render_frame->GetWebFrame()->FrameWidget()->SetTextZoomFactor(factor);
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
void ArkwebFrameExtImpl::SetJsOnlineProperty(bool network_up) {
  LOG(INFO) << "SetJsOnlineProperty:" << network_up;
  blink::WebNetworkStateNotifier::SetOnLine(network_up);
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_CONNINFO)

#if BUILDFLAG(ARKWEB_CLIPBOARD)
const int kMaxContextImageNodeSizeIfDownScale = 1024;
// 2GB
const int kNeedImageDownScaleSysMemKB = 2097152;

int GetSystemTotalMem() {
  base::SystemMemoryInfoKB meminfo;
  if (base::GetSystemMemoryInfo(&meminfo)) {
    return meminfo.total;
  }
  LOG(WARNING) << "Get sys meminfo failed";
  return -1;
}

bool NeedsDownscale(const gfx::Size& original_image_size, int total_mem) {
  if (total_mem > kNeedImageDownScaleSysMemKB) {
    return false;
  }
  if (original_image_size.width() <= kMaxContextImageNodeSizeIfDownScale &&
      original_image_size.height() <= kMaxContextImageNodeSizeIfDownScale) {
    return false;
  }
  return true;
}

SkBitmap Downscale(const SkBitmap& image, int total_mem) {
  if (image.isNull()) {
    return SkBitmap();
  }

  gfx::Size image_size(image.width(), image.height());
  if (!NeedsDownscale(image_size, total_mem)) {
    return image;
  }

  gfx::SizeF scaled_size = gfx::SizeF(image_size);

  if (scaled_size.width() > kMaxContextImageNodeSizeIfDownScale) {
    scaled_size.Scale(kMaxContextImageNodeSizeIfDownScale /
                      scaled_size.width());
  }

  if (scaled_size.height() > kMaxContextImageNodeSizeIfDownScale) {
    scaled_size.Scale(kMaxContextImageNodeSizeIfDownScale /
                      scaled_size.height());
  }

  return skia::ImageOperations::Resize(image,
                                       skia::ImageOperations::RESIZE_GOOD,
                                       static_cast<int>(scaled_size.width()),
                                       static_cast<int>(scaled_size.height()));
}

void ArkwebFrameExtImpl::GetImageForContextNode(int command_id) {
  if (!frame_) {
    LOG(ERROR) << "GetImageForContextNode frame is nullptr";
    return;
  }
  if (total_mem_ == -1) {
    total_mem_ = GetSystemTotalMem();
  }

  cef::mojom::GetImageForContextNodeParamsPtr params =
      cef::mojom::GetImageForContextNodeParams::New();
  blink::WebNode context_node;
  if (frame_->View() && frame_->View()->FocusedFrame()) {
    context_node = frame_->View()->FocusedFrame()->ContextMenuNode();
  } else {
    context_node = frame_->ContextMenuNode();
  }
  std::vector<uint8_t> image_data;
  gfx::Size original_size;
  std::string image_extension;

  if (context_node.IsNull() || !context_node.IsElementNode() ||
      context_node.To<blink::WebElement>().ImageContents().drawsNothing()) {
    LOG(WARNING) << "Context node is null or is not element node, or image "
                    "draws nothing";
    SendToBrowserFrame(
        __FUNCTION__,
        base::BindOnce(
            [](cef::mojom::GetImageForContextNodeParamsPtr data, int command_id,
               const BrowserFrameType& browser_frame) {
              browser_frame->OnGetImageForContextNodeNull(command_id);
            },
            std::move(params), command_id));
    return;
  }

  blink::WebElement web_element = context_node.To<blink::WebElement>();
  original_size = web_element.GetImageSize();

  SkBitmap image = web_element.ImageContents();
  SkBitmap resize_image = Downscale(image, total_mem_);
  image_extension = "." + web_element.ImageExtension();
  params->width = resize_image.width();
  params->height = resize_image.height();
  params->image = resize_image;
  params->image_extension = image_extension;
  LOG(DEBUG) << "GetImageForContextNode, image width: " << resize_image.width()
             << ", height: " << resize_image.height();
  SendToBrowserFrame(
      __FUNCTION__,
      base::BindOnce(
          [](cef::mojom::GetImageForContextNodeParamsPtr data, int command_id,
             const BrowserFrameType& browser_frame) {
            browser_frame->OnGetImageForContextNode(std::move(data),
                                                    command_id);
          },
          std::move(params), command_id));
}
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_TERMINATE_RENDER)
void ArkwebFrameExtImpl::TerminateRenderProcess() {
  base::ProcessId realPid = base::GetCurrentRealPid();
  LOG(INFO) << "TerminateRenderProcess start in render side, pid: " << getpid()
            << ", propid: " << realPid;
  // try to kill render process by pid
  if (kill(getpid(), SIGTERM) != 0) {
    LOG(ERROR) << "Unable to terminate pid: " << getpid();
    return;
  }
  // if not, kill by procpid
  if (kill(realPid, SIGTERM) != 0) {
    LOG(ERROR) << "Unable to terminate getprocpid: " << realPid;
    return;
  }
  LOG(INFO) << "TerminateRenderProcess end in render side";
}
#endif

#if BUILDFLAG(ARKWEB_SYNC_RENDER)
void ArkwebFrameExtImpl::UpdateDrawRect() {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "SetDrawRect get webview failed";
    return;
  }
  webview->UpdateDrawRect();
}
#endif

#if BUILDFLAG(ARKWEB_MEDIA)
void ArkwebFrameExtImpl::GetImagesWithResponse(
    cef::mojom::RenderFrame::GetImagesWithResponseCallback callback) {
  ExecuteOnLocalFrame(
      __FUNCTION__,
      base::BindOnce(
          [](cef::mojom::RenderFrame::GetImagesWithResponseCallback callback,
             blink::WebLocalFrame* frame) {
            blink::WebElementCollection collection =
                frame->GetDocument().GetElementsByHTMLTagName("img");
            DCHECK(!collection.IsNull());
            bool response = !(collection.FirstItem()).IsNull();
            std::move(callback).Run(response);
          },
          std::move(callback)));
}
#endif  // BUILDFLAG(ARKWEB_MEDIA)

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkwebFrameExtImpl::ScrollByWithAnime(float delta_x,
                                           float delta_y,
                                           int32_t duration) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "scrollbywithanime get webview failed";
    return;
  }
  auto scroll_offset = webview->GetScrollOffset();
  webview->SmoothScroll(delta_x + scroll_offset.x(),
                        delta_y + scroll_offset.y(),
                        base::Milliseconds(duration));
}

void ArkwebFrameExtImpl::ScrollToWithAnime(float x, float y, int32_t duration) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "scrolltowithanime get webview failed";
    return;
  }
  webview->SmoothScroll(x, y, base::Milliseconds(duration));
}

void ArkwebFrameExtImpl::ScrollTo(float x, float y) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "scrollto get webview failed";
    return;
  }
  webview->SetScrollOffset(gfx::PointF(x, y));
}

void ArkwebFrameExtImpl::ScrollBy(float delta_x, float delta_y) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "scrollby get webview failed";
    return;
  }
  auto scroll_offset = webview->GetScrollOffset();
  webview->SetScrollOffset(
      gfx::PointF(delta_x + scroll_offset.x(), delta_y + scroll_offset.y()));
}

void ArkwebFrameExtImpl::SlideScroll(float vx, float vy) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webview = render_frame->GetWebView();
  if (!webview) {
    LOG(ERROR) << "scrollby get webview failed";
    return;
  }
  float dx = vx == 0 ? 0 : computeSlidePosition(vx);
  float dy = vy == 0 ? 0 : computeSlidePosition(vy);
  auto scroll_offset = webview->GetScrollOffset();
  webview->SmoothScroll(
      dx + scroll_offset.x(), dy + scroll_offset.y(),
      base::Milliseconds(DEFAULT_SCROLL_ANIMATION_DURATION_MILLISEC));
}

void ArkwebFrameExtImpl::ZoomBy(float delta, float width, float height) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  render_frame->SetZoomLevel(delta, gfx::Point(width / 2, height / 2));
}

void ArkwebFrameExtImpl::SetOverscrollMode(int mode) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  render_frame->SetOverscrollMode(mode);
}

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void ArkwebFrameExtImpl::GetScrollOffset(
    cef::mojom::RenderFrame::GetScrollOffsetCallback callback) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());
  blink::WebView* webView = render_frame->GetWebView();
  if (!webView) {
    LOG(ERROR) << "GetScrollOffset get webView failed";
    return;
  }
  auto scroll_offset = webView->GetScrollOffset();
  std::move(callback).Run(scroll_offset.x(), scroll_offset.y());
}

void ArkwebFrameExtImpl::GetOverScrollOffset(
    cef::mojom::RenderFrame::GetOverScrollOffsetCallback callback) {
  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  DCHECK(render_frame->IsMainFrame());

  auto overScroll_offset = render_frame->GetOverScrollOffset();
  std::move(callback).Run(overScroll_offset.x(), overScroll_offset.y());
}
#endif
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
bool ArkwebFrameExtImpl::ShouldOverrideUrlLoading(
    const CefString& url,
    const CefString& request_method,
    bool user_gesture,
    bool is_redirect,
    bool is_outermost_main_frame) {
  bool override = false;
  if (auto& browser_frame = CefFrameImpl::GetBrowserFrame()) {
    browser_frame->ShouldOverrideUrlLoading(
        url.ToString(), request_method.ToString(), user_gesture, is_redirect,
        is_outermost_main_frame, &override);
  }

  return override;
}
#endif

#if BUILDFLAG(ARKWEB_SCROLLBAR)
void ArkwebFrameExtImpl::UpdatePixelRatio(float ratio) {
  LOG(INFO) << "UpdatePixelRatio in render side SetPixelRatio:" << ratio;
  base::ohos::SetPixelRatio(ratio);
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
void ArkwebFrameExtImpl::RemoveCache() {
  blink::WebCache::Clear();
}
#endif
