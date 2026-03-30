// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OH_GIN_JAVASCRIPT_BRIDGE_ERRORS_H
#define OH_GIN_JAVASCRIPT_BRIDGE_ERRORS_H

#include "cef/ohos_cef_ext/libcef/common/mojom/oh_gin_javascript_bridge.mojom.h"

namespace NWEB {

__attribute__((visibility("default"))) const char*
OhGinJavascriptBridgeErrorToString(content::mojom::OhGinJavascriptBridgeError error);
}  // namespace NWEB
#endif
