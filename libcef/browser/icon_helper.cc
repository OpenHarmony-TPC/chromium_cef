// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/icon_helper.h"

#include "include/cef_display_handler.h"

#include "base/logging.h"
#include "components/favicon_base/select_favicon_frames.h"
#ifdef OHOS_NAVIGATION
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#endif
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

#if defined(OHOS_WPT)
#include "base/hash/hash.h"
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
#include "content/public/browser/navigation_handle.h"
#endif  // defined(OHOS_FAVICON)

namespace {

constexpr int LARGEST_ICON_SIZE = 192;

cef_color_type_t TransformSkColorType(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      return CEF_COLOR_TYPE_RGBA_8888;
    case kBGRA_8888_SkColorType:
      return CEF_COLOR_TYPE_BGRA_8888;
    default:
      return CEF_COLOR_TYPE_UNKNOWN;
  }
}

cef_alpha_type_t TransformSkAlphaType(SkAlphaType alpha_type) {
  switch (alpha_type) {
    case kOpaque_SkAlphaType:
      return CEF_ALPHA_TYPE_OPAQUE;
    case kPremul_SkAlphaType:
      return CEF_ALPHA_TYPE_PREMULTIPLIED;
    case kUnpremul_SkAlphaType:
      return CEF_ALPHA_TYPE_POSTMULTIPLIED;
    default:
      return CEF_ALPHA_TYPE_UNKNOWN;
  }
}

}  // namespace

#ifdef OHOS_NAVIGATION
namespace content {
struct FaviconStatus;
}
#endif

void IconHelper::SetDisplayHandler(
    const CefRefPtr<CefDisplayHandler>& handler) {
  handler_ = handler;
}

void IconHelper::SetBrowser(const CefRefPtr<CefBrowser>& browser) {
  browser_ = browser;
}

void IconHelper::SetWebContents(content::WebContents* new_contents) {
  web_contents_ = new_contents;
}

