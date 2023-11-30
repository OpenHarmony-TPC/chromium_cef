// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_SOC_PERF_UTIL_H_
#define CEF_LIBCEF_COMMON_SOC_PERF_UTIL_H_

#include "base/time/time.h"

namespace soc_perf {
extern int video_layout_num;
extern int layer_num;

class SocPerUtil {
 public:
  // #if defined(OHOS_PERFORMANCE_INC_FREQ)
  static void EnableFlingBoost();
  static void DisableFlingBoost();
  // #endif
  static void StartBoost();

 private:
  static void TryRunSocPerf();
  static bool boost_started;
  static bool boost_finished;
  static base::Time first_time_boost_timestamp;
  static base::Time last_time_boost_timestamp;
};

}  // namespace soc_perf

#endif  // CEF_LIBCEF_COMMON_SOC_PERF_UTIL_H_
