// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_std_ohos.h"

#undef Success     // Definition conflicts with cef_message_router.h
#undef RootWindow  // Definition conflicts with root_window.h

#include "include/base/cef_logging.h"
#include "tests/cefclient/browser/client_handler_std.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

BrowserWindowStdOhos::BrowserWindowStdOhos(Delegate* delegate,
                                           bool with_controls,
                                           const std::string& startup_url)
    : BrowserWindow(delegate) {
  client_handler_ = new ClientHandlerStd(this, with_controls, startup_url);
}

void BrowserWindowStdOhos::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  LOG(INFO) << "call BrowserWindowStdOhos::CreateBrowser parent_handle:"
            << parent_handle;
  REQUIRE_MAIN_THREAD();

  CefWindowInfo window_info;
  window_info.SetAsChild(parent_handle, rect);

  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowStdOhos::GetPopupConfig(CefWindowHandle temp_handle,
                                          CefWindowInfo& windowInfo,
                                          CefRefPtr<CefClient>& client,
                                          CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();
  // The window will be properly sized after the browser is created.
  windowInfo.SetAsChild(temp_handle, CefRect());
  client = client_handler_;
}

void BrowserWindowStdOhos::ShowPopup(ClientWindowHandle parent_handle,
                                     int x,
                                     int y,
                                     size_t width,
                                     size_t height) {
  REQUIRE_MAIN_THREAD();
}

void BrowserWindowStdOhos::Show() {
  REQUIRE_MAIN_THREAD();
}

void BrowserWindowStdOhos::Hide() {
  REQUIRE_MAIN_THREAD();
}

void BrowserWindowStdOhos::SetBounds(int x,
                                     int y,
                                     size_t width,
                                     size_t height) {
  REQUIRE_MAIN_THREAD();
}

void BrowserWindowStdOhos::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  if (browser_) {
    browser_->GetHost()->SetFocus(focus);
  }
}

ClientWindowHandle BrowserWindowStdOhos::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  // There is no Widget* representation of this object.
  NOTREACHED();
  return (ClientWindowHandle)NULL;
}

}  // namespace client
