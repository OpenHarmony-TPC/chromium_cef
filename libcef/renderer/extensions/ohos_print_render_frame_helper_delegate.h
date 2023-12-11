// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef OHOS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define OHOS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

namespace extensions {

class OhosPrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  explicit OhosPrintRenderFrameHelperDelegate(bool is_windowless);
  ~OhosPrintRenderFrameHelperDelegate() override;

  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;
  bool IsPrintPreviewEnabled() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;

 private:
  bool is_windowless_;
};

}  // namespace extensions

#endif  // OHOS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_