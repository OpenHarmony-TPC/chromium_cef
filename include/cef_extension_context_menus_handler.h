// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
#ifndef CEF_INCLUDE_CEF_EXTENSION_CONTEXT_MENUS_HANDLER_H_
#define CEF_INCLUDE_CEF_EXTENSION_CONTEXT_MENUS_HANDLER_H_
#pragma once
 
#include "ohos_nweb/src/capi/nweb_context_menus_item.h"
 
///
/// Implement this interface to handle extension's context menus events. The methods of
///  this class will be called on the UI thread.
///
class CefExtensionContextMenusHandler {
 public:
  ///
  /// Called when chrome.contextMenus.create
  ///
  /*--cef()--*/
  virtual void OnContextMenusCreate(const std::string& extension_id, const NWebContextMenusItem& menu_item) {}
 
  ///
  /// Called when chrome.contextMenus.update
  ///
  /*--cef()--*/
  virtual void OnContextMenusUpdate(const std::string& extension_id, const NWebContextMenusItem& menu_item) {}
 
  ///
  /// Called when chrome.contextMenus.remove
  ///
  /*--cef()--*/
  virtual void OnContextMenusRemove(const std::string& extension_id, const std::string& menu_item_id) {}
 
  ///
  /// Called when chrome.contextMenus.removeAll
  ///
  /*--cef()--*/
  virtual void OnContextMenusRemoveAll(const std::string& extension_id) {}
 
 protected:
  virtual ~CefExtensionContextMenusHandler() {}
};
 
#endif  // CEF_INCLUDE_CEF_EXTENSION_CONTEXT_MENUS_HANDLER_H_