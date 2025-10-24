/*
 * Copyright (c) 2023-2025 Haitai FangYuan Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
