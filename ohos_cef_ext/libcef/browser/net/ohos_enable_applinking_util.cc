// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/browser/net/ohos_enable_applinking_util.h"
#include "base/logging.h"

bool OhosEnableApplinkingUtil::is_arkweb_applinking_enabled_ = true;

bool OhosEnableApplinkingUtil::IsAppLinkingEnabled() {
  return is_arkweb_applinking_enabled_;
}

void OhosEnableApplinkingUtil::EnableAppLinking(bool enable) {
  LOG(DEBUG) << "before EnableAppLinking is_arkweb_applinking_enabled_ is: "
             << is_arkweb_applinking_enabled_
             << ", after EnableAppLinking is: " << enable;
  is_arkweb_applinking_enabled_ = enable;
}
