// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/ohos_cef_media_drm_bridge_client.h"
#include "base/logging.h"

void OhosCefMediaDrmBridgeClient::AddKeySystemUUIDMappings(
    KeySystemUuidMap* map) {}

media::OhosMediaDrmBridgeDelegate*
OhosCefMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  LOG(INFO) << "WiseplayDRM | " << __FUNCTION__;
  if (scheme_uuid == wiseplay_delegate_.GetUUID()) {
    LOG(INFO) << "WiseplayDRM | " << __FUNCTION__;
    return &wiseplay_delegate_;
  }
  LOG(ERROR) << "WiseplayDRM | " << __FUNCTION__;
  return nullptr;
}
