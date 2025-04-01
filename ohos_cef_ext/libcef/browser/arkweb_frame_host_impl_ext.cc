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

#include "cef/libcef/browser/frame_host_impl.h"

#include "arkweb/build/features/features.h"
#include "cef/include/cef_request.h"
#include "cef/include/cef_stream.h"
#include "cef/include/cef_v8.h"
#include "cef/include/test/cef_test_helpers.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/net_service/browser_urlrequest_impl.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/url_util.h"
#include "cef/libcef/common/process_message_impl.h"
#include "cef/libcef/common/process_message_smr_impl.h"
#include "cef/libcef/common/request_impl.h"
#include "cef/libcef/common/string_util.h"
#include "cef/libcef/common/task_runner_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "cef/ohos_cef_ext/libcef/browser/arkweb_frame_host_impl_ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"

#if BUILDFLAG(IS_ARKWEB)
#include "cef/ohos_cef_ext/include/arkweb_frame_ext.h"
#endif
#include "content/public/browser/browsing_data_remover.h"

void ArkwebFrameHostExtImpl::PostURL(const CefString& url,
                                     const std::vector<char>& post_data) {
#if BUILDFLAG(ARKWEB_POST_URL)
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    "Content-Type:application/x-www-form-urlencoded", "POST",
                    post_data);
#endif
}

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
void ArkwebFrameHostExtImpl::LoadURLWithUserGesture(const CefString& url,
                                                    bool user_gesture) {
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    std::string()
#if BUILDFLAG(ARKWEB_POST_URL)
                        ,
                    std::string(), std::vector<char>()
#endif
                                       ,
                    user_gesture);
}
#endif

void ArkwebFrameHostExtImpl::LoadHeaderUrl(
    const CefString& url,
    const CefString& additionalHttpHeaders) {
#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  std::string refererValue = GetRefererValue(additionalHttpHeaders.ToString());
  content::Referrer referer;
  if (refererValue.empty()) {
    referer = content::Referrer();
  } else {
    GURL refererUrl = url_util::MakeGURL(refererValue, /*fixup=*/false);
    referer =
        content::Referrer(refererUrl, network::mojom::ReferrerPolicy::kDefault);
  }
  LoadURLWithExtras(url, referer, kPageTransitionExplicit,
                    additionalHttpHeaders);
#else
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    additionalHttpHeaders);
#endif
}

#if BUILDFLAG(ARKWEB_MEDIA)
void ArkwebFrameHostExtImpl::GetImagesCallback(
    CefRefPtr<CefFrameHostImpl> frame,
    CefRefPtr<CefGetImagesCallback> callback,
    bool response) {
  if (auto browser = frame->GetBrowser()) {
    callback->GetImages(response);
  }
}

void ArkwebFrameHostExtImpl::GetImagesWithResponse(
    cef::mojom::RenderFrame::GetImagesWithResponseCallback response_callback) {
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](cef::mojom::RenderFrame::GetImagesWithResponseCallback
                 response_callback,
             const RenderFrameType& render_frame) {
            render_frame->GetImagesWithResponse(std::move(response_callback));
          },
          std::move(response_callback)));
}
#endif  // BUILDFLAG(ARKWEB_MEDIA)

void ArkwebFrameHostExtImpl::GetImages(
    CefRefPtr<CefGetImagesCallback> callback) {
#if BUILDFLAG(ARKWEB_MEDIA)
  GetImagesWithResponse(base::BindOnce(
      &ArkwebFrameHostExtImpl::GetImagesCallback, base::Unretained(this),
      CefRefPtr<CefFrameHostImpl>(this), callback));
#endif  // BUILDFLAG(ARKWEB_MEDIA)
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkwebFrameHostExtImpl::SetInitialScale(float scale) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float scale, const RenderFrameType& render_frame) {
                          render_frame->SetInitialScale(scale);
                        },
                        scale));
}
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void ArkwebFrameHostExtImpl::PutZoomingForTextFactor(float factor) {
  CEF_REQUIRE_UIT();
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float factor, const RenderFrameType& render_frame) {
                          render_frame->PutZoomingForTextFactor(factor);
                        },
                        factor));
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
void ArkwebFrameHostExtImpl::SetJsOnlineProperty(bool network_up) {
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](bool network_up, const RenderFrameType& render_frame) {
            render_frame->SetJsOnlineProperty(network_up);
          },
          network_up));
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
#if BUILDFLAG(ARKWEB_CLIPBOARD)
void ArkwebFrameHostExtImpl::GetImageForContextNode(int command_id) {
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](int command_id, const RenderFrameType& render_frame) {
            render_frame->GetImageForContextNode(command_id);
          },
          command_id));
}

