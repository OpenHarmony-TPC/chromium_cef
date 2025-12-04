// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_OHOS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_OHOS_H_
#pragma once

#include <memory>
#include <string>

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"
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
  void SetBounds(int x,
                 int y,
                 size_t width,
                 size_t height,
                 bool content_bounds) override;
  bool DefaultToContentBounds() const override;
  void Close(bool force) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  std::optional<float> GetDeviceScaleFactor() const override;
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
  void OnContentsBounds(const CefRect& new_bounds) override {
    RootWindow::SetBounds(new_bounds,
                          /*content_bounds=*/DefaultToContentBounds());
  }
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
  OsrRendererSettings osr_settings_;
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
