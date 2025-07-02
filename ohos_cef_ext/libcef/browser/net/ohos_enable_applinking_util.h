// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ENABLE_APPLINKING_UTIL_H_
#define OHOS_ENABLE_APPLINKING_UTIL_H_

class OhosEnableApplinkingUtil {
  public:
    static bool IsAppLinkingEnabled();
    static void EnableAppLinking(bool enable);

  private:
    static bool is_arkweb_applinking_enabled_;
};

#endif // OHOS_ENABLE_APPLINKING_UTIL_H_
