// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on page_load_metrics_initialize.h originally written by
// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_
#define CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_

namespace content {
class WebContents;
}

namespace cef {
void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents);

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_