void ArkwebFrameHostExtImpl::OnGetImageForContextNode(
    cef::mojom::GetImageForContextNodeParamsPtr params,
    int command_id) {
  CefImageImpl* image_impl = new (std::nothrow) CefImageImpl();
  if (image_impl != nullptr && params->image.width() > 0 &&
      params->image.height() > 0) {
    image_impl->AddBitmap(1.0, params->image);
  } else {
    return;
  }
  CefRefPtr<CefImage> image(image_impl);
  CefRefPtr<CefContextMenuHandler> handler = nullptr;
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (handler) {
    handler->AsCefContextMenuHandlerExt()->OnGetImageForContextNode(
        GetBrowser(), image, command_id);
  }
}

void ArkwebFrameHostExtImpl::OnGetImageForContextNodeNull(int command_id) {
  CefRefPtr<CefImage> image(new CefImageImpl());
  CefRefPtr<CefContextMenuHandler> handler = nullptr;
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (handler) {
    handler->AsCefContextMenuHandlerExt()->OnGetImageForContextNode(
        GetBrowser(), image, command_id);
  }
}
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)

void ArkwebFrameHostExtImpl::OnGetImageFromCache(
    std::string url,
    int command_id,
    uint32_t buffer_size,
    base::ReadOnlySharedMemoryRegion region) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  CefRefPtr<CefContextMenuHandler> handler;
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (!handler) {
    return;
  }

  CefImageImpl* image_impl = new (std::nothrow) CefImageImpl();
  if (!region.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory region is invalid";
    handler->AsCefContextMenuHandlerExt()->OnGetImageFromCache(image_impl,
                                                               command_id);
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(0, buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory mapping is invalid";
    handler->AsCefContextMenuHandlerExt()->OnGetImageFromCache(image_impl,
                                                               command_id);
    return;
  }

  uint8_t* buffer = (uint8_t*)(mapping.memory());
  sk_sp<SkData> sk_data = SkData::MakeWithoutCopy(mapping.data(), buffer_size);
  if (sk_data) {
    sk_sp<SkImage> sk_image = SkImages::DeferredFromEncodedData(sk_data);
    if (sk_image) {
      SkBitmap bitmap;
      sk_image->asLegacyBitmap(&bitmap);
      image_impl->AddBitmap(1.0, bitmap);
    }
  }
  handler->AsCefContextMenuHandlerExt()->OnGetImageFromCache(image_impl,
                                                             command_id);
}

#if BUILDFLAG(ARKWEB_NWEB_EX)
void ArkwebFrameHostExtImpl::OnGetImageFromCacheEx(
    std::string url,
    int command_id,
    uint32_t buffer_size,
    base::ReadOnlySharedMemoryRegion region) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  CefRefPtr<CefContextMenuHandler> handler;
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (!handler) {
    return;
  }

  if (!region.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory region is invalid";
    handler->AsCefContextMenuHandlerExt()->OnGetImageFromCacheEx(command_id,
                                                                 nullptr, 0);
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(0, buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory mapping is invalid";
    handler->AsCefContextMenuHandlerExt()->OnGetImageFromCacheEx(command_id,
                                                                 nullptr, 0);
    return;
  }
  uint8_t* buffer = (uint8_t*)(mapping.memory());

  LOG(DEBUG) << "OnGetImageFromCacheEx invoke";
  handler->AsCefContextMenuHandlerExt()->OnGetImageFromCacheEx(
      command_id, buffer, buffer_size);
}
#endif

#if BUILDFLAG(ARKWEB_TERMINATE_RENDER)
void ArkwebFrameHostExtImpl::TerminateRenderProcess(bool& result) {
  LOG(INFO) << "TerminateRenderProcess in CefFrameHostImpl start, getpid:"
            << getpid();
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->TerminateRenderProcess();
                    }));
  result = true;
}
#endif

#if BUILDFLAG(ARKWEB_SYNC_RENDER)
void ArkwebFrameHostExtImpl::UpdateDrawRect() {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->UpdateDrawRect();
                    }));
}
#endif

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void ArkwebFrameHostExtImpl::ScrollToWithAnime(float x,
                                               float y,
                                               int32_t duration) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float x, float y, int32_t duration,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollToWithAnime(
                                            x, y, duration);
                                      },
                                      x, y, duration));
}

