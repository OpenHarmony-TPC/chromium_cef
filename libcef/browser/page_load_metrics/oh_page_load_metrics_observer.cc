// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/page_load_metrics/oh_page_load_metrics_observer.h"

#include <string>

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"

#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/renderer/render_frame.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/first_meaningful_paint_details_impl.h"
#include "libcef/browser/largest_contentful_paint_details_impl.h"
#include "libcef/common/app_manager.h"
#include "libcef/renderer/browser_impl.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/gurl.h"

OhPageLoadMetricsObserver::OhPageLoadMetricsObserver() {
  network_quality_tracker_ = g_browser_process->network_quality_tracker();
  DCHECK(network_quality_tracker_);
}

OhPageLoadMetricsObserver::ObservePolicy OhPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OhPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class uses the event OnLoadedResource, but only uses the one with
  // network::mojom::RequestDestination::kDocument, which never occur in
  // FencedFrames' navigation. So, we can use STOP_OBSERVING.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OhPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OhPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // We continue observing after being backgrounded, in case we are foregrounded
  // again without being killed. In those cases we may still report non-buffered
  // metrics such as FCP after being re-foregrounded.
  ReportLargestContentfulPaint(timing);
  return CONTINUE_OBSERVING;
}

OhPageLoadMetricsObserver::ObservePolicy OhPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportLargestContentfulPaint(timing);
  return CONTINUE_OBSERVING;
}

void OhPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportLargestContentfulPaint(timing);
}

void OhPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_contentful_paint_ms =
      timing.paint_timing->first_contentful_paint->InMilliseconds();
  ReportFirstContentfulPaint(
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds(),
      first_contentful_paint_ms);
}

void OhPageLoadMetricsObserver::ReportFirstContentfulPaint(
    int64_t navigation_start_tick,
    int64_t first_contentful_paint_ms) {
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(
          GetDelegate().GetWebContents());
  if (!browser.get()) {
    return;
  }
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get()) {
    return;
  }
  CefRefPtr<CefLoadHandler> load_handler = client->GetLoadHandler();
  if (!load_handler.get()) {
    return;
  }
  load_handler->OnFirstContentfulPaint(navigation_start_tick,
                                       first_contentful_paint_ms);
}

void OhPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_meaningful_paint_time = 0;
  if (timing.paint_timing->first_meaningful_paint.has_value() &&
      timing.paint_timing->first_meaningful_paint->is_positive()) {
    first_meaningful_paint_time =
        timing.paint_timing->first_meaningful_paint->InMilliseconds();
  }

  int64_t navigation_start_time =
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds();
  ReportFirstMeaningfulPaint(navigation_start_time,
                             first_meaningful_paint_time);
}

void OhPageLoadMetricsObserver::ReportFirstMeaningfulPaint(
    int64_t navigation_start_time,
    int64_t first_meaningful_paint_time) {
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(
          GetDelegate().GetWebContents());
  if (!browser.get()) {
    return;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get()) {
    return;
  }

  CefRefPtr<CefLoadHandler> load_handler = client->GetLoadHandler();
  if (!load_handler.get()) {
    return;
  }
  CefRefPtr<CefFirstMeaningfulPaintDetails> details =
      new CefFirstMeaningfulPaintDetailsImpl(navigation_start_time,
                                             first_meaningful_paint_time);
  load_handler->OnFirstMeaningfulPaint(details);
}

void OhPageLoadMetricsObserver::ReportLargestContentfulPaint(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(
          GetDelegate().GetWebContents());
  if (!browser.get()) {
    return;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get()) {
    return;
  }

  CefRefPtr<CefLoadHandler> load_handler = client->GetLoadHandler();
  if (!load_handler.get()) {
    return;
  }

  int64_t navigation_start_time =
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds();
  int64_t largest_image_paint_time = 0;
  if (timing.paint_timing->largest_contentful_paint->largest_image_paint
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_image_paint
          ->is_positive()) {
    largest_image_paint_time = timing.paint_timing->largest_contentful_paint
                                   ->largest_image_paint->InMilliseconds();
  }

  int64_t largest_text_paint_time = 0;
  if (timing.paint_timing->largest_contentful_paint->largest_text_paint
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->is_positive()) {
    largest_text_paint_time = timing.paint_timing->largest_contentful_paint
                                  ->largest_text_paint->InMilliseconds();
  }

  int64_t largest_image_load_start_time = 0;
  if (timing.paint_timing->largest_contentful_paint->largest_image_load_start
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_image_load_start
          ->is_positive()) {
    largest_image_load_start_time =
        timing.paint_timing->largest_contentful_paint->largest_image_load_start
            ->InMilliseconds();
  }

  int64_t largest_image_load_end_time = 0;
  if (timing.paint_timing->largest_contentful_paint->largest_image_load_end
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_image_load_end
          ->is_positive()) {
    largest_image_load_end_time =
        timing.paint_timing->largest_contentful_paint->largest_image_load_end
            ->InMilliseconds();
  }

  double_t image_bpp = timing.paint_timing->largest_contentful_paint->image_bpp;

  CefRefPtr<CefLargestContentfulPaintDetails> details =
      new CefLargestContentfulPaintDetailsImpl(
          navigation_start_time, largest_image_paint_time,
          largest_text_paint_time, largest_image_load_start_time,
          largest_image_load_end_time, image_bpp);
  load_handler->OnLargestContentfulPaint(details);
}
