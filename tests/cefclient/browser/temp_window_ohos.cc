// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tests/cefclient/browser/temp_window_ohos.h"


namespace client {

TempWindowOhos* g_temp_window = nullptr;

TempWindowOhos::TempWindowOhos() {
  DCHECK(!g_temp_window);
  g_temp_window = this;
}

TempWindowOhos::~TempWindowOhos() {
  g_temp_window = nullptr;
}

// static
CefWindowHandle TempWindowOhos::GetWindowHandle() {
  DCHECK(g_temp_window);
  return g_temp_window->ohos_window_;
}

}  // namespace client


