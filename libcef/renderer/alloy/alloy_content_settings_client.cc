// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/alloy_content_settings_client.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/web/web_local_frame.h"

AlloyContentSettingsClient::AlloyContentSettingsClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
}

AlloyContentSettingsClient::~AlloyContentSettingsClient() {}

bool AlloyContentSettingsClient::ShouldAutoupgradeMixedContent() {
  return render_frame()->GetBlinkPreferences().allow_mixed_content_upgrades;
}

void AlloyContentSettingsClient::OnDestruct() {
  delete this;
}
