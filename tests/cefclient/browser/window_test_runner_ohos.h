// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_OHOS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_OHOS_H_
#pragma once

#include "tests/cefclient/browser/window_test_runner.h"

namespace client::window_test {

// Ohos platform implementation.
class WindowTestRunnerOhos : public WindowTestRunner {
 public:
  WindowTestRunnerOhos();

  void Minimize(CefRefPtr<CefBrowser> browser) override;
  void Maximize(CefRefPtr<CefBrowser> browser) override;
  void Restore(CefRefPtr<CefBrowser> browser) override;
  void Fullscreen(CefRefPtr<CefBrowser> browser) override;
};

}  // namespace client::window_test

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_OHOS_H_
