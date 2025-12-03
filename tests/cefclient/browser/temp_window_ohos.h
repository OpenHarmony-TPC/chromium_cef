// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_TEMP_WINDOW_OHOS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_TEMP_WINDOW_OHOS_H_
#pragma once

#include "include/cef_base.h"

namespace client {

// Represents a singleton hidden window that acts as a temporary parent for
// popup browsers. Only accessed on the UI thread.
class TempWindowOhos {
 public:
  // Returns the singleton window handle.
  static CefWindowHandle GetWindowHandle();

 private:
  // A single instance will be created/owned by RootWindowManager.
  friend class RootWindowManager;
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<TempWindowOhos>;

  TempWindowOhos();
  ~TempWindowOhos();

  CefWindowHandle ohos_window_;

  DISALLOW_COPY_AND_ASSIGN(TempWindowOhos);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_TEMP_WINDOW_OHOS_H_

