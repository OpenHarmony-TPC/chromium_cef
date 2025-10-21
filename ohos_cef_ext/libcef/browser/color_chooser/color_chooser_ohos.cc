// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "color_chooser_ohos.h"

#include "base/logging.h"
#include "cef/include/cef_client.h"
#include "cef/include/cef_dialog_handler.h"
#include "cef/ohos_cef_ext/include/arkweb_dialog_handler_ext.h"

class ColorChooserCallbackImpl : public CefColorChooserCallback {
 public:
  explicit ColorChooserCallbackImpl(ColorChooserOhos* chooser)
    : chooser_(chooser) {}

  void Continue(uint32_t color) override {
    if (chooser_) {
      chooser_->OnColorChosen(color);
    }
  }

  void Cancel() override {
    if (chooser_) {
      chooser_->End();
    }
  }
 private:
  ColorChooserOhos* chooser_ = nullptr;
  IMPLEMENT_REFCOUNTING(ColorChooserCallbackImpl);
};

ColorChooserOhos::~ColorChooserOhos() = default;

ColorChooserOhos::ColorChooserOhos(
    CefRefPtr<CefClient> client,
    content::WebContents* tab,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
    : client_(client), web_contents_(tab) {
  if (client) {
    if (auto handler = client->GetDialogHandler()) {
      CefRefPtr<CefColorChooserCallback> callback = new ColorChooserCallbackImpl(this);
      handler->OnColorChooserShow(static_cast<uint32_t>(initial_color), callback);
    }
  }
}

void ColorChooserOhos::End() {
  LOG(ERROR) << "ColorChooserOhos::End()";
  web_contents_->DidEndColorChooser();
}

void ColorChooserOhos::SetSelectedColor(SkColor color) {
  // Not implemented since the color is set on the java side only, in theory
  // it can be set from JS which would override the user selection but
  // we don't support that for now.
}

void ColorChooserOhos::OnColorChosen(uint32_t color) {
  web_contents_->DidChooseColorInColorChooser(color);
  web_contents_->DidEndColorChooser();
}