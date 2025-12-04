// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/browser_platform_delegate_osr_ohos.h"

#include <utility>

CefBrowserPlatformDelegateOsrOhos::CefBrowserPlatformDelegateOsrOhos(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    bool use_shared_texture,
    bool use_external_begin_frame)
    : CefBrowserPlatformDelegateOsr(std::move(native_delegate),
                                    use_shared_texture,
                                    use_external_begin_frame) {}

CefWindowHandle CefBrowserPlatformDelegateOsrOhos::GetHostWindowHandle()
    const {
  return native_delegate_->window_info().parent_window;
}
