// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "libcef/browser/page_load_metrics/alloy_page_load_metrics_memory_tracker_factory.h"

#include "libcef/browser/page_load_metrics/oh_page_load_metrics_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
}  // namespace page_load_metrics

namespace cef {
namespace {

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);

  PageLoadMetricsEmbedder(const PageLoadMetricsEmbedder&) = delete;
  PageLoadMetricsEmbedder& operator=(const PageLoadMetricsEmbedder&) = delete;

  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override;
  bool IsNoStatePrefetch(content::WebContents* web_contents) override;
  bool IsExtensionUrl(const GURL& url) override;
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterEmbedderObservers(
      page_load_metrics::PageLoadTracker* tracker) override;
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : PageLoadMetricsEmbedderBase(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() = default;

void PageLoadMetricsEmbedder::RegisterEmbedderObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  tracker->AddObserver(std::make_unique<OhPageLoadMetricsObserver>());
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  return false;
}

bool PageLoadMetricsEmbedder::IsNoStatePrefetch(
    content::WebContents* web_contents) {
  return false;
}

bool PageLoadMetricsEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring))
    return nullptr;

  return AlloyPageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
      browser_context);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  // Change this method? consider to modify the peer in
  // chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
  // weblayer/browser/page_load_metrics_initialize.cc
  // as well.
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace cef
