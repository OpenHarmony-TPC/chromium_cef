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

#include "tests/cefclient/browser/root_window_ohos.h"

#undef Success     // Definition conflicts with cef_message_router.h
#undef RootWindow  // Definition conflicts with root_window.h

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "tests/cefclient/browser/browser_window_osr_ohos.h"
#include "tests/cefclient/browser/browser_window_std_ohos.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/temp_window.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/common/client_switches.h"

using namespace ohos::adapter::xcomponent;

namespace client {

RootWindowOhos::RootWindowOhos(bool use_alloy_style)
    : RootWindow(use_alloy_style),
      with_controls_(false),
      always_on_top_(false),
      with_osr_(false),
      is_popup_(false),
      window_destroyed_(false),
      browser_destroyed_(false),
      force_close_(false),
      is_closing_(false) {}

RootWindowOhos::~RootWindowOhos() {
  REQUIRE_MAIN_THREAD();

  // The window and browser should already have been destroyed.
  DCHECK(window_destroyed_);
  DCHECK(browser_destroyed_);
}

void RootWindowOhos::Init(RootWindow::Delegate* delegate,
                          std::unique_ptr<RootWindowConfig> config,
                          const CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = config->with_controls;
  always_on_top_ = config->always_on_top;
  with_osr_ = config->with_osr;
  start_rect_ = config->bounds;

  CreateBrowserWindow(config->url);

  initialized_ = true;

  // Always post asynchronously to avoid reentrancy of the GDK lock.
  MAIN_POST_CLOSURE(base::BindOnce(&RootWindowOhos::CreateRootWindow, this,
                                   settings, config->initially_hidden));
}

void RootWindowOhos::InitAsPopup(RootWindow::Delegate* delegate,
                                 bool with_controls,
                                 bool with_osr,
                                 const CefPopupFeatures& popupFeatures,
                                 CefWindowInfo& windowInfo,
                                 CefRefPtr<CefClient>& client,
                                 CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = with_controls;
  with_osr_ = with_osr;
  is_popup_ = true;

  if (popupFeatures.xSet) {
    start_rect_.x = popupFeatures.x;
  }
  if (popupFeatures.ySet) {
    start_rect_.y = popupFeatures.y;
  }
  if (popupFeatures.widthSet) {
    start_rect_.width = popupFeatures.width;
  }
  if (popupFeatures.heightSet) {
    start_rect_.height = popupFeatures.height;
  }

  CreateBrowserWindow(std::string());

  initialized_ = true;

  // The new popup is initially parented to a temporary window. The native root
  // window will be created after the browser is created and the popup window
  // will be re-parented to it at that time.
  browser_window_->GetPopupConfig(TempWindow::GetWindowHandle(), windowInfo,
                                  client, settings);
}

void RootWindowOhos::Show(ShowMode mode) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }
}

void RootWindowOhos::Hide() {
  REQUIRE_MAIN_THREAD();
}

void RootWindowOhos::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();
  if (!window_) {
    return;
  }
}

void RootWindowOhos::Close(bool force) {
  REQUIRE_MAIN_THREAD();
}

void RootWindowOhos::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    browser_window_->SetDeviceScaleFactor(device_scale_factor);
  }
}

float RootWindowOhos::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    return browser_window_->GetDeviceScaleFactor();
  }

  NOTREACHED();
  return 0.0f;
}

CefRefPtr<CefBrowser> RootWindowOhos::GetBrowser() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_) {
    return browser_window_->GetBrowser();
  }
  return nullptr;
}

ClientWindowHandle RootWindowOhos::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return window_;
}

bool RootWindowOhos::WithWindowlessRendering() const {
  REQUIRE_MAIN_THREAD();
  return with_osr_;
}

void RootWindowOhos::CreateBrowserWindow(const std::string& startup_url) {
  if (with_osr_) {
    OsrRendererSettings settings = {};
    MainContext::Get()->PopulateOsrSettings(&settings);
    browser_window_.reset(
        new BrowserWindowOsrOhos(this, with_controls_, startup_url, settings));
  } else {
    browser_window_.reset(
        new BrowserWindowStdOhos(this, with_controls_, startup_url));
  }
}

void RootWindowOhos::CreateRootWindow(const CefBrowserSettings& settings,
                                      bool initially_hidden) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!window_);

  // TODO(port): If no x,y position is specified the window will always appear
  // in the upper-left corner. Maybe there's a better default place to put it?
  int x = start_rect_.x;
  int y = start_rect_.y;
  int width;
  int height;
  if (start_rect_.IsEmpty()) {
    // TODO(port): Also, maybe there's a better way to choose the default size.
    // Default OSR widget size.
    auto window_rect = WindowAdapter::GetInstance().GetInitialBounds();
    width = window_rect.width;
    height = window_rect.height;
  } else {
    width = start_rect_.width;
    height = start_rect_.height;
  }

  Show(ShowNormal);

  // Most window managers ignore requests for initial window positions (instead
  // using a user-defined placement algorithm) and honor requests after the
  // window has already been shown.

  if (!is_popup_) {
    // Create the browser window.
    browser_window_->CreateBrowser(window_, browser_bounds_, settings, nullptr,
                                   delegate_->GetRequestContext());
  } else {
    // With popups we already have a browser window. Parent the browser window
    // to the root window and show it in the correct location.
    browser_window_->ShowPopup(window_, browser_bounds_.x, browser_bounds_.y,
                               browser_bounds_.width, browser_bounds_.height);
  }
}

