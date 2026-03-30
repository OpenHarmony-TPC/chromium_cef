// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/common/javascript/oh_gin_javascript_bridge_errors.h"

#include "base/notreached.h"
namespace NWEB {
const char* OhGinJavascriptBridgeErrorToString(
    content::mojom::OhGinJavascriptBridgeError error) {
  switch (error) {
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeNoError:
      return "No error";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeUnknownObjectId:
      return "Unknown Ace object ID";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeObjectIsGone:
      return "Ace object is gone";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeMethodNotFound:
      return "Method not found";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeAccessToObjectGetClassIsBlocked:
      return "Access to Ace is blocked";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeJavaExceptionRaised:
      return "Ace exception was raised during method invocation";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeNonAssignableTypes:
      return "The type of the object passed to the method is incompatible "
             "with the type of method's argument";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeRenderFrameDeleted:
      return "RenderFrame has been deleted";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgePermissionDenied:
      return "Jsb Permission Denied";
    case content::mojom::OhGinJavascriptBridgeError::kOhGinJavascriptBridgeClientDeleted:
      return "Client has been deleted";
  }
  NOTREACHED();
}
}  // namespace NWEB
