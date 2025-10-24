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

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_OHOS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_OHOS_H_
#pragma once

#include <memory>
#include <string>

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/root_window.h"

namespace client {

// RootWindowOhos implementation of a top-level native window in the browser
// process. The methods of this class must be called on the main thread unless
// otherwise indicated.
class RootWindowOhos : public RootWindow, public BrowserWindow::Delegate {
 public:
  // Constructor may be called on any thread.
  explicit RootWindowOhos(bool use_alloy_style);
  ~RootWindowOhos();

  // RootWindow methods.
  void Init(RootWindow::Delegate* delegate,
            std::unique_ptr<RootWindowConfig> config,
            const CefBrowserSettings& settings) override;
  void InitAsPopup(RootWindow::Delegate* delegate,
                   bool with_controls,
                   bool with_osr,
                   const CefPopupFeatures& popupFeatures,
                   CefWindowInfo& windowInfo,
                   CefRefPtr<CefClient>& client,
                   CefBrowserSettings& settings) override;
  void Show(ShowMode mode) override;
  void Hide() override;
  void SetBounds(int x, int y, size_t width, size_t height) override;
  void Close(bool force) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  float GetDeviceScaleFactor() const override;
  CefRefPtr<CefBrowser> GetBrowser() const override;
  ClientWindowHandle GetWindowHandle() const override;
  bool WithWindowlessRendering() const override;

 private:
  void CreateBrowserWindow(const std::string& startup_url);
  void CreateRootWindow(const CefBrowserSettings& settings,
                        bool initially_hidden);

  // BrowserWindow::Delegate methods.
  bool UseAlloyStyle() const override { return IsAlloyStyle(); }
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBrowserWindowClosing() override;
  void OnBrowserWindowDestroyed() override;
  void OnSetAddress(const std::string& url) override;
  void OnSetTitle(const std::string& title) override;
  void OnSetFullscreen(bool fullscreen) override;
  void OnAutoResize(const CefSize& new_size) override;
  void OnSetLoadingState(bool isLoading,
                         bool canGoBack,
                         bool canGoForward) override;
  void OnSetDraggableRegions(
      const std::vector<CefDraggableRegion>& regions) override;

  void NotifyMoveOrResizeStarted();
  void NotifySetFocus();
  void NotifyVisibilityChange(bool show);
  void NotifyMenuBarHeight(int height);
  void NotifyContentBounds(int x, int y, int width, int height);
  void NotifyLoadURL(const std::string& url);
  void NotifyButtonClicked(int id);
  void NotifyMenuItem(int id);
  void NotifyForceClose();
  void NotifyCloseBrowser();
  void NotifyDestroyedIfDone(bool window_destroyed, bool browser_destroyed);

  // After initialization all members are only accessed on the main thread.
  // Members set during initialization.
  bool with_controls_;
  bool always_on_top_;
  bool with_osr_;
  bool is_popup_;
  CefRect start_rect_;
  std::unique_ptr<BrowserWindow> browser_window_;

  // Main window.
  ClientWindowHandle window_;

  CefRect browser_bounds_;

  bool window_destroyed_;
  bool browser_destroyed_;

  // Members only accessed on the UI thread because they're needed for
  // WindowDelete.
  bool force_close_;
  bool is_closing_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowOhos);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_OHOS_H_
