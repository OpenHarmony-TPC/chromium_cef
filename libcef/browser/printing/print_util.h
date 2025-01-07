// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
#pragma once

#include "include/cef_browser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace print_util {

struct PdfCreateParams {
  absl::optional<std::string> header_template;
  absl::optional<std::string> footer_template;
  absl::optional<double> scale;
  absl::optional<double> paper_width;
  absl::optional<double> paper_height;
  absl::optional<double> margin_top;
  absl::optional<double> margin_bottom;
  absl::optional<double> margin_left;
  absl::optional<double> margin_right;
};

// Called from CefBrowserHostBase::Print.
void Print(content::WebContents* web_contents, bool print_preview_disabled);

// Called from CefBrowserHostBase::PrintToPDF.
void PrintToPDF(content::WebContents* web_contents,
                const CefString& path,
                const CefPdfPrintSettings& settings,
                CefRefPtr<CefPdfPrintCallback> callback);

void CreateToPDF(content::WebContents* web_contents,
                 const CefPdfPrintSettings& settings,
                 CefRefPtr<CefPdfValueCallback> callback);

}  // namespace print_util

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
