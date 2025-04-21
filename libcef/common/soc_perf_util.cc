// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soc_perf_util.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ohos_adapter_helper.h"
#include "soc_perf_client_adapter.h"

#if BUILDFLAG(IS_OHOS)
#include "base/ohos/sys_info_utils.h"
#endif

namespace soc_perf {
bool SocPerUtil::boost_started = false;
bool SocPerUtil::boost_finished = false;
bool SocPerUtil::is_slide = false;
base::Time SocPerUtil::first_time_boost_timestamp;
base::Time SocPerUtil::last_time_boost_timestamp;

namespace {
const int SOC_PERF_CONFIG_ID = 10020;
const int SOC_PERF_WEB_GESTURE_ID = 10012;
#if BUILDFLAG(IS_OHOS)
const int SOC_PERF_WEB_SLIDE_SCROLL = 10097;
#endif
const int MAX_BOOST_RUN_TIME_IN_SECOND = 10;
const int REST_TIME_IN_SECOND = 2;
}  // namespace

void SocPerUtil::EnableFlingBoost() {
  int socPerfId = SOC_PERF_WEB_GESTURE_ID;
#if BUILDFLAG(IS_OHOS)
  if (base::ohos::IsPcDevice()) {
    socPerfId = SOC_PERF_WEB_SLIDE_SCROLL;
  }
#endif
  TRACE_EVENT0("power", "SocPerUtil::EnableFlingBoost");
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(socPerfId, true);
}

void SocPerUtil::DisableFlingBoost() {
  int socPerfId = SOC_PERF_WEB_GESTURE_ID;
#if BUILDFLAG(IS_OHOS)
  if (base::ohos::IsPcDevice()) {
    socPerfId = SOC_PERF_WEB_SLIDE_SCROLL;
  }
#endif
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(socPerfId, false);
}

void SocPerUtil::TryRunSocPerf() {
  if ((base::Time().Now() - first_time_boost_timestamp).InSeconds() >=
      MAX_BOOST_RUN_TIME_IN_SECOND) {
    boost_finished = true;
    TRACE_EVENT0("power", "SocPerUtil::JsSocPerfFinished");
    return;
  }
  if ((base::Time().Now() - last_time_boost_timestamp).InSeconds() >=
      REST_TIME_IN_SECOND) {
    TRACE_EVENT0("power", "SocPerUtil::JsSocPerfThreadBoost");
    last_time_boost_timestamp = base::Time().Now();
    OHOS::NWeb::OhosAdapterHelper::GetInstance()
        .CreateSocPerfClientAdapter()
        ->ApplySocPerfConfigById(SOC_PERF_CONFIG_ID);
  }
}

void SocPerUtil::StartBoost() {
  if (base::ohos::IsMobileDevice() && is_slide) {
    return;
  }
  if (boost_finished) {
    return;
  }
  if (!boost_started) {
    boost_started = true;
    first_time_boost_timestamp = base::Time().Now();
  }
  TryRunSocPerf();
}

}  // namespace soc_perf
