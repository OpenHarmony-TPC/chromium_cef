// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple/simple_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_display.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefsimple/simple_handler.h"

#if defined(OS_OHOS)
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/window/window_common.h"
#endif

#if defined(OS_OHOS)
using namespace ohos::adapter::xcomponent;
using namespace ohos::adapter::window;
#endif

namespace {

// When using the Views framework this object provides the delegate
// implementation for the CefWindow that hosts the Views-based browser.
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                       cef_runtime_style_t runtime_style,
                       cef_show_state_t initial_show_state)
      : browser_view_(browser_view),
        runtime_style_(runtime_style),
        initial_show_state_(initial_show_state) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window.
    window->AddChildView(browser_view_);

    if (initial_show_state_ != CEF_SHOW_STATE_HIDDEN) {
      window->Show();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

#if defined(OS_OHOS)
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override {
    // Always provide explicit bounds so new windows don't inherit previously
    // saved/moved/resized window state.
    auto window_rect = WindowAdapter::GetInstance().GetInitialBounds();
    const int target_width = 800;
    const int target_height = 600;
    // WindowDelegate expects DIP. Prefer display work area in DIP for centering
    // (excludes system bars) and avoid pixel->DIP rounding drift.
    CefRect initial_bounds_px(window_rect.left, window_rect.top,
                              window_rect.width, window_rect.height);
    CefRefPtr<CefDisplay> display =
        CefDisplay::GetDisplayMatchingBounds(initial_bounds_px,
                                             /*input_pixel_coords=*/true);
    if (!display) {
      display = CefDisplay::GetPrimaryDisplay();
    }
    CefRect work_area = display ? display->GetWorkArea() : CefRect();
    if (work_area.IsEmpty()) {
      // Fallback: treat the pixel bounds as DIP bounds.
      work_area = initial_bounds_px;
    }

    int x = work_area.x + (work_area.width - target_width) / 2;
    int y = work_area.y + (work_area.height - target_height) / 2;

    if (x < work_area.x) {
      x = work_area.x;
    }
    if (y < work_area.y) {
      y = work_area.y;
    }
    if (x + target_width > work_area.x + work_area.width) {
      x = work_area.x + work_area.width - target_width;
    }
    if (y + target_height > work_area.y + work_area.height) {
      y = work_area.y + work_area.height - target_height;
    }
    if (x < work_area.x) {
      x = work_area.x;
    }
    if (y < work_area.y) {
      y = work_area.y;
    }
    return CefRect(x, y, target_width, target_height);
  }
#endif

  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return initial_show_state_;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  const cef_runtime_style_t runtime_style_;
  const cef_show_state_t initial_show_state_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit SimpleBrowserViewDelegate(cef_runtime_style_t runtime_style)
      : runtime_style_(runtime_style) {}

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    // Create a new top-level Window for the popup. It will show itself after
    // creation.
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        popup_browser_view, runtime_style_, CEF_SHOW_STATE_NORMAL));

    // We created the Window.
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  const cef_runtime_style_t runtime_style_;

  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

}  // namespace

SimpleApp::SimpleApp() = default;

void SimpleApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // Check if Alloy style will be used.
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
  bool use_alloy_style = command_line->HasSwitch("use-alloy-style");
  if (use_alloy_style) {
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  // SimpleHandler implements browser-level callbacks.
  CefRefPtr<SimpleHandler> handler(new SimpleHandler(use_alloy_style));

  // Specify CEF browser settings here.
  CefBrowserSettings browser_settings;

  std::string url;

  // Check if a "--url=" value was provided via the command-line. If so, use
  // that instead of the default URL.
  url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://www.google.com";
  }

  // Views is enabled by default (add `--use-native` to disable).
  const bool use_views = !command_line->HasSwitch("use-native");

  // If using Views create the browser using the Views framework, otherwise
  // create the browser using the native platform framework.
  if (use_views) {
    // Create the BrowserView.
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        handler, url, browser_settings, nullptr, nullptr,
        new SimpleBrowserViewDelegate(runtime_style));

    // Optionally configure the initial show state.
    cef_show_state_t initial_show_state = CEF_SHOW_STATE_NORMAL;
    const std::string& show_state_value =
        command_line->GetSwitchValue("initial-show-state");
    if (show_state_value == "minimized") {
      initial_show_state = CEF_SHOW_STATE_MINIMIZED;
    } else if (show_state_value == "maximized") {
      initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
    }
#if defined(OS_MAC)
    // Hidden show state is only supported on MacOS.
    else if (show_state_value == "hidden") {
      initial_show_state = CEF_SHOW_STATE_HIDDEN;
    }
#endif

    // Create the Window. It will show itself after creation.
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        browser_view, runtime_style, initial_show_state));
  } else {
    // Information used when creating the native window.
    CefWindowInfo window_info;

#if defined(OS_WIN)
    // On Windows we need to specify certain flags that will be passed to
    // CreateWindowEx().
    window_info.SetAsPopup(nullptr, "cefsimple");
#endif

    // Alloy style will create a basic native window. Chrome style will create a
    // fully styled Chrome UI window.
    window_info.runtime_style = runtime_style;
    // enable osr
    // window_info.windowless_rendering_enabled = true;

    // Create the first browser window.
    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings,
                                  nullptr, nullptr);
  }
}

CefRefPtr<CefClient> SimpleApp::GetDefaultClient() {
  // Called when a new browser window is created via Chrome style UI.
  return SimpleHandler::GetInstance();
}
