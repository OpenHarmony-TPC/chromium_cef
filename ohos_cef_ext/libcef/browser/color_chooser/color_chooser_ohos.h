// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COLOR_CHOOSER_COLOR_CHOOSER_OHOS_H_
#define CEF_LIBCEF_COLOR_CHOOSER_COLOR_CHOOSER_OHOS_H_

#include "cef/include/cef_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

class ColorChooserOhos : public content::ColorChooser {
 public:
  ColorChooserOhos(
      CefRefPtr<CefClient> client,
      content::WebContents* tab,
      SkColor initial_color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  ColorChooserOhos(const ColorChooserOhos&) = delete;
  ColorChooserOhos& operator=(const ColorChooserOhos&) = delete;

  ~ColorChooserOhos() override;

  void OnColorChosen(uint32_t color);

  // ColorPicker interface
  void End() override;
  void SetSelectedColor(SkColor color) override;

 private:
  // The web contents invoking the color chooser.  No ownership. because it will
  // outlive this class.
  CefRefPtr<CefClient> client_;
  raw_ptr<content::WebContents> web_contents_;
};
#endif