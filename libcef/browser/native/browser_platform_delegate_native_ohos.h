// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_OHOS_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_OHOS_H_

#include "cef/libcef/browser/native/browser_platform_delegate_native_aura.h"

// Windowed browser implementation for Ohos.
class CefBrowserPlatformDelegateNativeOhos
    : public CefBrowserPlatformDelegateNativeAura {
 public:
  CefBrowserPlatformDelegateNativeOhos(const CefWindowInfo& window_info,
                                       SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  void SetFocus(bool setFocus) override;
  void NotifyMoveOrResizeStarted() override;
  void SizeTo(int width, int height) override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const input::NativeWebKeyboardEvent& event) const override;

  // CefBrowserPlatformDelegateNativeAura methods:
  ui::KeyEvent TranslateUiKeyEvent(const CefKeyEvent& key_event) const override;
  input::NativeWebKeyboardEvent TranslateWebKeyEvent(
      const CefKeyEvent& key_event) const override;

 private:
  // True if the host window has been created.
  bool host_window_created_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_OHOS_H_
