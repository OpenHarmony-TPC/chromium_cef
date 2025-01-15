// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
#ifndef CEF_INCLUDE_CEF_EXTENSION_WINDOW_HANDLER_H_
#define CEF_INCLUDE_CEF_EXTENSION_WINDOW_HANDLER_H_
#pragma once
 
#include "ohos_nweb/src/capi/web_extension_window_items.h"
 
///
/// Implement this interface to handle extension's context menus events. The methods of
///  this class will be called on the UI thread.
///
class CefExtensionWindowHandler {
 public:
  ///
  /// Called when chrome.windows.getAll
  ///
  /*--cef()--*/
  virtual std::vector<WebExtensionWindow> OnGetAllWindows(
      const WebExtensionWindowQueryOptions& queryOptions) {return {};}
 protected:
  virtual ~CefExtensionWindowHandler() {}
};
 
#endif  // CEF_INCLUDE_CEF_EXTENSION_WINDOW_HANDLER_H_