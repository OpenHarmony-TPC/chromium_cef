// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "libcef/renderer/extensions/ohos_print_render_frame_helper_delegate.h"

#include "libcef/common/extensions/extensions_util.h"
#include "third_party/blink/public/web/web_element.h"

namespace extensions {

OhosPrintRenderFrameHelperDelegate::OhosPrintRenderFrameHelperDelegate(
    bool is_windowless)
    : is_windowless_(is_windowless) {}

OhosPrintRenderFrameHelperDelegate::~OhosPrintRenderFrameHelperDelegate() = default;

blink::WebElement OhosPrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool OhosPrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  return !is_windowless_ && PrintPreviewEnabled();
}

bool OhosPrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace extensions