void IconHelper::OnUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (!handler_ || !browser_) {
    LOG(ERROR) << "Initialized value is invalid";
    return;
  }
  for (const auto& candidate : candidates) {
#if defined(OHOS_WPT)
    if (!candidate->icon_url.is_valid() ||
        CheckFailedFaviconUrl(candidate->icon_url)) {
#else
    if (!candidate->icon_url.is_valid()) {
#endif  // defined(OHOS_WPT)
      continue;
    }
    switch (candidate->icon_type) {
      case blink::mojom::FaviconIconType::kFavicon:
        DownloadFavicon(candidate);
        break;
      case blink::mojom::FaviconIconType::kTouchIcon:
        handler_->OnReceivedTouchIconUrl(
            browser_, CefString(candidate->icon_url.spec()), false);
        break;
      case blink::mojom::FaviconIconType::kTouchPrecomposedIcon:
        handler_->OnReceivedTouchIconUrl(
            browser_, CefString(candidate->icon_url.spec()), true);
        break;
      case blink::mojom::FaviconIconType::kInvalid:
        LOG(INFO) << "An invalid icon type.";
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void IconHelper::DownloadFavicon(const blink::mojom::FaviconURLPtr& candidate) {
  if (!web_contents_) {
    LOG(ERROR) << "WebContents is invalid";
    return;
  }
  web_contents_->DownloadImage(
      candidate->icon_url,
      true,               // Is a favicon
      gfx::Size(),        // No preferred size
      LARGEST_ICON_SIZE,  // Max bitmap size 192
      false,              // Normal cache policy
      base::BindOnce(&IconHelper::DownloadFaviconCallback, this));
}

void IconHelper::DownloadFaviconCallback(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (http_status_code == 404) {
#if defined(OHOS_WPT)
    InsertFailedFaviconUrl(image_url);
#endif  // defined(OHOS_WPT)
    return;
  }

  if (bitmaps.size() == 0) {
    return;
  }

#if defined(OHOS_FAVICON)
  if (ShouldAbort()) {
    return;
  }
#endif  // defined(OHOS_FAVICON)

  std::vector<size_t> best_indices;
  SelectFaviconFrameIndices(original_bitmap_sizes,
                            std::vector<int>(1U, LARGEST_ICON_SIZE),
                            &best_indices, nullptr);
  const auto& bitmap =
      bitmaps[best_indices.size() == 0 ? 0 : best_indices.front()];
  if (bitmap.drawsNothing()) {
    return;
  }
  std::unique_ptr<SkBitmap> bitmap_ptr =std::make_unique<SkBitmap>(bitmap_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
    FROM_HERE,
    base::BindOnce([](std::unique_ptr<SkBitmap> ptr){}, std::move(bitmap_ptr)),
    base::Seconds(5)
  );
  bitmap_ = bitmap;
  auto ret = bitmap_.writePixels(bitmap.pixmap());
  if (!ret) {
    LOG(ERROR) << "Copy pixels failed.";
    return;
  }
  void* pixels = bitmap_.getPixels();
  size_t byte_size = bitmap_.computeByteSize();
  if (!pixels || byte_size == 0) {
    LOG(ERROR) << "Pixels value is invalid.";
    return;
  }
  auto color_type = TransformSkColorType(bitmap_.colorType());
  auto alpha_type = TransformSkAlphaType(bitmap_.alphaType());
  size_t pixel_width = bitmap_.width();
  size_t pixel_height = bitmap_.height();

#ifdef OHOS_NAVIGATION
  if (web_contents_) {
    content::NavigationEntry* entry =
        web_contents_->GetController().GetLastCommittedEntry();
    if (entry) {
      entry->GetFavicon().valid = true;
      entry->GetFavicon().url = image_url;
      entry->GetFavicon().image = gfx::Image::CreateFrom1xBitmap(bitmap_);
    }
  }
#endif

  OnReceivedIcon(pixels, pixel_width, pixel_height, color_type, alpha_type);
  OnReceivedIconUrl(CefString(image_url.spec()), pixels, pixel_width,
                    pixel_height, color_type, alpha_type);
}

void IconHelper::OnReceivedIcon(const void* data,
                                size_t width,
                                size_t height,
                                cef_color_type_t color_type,
                                cef_alpha_type_t alpha_type) {
  if (!handler_ || !browser_) {
    LOG(ERROR) << "Initialized value is invalid";
    return;
  }
  handler_->OnReceivedIcon(data, width, height, color_type, alpha_type);
}

void IconHelper::OnReceivedIconUrl(const CefString& image_url,
                                   const void* data,
                                   size_t width,
                                   size_t height,
                                   cef_color_type_t color_type,
                                   cef_alpha_type_t alpha_type) {
  if (!handler_ || !browser_) {
    LOG(ERROR) << "Initialized value is invalid";
    return;
  }
  handler_->OnReceivedIconUrl(image_url, data, width, height, color_type,
                              alpha_type);
}

#if defined(OHOS_WPT)
void IconHelper::InsertFailedFaviconUrl(const GURL& icon_url) {
  size_t url_hash = base::FastHash(icon_url.spec());
  failed_favicon_urls_set_.insert(url_hash);
}

bool IconHelper::CheckFailedFaviconUrl(const GURL& icon_url) {
  size_t url_hash = base::FastHash(icon_url.spec());
  return failed_favicon_urls_set_.find(url_hash) !=
         failed_favicon_urls_set_.end();
}

void IconHelper::ClearFailedFaviconUrlSets(
    content::NavigationHandle* navigation) {
  if (navigation && navigation->IsInPrimaryMainFrame() &&
      navigation->GetReloadType() == content::ReloadType::BYPASSING_CACHE) {
    failed_favicon_urls_set_.clear();
  }
}
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
// should abort fetching favicon or not.
bool IconHelper::ShouldAbort() {
  if (!web_contents_) {
    return true;
  }

  GURL page_url = web_contents_->GetURL();
  if (!document_on_load_completed_) {
    LOG(DEBUG) << "should abort fetching favicon, document load not completed,"
              << " page_url: " << page_url;
    return true;
  }

  if (last_page_url_ != web_contents_->GetURL()) {
    LOG(DEBUG) << "should abort fetching favicon, page urls not equal,"
              << " last_page_url_:" << last_page_url_
              << ", page_url:" << page_url;
    return true;
  }

  return false;
}

void IconHelper::SetMainFrameDocumentOnLoadCompleted(bool complete) {
  document_on_load_completed_ = complete;
}

void IconHelper::SetLastPageUrl(const GURL& url) {
  last_page_url_ = url;
}
#endif  // defined(OHOS_FAVICON)
