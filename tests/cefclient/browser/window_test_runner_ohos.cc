// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_ohos.h"

#include "include/views/cef_browser_view.h"
#include "include/views/cef_display.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/root_window_views.h"
#include "tests/cefclient/browser/views_window.h"

namespace client::window_test {

namespace {

CefRefPtr<CefWindow> GetWindow(const CefRefPtr<CefBrowser>& browser) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(browser->GetHost()->HasView());

  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  DCHECK(browser_view.get());

  CefRefPtr<CefWindow> window = browser_view->GetWindow();
  DCHECK(window.get());
  return window;
}

void SetTitlebarHeight(const CefRefPtr<CefBrowser>& browser,
                       const std::optional<float>& height) {
  CEF_REQUIRE_UI_THREAD();
  auto root_window = RootWindow::GetForBrowser(browser->GetIdentifier());
  DCHECK(root_window.get());

  auto root_window_views = static_cast<RootWindowViews*>(root_window.get());
  root_window_views->SetTitlebarHeight(height);
}

}  // namespace

WindowTestRunnerOhos::WindowTestRunnerOhos() = default;

void WindowTestRunnerOhos::Minimize(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Minimize();
}

void WindowTestRunnerOhos::Maximize(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Maximize();
}

void WindowTestRunnerOhos::Restore(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Restore();
}

void WindowTestRunnerOhos::Fullscreen(CefRefPtr<CefBrowser> browser) {
  auto window = GetWindow(browser);

  // Results in a call to ViewsWindow::OnWindowFullscreenTransition().
  if (window->IsFullscreen()) {
    window->SetFullscreen(false);
  } else {
    window->SetFullscreen(true);
  }
}

}  // namespace client::window_test
