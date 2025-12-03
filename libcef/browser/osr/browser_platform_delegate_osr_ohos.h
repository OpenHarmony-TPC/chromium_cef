// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_OHOS_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_OHOS_H_

#include "cef/libcef/browser/osr/browser_platform_delegate_osr.h"

// Windowless browser implementation for OHOS.
class CefBrowserPlatformDelegateOsrOhos
    : public CefBrowserPlatformDelegateOsr {
 public:
  CefBrowserPlatformDelegateOsrOhos(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      bool use_shared_texture,
      bool use_external_begin_frame);

  // CefBrowserPlatformDelegate methods:
  CefWindowHandle GetHostWindowHandle() const override;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_OHOS_H_
