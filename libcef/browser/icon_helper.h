// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ICON_HELPER_H_
#define CEF_LIBCEF_BROWSER_ICON_HELPER_H_
#pragma once

#include <vector>
#include <queue>

#include "include/cef_base.h"
#include "libcef/browser/thread_util.h"
#include "ohos_nweb/src/capi/nweb_icon_size.h"

#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OHOS_WPT)
#include <unordered_set>
#include "content/public/browser/navigation_handle.h"
#endif  // defined(OHOS_WPT)

namespace gfx {
class Size;
}  // namespace gfx

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

class CefBrowser;
class CefDisplayHandler;
class GURL;

class IconHelper : public virtual CefBaseRefCounted {
 public:
  IconHelper() = default;
  ~IconHelper() { web_contents_ = nullptr; }

  IconHelper(const IconHelper&) = delete;
  IconHelper& operator=(const IconHelper&) = delete;

  void SetDisplayHandler(const CefRefPtr<CefDisplayHandler>& handler);
  void SetWebContents(content::WebContents* new_contents);
  void OnUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates,
      const CefRefPtr<CefBrowser>& browser);
  void DownloadFavicon(const blink::mojom::FaviconURLPtr& candidate, int request_id, const CefRefPtr<CefBrowser>& browser);
  void DownloadFaviconCallback(
      int request_id,
      const CefRefPtr<CefBrowser>& browser,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);
  void DownloadFaviconHandler(
      const GURL& image_url,
      const SkBitmap& bitmaps,
      const CefRefPtr<CefBrowser>& browser);
  void OnReceivedIcon(const void* data,
                      size_t width,
                      size_t height,
                      cef_color_type_t color_type,
                      cef_alpha_type_t alpha_type,
                      const CefRefPtr<CefBrowser>& browser);

  void OnReceivedIconUrl(const CefString& image_url,
                         const void* data,
                         size_t width,
                         size_t height,
                         cef_color_type_t color_type,
                         cef_alpha_type_t alpha_type,
                         const CefRefPtr<CefBrowser>& browser);

#if defined(OHOS_WPT)
  void ClearFailedFaviconUrlSets(content::NavigationHandle* navigation_handle);
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
  void SetMainFrameDocumentOnLoadCompleted(bool complete);
  void SetLastPageUrl(const GURL& url);
#endif  // defined(OHOS_FAVICON)

#if defined(OHOS_NWEB_EX)
  struct CallbackData {
    float score;
    GURL image_url;
    SkBitmap bitmap;
 
    CallbackData() {}
 
    CallbackData(float score_num, const GURL& url,
                 const SkBitmap& bmp)
        : score(score_num), image_url(url),
          bitmap(bmp) {}
  };
#endif

 private:
#if defined(OHOS_WPT)
  void InsertFailedFaviconUrl(const GURL& icon_url);
  bool CheckFailedFaviconUrl(const GURL& icon_url);
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
  bool ShouldAbort();

  bool document_on_load_completed_ = false;
  GURL last_page_url_;
#endif  // defined(OHOS_FAVICON)

  content::WebContents* web_contents_ = nullptr;
  CefRefPtr<CefDisplayHandler> handler_ = nullptr;
  SkBitmap bitmap_;

#if defined(OHOS_WPT)
  std::unordered_set<size_t> failed_favicon_urls_set_;
#endif  // defined(OHOS_WPT)

#if defined(OHOS_NWEB_EX)
  std::map<int, std::unordered_set<std::string>> pending_downloads_map_;
  std::map<int, CallbackData> best_results_map_;
  std::mutex mutex_;
#endif

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(IconHelper);
};

#endif  // CEF_LIBCEF_BROWSER_ICON_HELPER_H_
