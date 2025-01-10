// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/alloy/alloy_ohos_media_drm_bridge_client.h"

#include "base/logging.h"

AlloyOHOSMediaDrmBridgeClient::AlloyOHOSMediaDrmBridgeClient() {}

AlloyOHOSMediaDrmBridgeClient::~AlloyOHOSMediaDrmBridgeClient() {}

void AlloyOHOSMediaDrmBridgeClient::AddKeySystemUUIDMappings(
    KeySystemUuidMap* map) {}

media::OHOSMediaDrmBridgeDelegate*
AlloyOHOSMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const media::UUID& scheme_uuid) {
  LOG(INFO) << "[DRM]" << __func__;
  if (scheme_uuid == widevine_delegate_.GetUUID()) {
    return &widevine_delegate_;
  }
  return nullptr;
}
