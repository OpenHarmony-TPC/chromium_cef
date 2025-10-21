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

void WindowTestRunnerOhos::SetPos(CefRefPtr<CefBrowser> browser,
                                   int x,
                                   int y,
                                   int width,
                                   int height) {
  CefRefPtr<CefWindow> window = GetWindow(browser);

  CefRect window_bounds(x, y, width, height);
  ModifyBounds(window->GetDisplay()->GetWorkArea(), window_bounds);

  window->SetBounds(window_bounds);
}

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
