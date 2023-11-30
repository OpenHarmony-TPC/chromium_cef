// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soc_perf_util.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ohos_adapter_helper.h"
#include "soc_perf_client_adapter.h"

namespace soc_perf {
int video_layout_num = 0;
int layer_num = 0;
bool SocPerUtil::boost_started = false;
bool SocPerUtil::boost_finished = false;
base::Time SocPerUtil::first_time_boost_timestamp;
base::Time SocPerUtil::last_time_boost_timestamp;

namespace {
const int SOC_PERF_CONFIG_ID = 10020;
const int SOC_PERF_SLIDE_NORMAL_CONFIG_ID = 10025;
const int SOC_PERF_WEB_GUSTURE_ID = 10012;
const int MIN_LAYER_NUM = 50;
const int MIN_VIDEO_LAYOUT_NUM = 1;
const int MAX_BOOST_RUN_TIME_IN_SECOND = 10;
const int REST_TIME_IN_SECOND = 2;
}  // namespace

// #if defined(OHOS_PERFORMANCE_INC_FREQ)
void SocPerUtil::EnableFlingBoost() {
  TRACE_EVENT2("power", "SocPerUtil::EnableFlingBoost2", "layout_num",
               video_layout_num, "layer_num", layer_num);
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GUSTURE_ID, false);

  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_SLIDE_NORMAL_CONFIG_ID, true);

  if (video_layout_num >= MIN_VIDEO_LAYOUT_NUM || layer_num >= MIN_LAYER_NUM) {
    OHOS::NWeb::OhosAdapterHelper::GetInstance()
        .CreateSocPerfClientAdapter()
        ->ApplySocPerfConfigByIdEx(SOC_PERF_CONFIG_ID, true);
  }
}

void SocPerUtil::DisableFlingBoost() {
  TRACE_EVENT0("power", "SocPerUtil::DisableFlingBoost");

  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_SLIDE_NORMAL_CONFIG_ID, false);

  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_CONFIG_ID, false);

  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .CreateSocPerfClientAdapter()
      ->ApplySocPerfConfigByIdEx(SOC_PERF_WEB_GUSTURE_ID, false);
}
// #endif OHOS_PERFORMANCE_INC_FREQ

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
