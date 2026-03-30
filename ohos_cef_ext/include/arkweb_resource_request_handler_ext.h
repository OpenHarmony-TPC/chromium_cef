/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

class CefRewriteUrlCallback : public virtual CefBaseRefCounted {
 public:
  virtual void OnComplete(const CefString& url) = 0;
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

  virtual void OnRewriteUrlForNavigationAsync(
      const CefString& original_url,
      const CefString& referrer,
      int transition_type,
      bool is_key_request,
      CefRefPtr<CefRewriteUrlCallback> callback) {}
};

#endif  // OHOS_CEF_EXT_INCLUDE_ARKWEB_RESOURCE_REQUEST_HANDLER_EXT_H_