void RootWindowOhos::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  // For popup browsers create the root window once the browser has been
  // created.
  if (is_popup_) {
    CreateRootWindow(CefBrowserSettings(), false);
  }
}

void RootWindowOhos::OnBrowserWindowClosing() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowOhos::OnBrowserWindowClosing, this));
    return;
  }

  is_closing_ = true;
}

void RootWindowOhos::OnBrowserWindowDestroyed() {
  REQUIRE_MAIN_THREAD();

  browser_window_.reset();

  if (!window_destroyed_) {
    // The browser was destroyed first. This could be due to the use of
    // off-screen rendering or execution of JavaScript window.close().
    // Close the RootWindow.
    Close(true);
  }

  NotifyDestroyedIfDone(false, true);
}

void RootWindowOhos::OnSetAddress(const std::string& url) {
  REQUIRE_MAIN_THREAD();
}

void RootWindowOhos::OnSetTitle(const std::string& title) {
  REQUIRE_MAIN_THREAD();
}

void RootWindowOhos::OnSetFullscreen(bool fullscreen) {
  REQUIRE_MAIN_THREAD();

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
  }
}

void RootWindowOhos::OnAutoResize(const CefSize& new_size) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }
}

void RootWindowOhos::OnSetLoadingState(bool isLoading,
                                       bool canGoBack,
                                       bool canGoForward) {
  REQUIRE_MAIN_THREAD();
}

void RootWindowOhos::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  REQUIRE_MAIN_THREAD();
  // TODO(cef): Implement support for draggable regions on this platform.
}

void RootWindowOhos::NotifyMoveOrResizeStarted() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyMoveOrResizeStarted, this));
    return;
  }

  // Called when size, position or stack order changes.
  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser.get()) {
    // Notify the browser of move/resize events so that:
    // - Popup windows are displayed in the correct location and dismissed
    //   when the window moves.
    // - Drag&drop areas are updated accordingly.
    browser->GetHost()->NotifyMoveOrResizeStarted();
  }
}

void RootWindowOhos::NotifySetFocus() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowOhos::NotifySetFocus, this));
    return;
  }

  if (!browser_window_.get()) {
    return;
  }

  browser_window_->SetFocus(true);
  delegate_->OnRootWindowActivated(this);
}

void RootWindowOhos::NotifyVisibilityChange(bool show) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyVisibilityChange, this, show));
    return;
  }

  if (!browser_window_.get()) {
    return;
  }

  if (show) {
    browser_window_->Show();
  } else {
    browser_window_->Hide();
  }
}

void RootWindowOhos::NotifyMenuBarHeight(int height) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyMenuBarHeight, this, height));
    return;
  }
}

void RootWindowOhos::NotifyContentBounds(int x, int y, int width, int height) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowOhos::NotifyContentBounds, this,
                                     x, y, width, height));
    return;
  }
}

void RootWindowOhos::NotifyLoadURL(const std::string& url) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyLoadURL, this, url));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser.get()) {
    browser->GetMainFrame()->LoadURL(url);
  }
}

void RootWindowOhos::NotifyButtonClicked(int id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyButtonClicked, this, id));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (!browser.get()) {
    return;
  }

  switch (id) {
    case IDC_NAV_BACK:
      browser->GoBack();
      break;
    case IDC_NAV_FORWARD:
      browser->GoForward();
      break;
    case IDC_NAV_RELOAD:
      browser->Reload();
      break;
    case IDC_NAV_STOP:
      browser->StopLoad();
      break;
    default:
      NOTREACHED() << "id=" << id;
  }
}

void RootWindowOhos::NotifyMenuItem(int id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyMenuItem, this, id));
    return;
  }

  // Run the test.
  if (delegate_) {
    delegate_->OnTest(this, id);
  }
}

void RootWindowOhos::NotifyForceClose() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowOhos::NotifyForceClose, this));
    return;
  }

  force_close_ = true;
}

void RootWindowOhos::NotifyCloseBrowser() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowOhos::NotifyCloseBrowser, this));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
    browser->GetHost()->CloseBrowser(false);
  }
}

void RootWindowOhos::NotifyDestroyedIfDone(bool window_destroyed,
                                           bool browser_destroyed) {
  // Each call will to this method will set only one state flag.
  DCHECK_EQ(1, window_destroyed + browser_destroyed);

  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowOhos::NotifyDestroyedIfDone,
                                     this, window_destroyed,
                                     browser_destroyed));
    return;
  }

  if (window_destroyed) {
    window_destroyed_ = true;
  }
  if (browser_destroyed) {
    browser_destroyed_ = true;
  }

  // Notify once both the window and the browser have been destroyed.
  if (window_destroyed_ && browser_destroyed_) {
    delegate_->OnRootWindowDestroyed(this);
  }
}

}  // namespace client
