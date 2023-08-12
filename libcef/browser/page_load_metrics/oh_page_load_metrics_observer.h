/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_OH_PAGE_LOAD_METRICS_OBSERVER_H_
#define CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_OH_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/nqe/effective_connection_type.h"

namespace network {
class NetworkQualityTracker;
}

class GURL;

class OhPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  OhPageLoadMetricsObserver();

  OhPageLoadMetricsObserver(const OhPageLoadMetricsObserver&) = delete;
  OhPageLoadMetricsObserver& operator=(const OhPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver:
  // PageLoadMetricsObserver lifecycle callbacks
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // PageLoadMetricsObserver event callbacks
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  OhPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker)
      : network_quality_tracker_(network_quality_tracker) {}

  virtual void ReportFirstContentfulPaint(int64_t navigation_start_tick,
                                          int64_t first_contentful_paint_ms);

 private:
  int64_t navigation_id_ = -1;

  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_OH_PAGE_LOAD_METRICS_OBSERVER_H_
