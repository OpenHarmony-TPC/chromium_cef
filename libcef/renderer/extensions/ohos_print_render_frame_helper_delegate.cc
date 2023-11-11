/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
