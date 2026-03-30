// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/browser/net/ohos_url_rewrite_controller.h"
#include "base/logging.h"

bool OhosUrlRewriteController::is_rewrite_url_enabled_ = false;

bool OhosUrlRewriteController::IsRewriteUrlEnabled() {
  return is_rewrite_url_enabled_;
}

void OhosUrlRewriteController::EnableRewriteUrl(bool enable) {
  LOG(DEBUG) << "before EnableRewriteUrl is_rewrite_url_enabled_ is: "
             << is_rewrite_url_enabled_
             << ", after EnableRewriteUrl is: " << enable;
  is_rewrite_url_enabled_ = enable;
}