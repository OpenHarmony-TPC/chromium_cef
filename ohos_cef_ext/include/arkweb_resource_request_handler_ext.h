// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_CEF_EXT_INCLUDE_ARKWEB_RESOURCE_REQUEST_HANDLER_EXT_H_
#define OHOS_CEF_EXT_INCLUDE_ARKWEB_RESOURCE_REQUEST_HANDLER_EXT_H_
#pragma once

#include "arkweb_request_ext.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_scheme.h"

///
/// CefInterceptCallback
///
class CefInterceptCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Continue.
  ///
  virtual void ContinueLoad(CefRefPtr<CefResourceHandler> resource_handler) {}
};

///
/// Extended from CefRequest
///
class ArkWebResourceRequestHandlerExt
    : public virtual CefResourceRequestHandler,
      public virtual CefBaseRefCounted {
 public:
  virtual CefRefPtr<ArkWebResourceRequestHandlerExt>
  AsArkWebResourceRequestHandlerExt() override {
    return this;
  }

  ///
  /// GetResourceHandlerByIO.
  ///
  virtual void GetResourceHandlerByIO(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefInterceptCallback> callback,
      CefRefPtr<CefSchemeHandlerFactory> scheme_factory,
      const CefString& scheme) {}
};

#endif  // OHOS_CEF_EXT_INCLUDE_ARKWEB_RESOURCE_REQUEST_HANDLER_EXT_H_