void ArkwebFrameHostExtImpl::ScrollByWithAnime(float delta_x,
                                               float delta_y,
                                               int32_t duration) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float delta_x, float delta_y, int32_t duration,
                           const RenderFrameType& render_frame) {
                          render_frame->ScrollByWithAnime(delta_x, delta_y,
                                                          duration);
                        },
                        delta_x, delta_y, duration));
}

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void ArkwebFrameHostExtImpl::GetOverScrollOffset(float* offset_x,
                                                 float* offset_y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float* offset_x, float* offset_y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->GetOverScrollOffset(
                                            offset_x, offset_y);
                                      },
                                      offset_x, offset_y));
}
#endif

void ArkwebFrameHostExtImpl::ScrollTo(float x, float y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float x, float y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollTo(x, y);
                                      },
                                      x, y));
}

void ArkwebFrameHostExtImpl::ScrollBy(float delta_x, float delta_y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float delta_x, float delta_y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollBy(delta_x,
                                                               delta_y);
                                      },
                                      delta_x, delta_y));
}

void ArkwebFrameHostExtImpl::SlideScroll(float vx, float vy) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float vx, float vy,
                                         const RenderFrameType& render_frame) {
                                        render_frame->SlideScroll(vx, vy);
                                      },
                                      vx, vy));
}

void ArkwebFrameHostExtImpl::ZoomBy(float delta, float width, float height) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float delta, float width, float height,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ZoomBy(delta, width,
                                                             height);
                                      },
                                      delta, width, height));
}

void ArkwebFrameHostExtImpl::GetHitData(int& type, CefString& extra_data) {
  std::string temp_extra_data;
  if (is_temporary()) {
    extra_data = temp_extra_data;
    return;
  } else if (!render_frame_host_) {
    extra_data = temp_extra_data;
    return;
  }

  if (!render_frame_.is_bound()) {
    extra_data = temp_extra_data;
    return;
  }
  render_frame_->GetHitData(&type, &temp_extra_data);
  extra_data = temp_extra_data;
}

void ArkwebFrameHostExtImpl::SetOverscrollMode(int mode) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](int mode, const RenderFrameType& render_frame) {
                          render_frame->SetOverscrollMode(mode);
                        },
                        mode));
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#if BUILDFLAG(ARKWEB_PAGE_UP_DOWN)
void ArkwebFrameHostExtImpl::ScrollPageUpDown(bool is_up,
                                              bool is_half,
                                              float view_height) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](bool is_up, bool is_half, float view_height,
                           const RenderFrameType& render_frame) {
                          render_frame->ScrollPageUpDown(is_up, is_half,
                                                         view_height);
                        },
                        is_up, is_half, view_height));
}

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void ArkwebFrameHostExtImpl::GetScrollOffset(float* offset_x, float* offset_y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float* offset_x, float* offset_y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->GetScrollOffset(offset_x,
                                                                      offset_y);
                                      },
                                      offset_x, offset_y));
}
#endif
#endif  // #ARKWEB_PAGE_UP_DOWN

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
void ArkwebFrameHostExtImpl::ShouldOverrideUrlLoading(
    const std::string& url,
    const std::string& request_method,
    bool user_gesture,
    bool is_redirect,
    bool is_outermost_main_frame,
    cef::mojom::BrowserFrame::ShouldOverrideUrlLoadingCallback callback) {
  bool override = false;
  CefRefPtr<CefBrowserHostBase> browser_host = GetBrowserHostBase();
  if (browser_host == nullptr) {
    std::move(callback).Run(override);
    return;
  }

  if (auto client = browser_host->GetClient()) {
    if (auto handler = client->GetRequestHandler()) {
      override = handler->AsCefRequestHandlerExt()->ShouldOverrideUrlLoading(
          browser_host.get(), url, request_method, user_gesture, is_redirect,
          is_outermost_main_frame);
    }
  }
  std::move(callback).Run(override);
}
#endif

void ArkwebFrameHostExtImpl::RemoveCache(bool include_disk_files) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->RemoveCache();
                    }));

  if (include_disk_files) {
    auto browser = GetBrowserHostBase();
    if (!browser) {
      LOG(ERROR) << "RemoveCache: browser is null";
      return;
    }

    auto web_contents = browser->GetWebContents();
    if (!web_contents) {
      LOG(ERROR) << "RemoveCache: web contents is null";
      return;
    }

    content::BrowsingDataRemover* remover =
        web_contents->GetBrowserContext()->GetBrowsingDataRemover();
    remover->Remove(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
  }
}
