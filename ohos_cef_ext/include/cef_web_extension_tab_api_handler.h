/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
 
#ifndef CEF_INCLUDE_CEF_WEB_EXTENSION_TAB_API_HANDLER_H_
#define CEF_INCLUDE_CEF_WEB_EXTENSION_TAB_API_HANDLER_H_
#pragma once
 
#include "include/cef_base.h"
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
 
class CefWebExtensionTabApiHandler {
 public:
  // chrome.tabs.get(tabid)
  virtual std::unique_ptr<NWebExtensionTab> GetTab(int tab_id) {
    return nullptr;
  }
  // chrome.tabs.query(queryInfo)
  virtual std::vector<NWebExtensionTab> QueryTab(
      const NWebExtensionTabQueryInfo& queryInfo) {
    return {};
  }
 
 protected:
  virtual ~CefWebExtensionTabApiHandler() {}
};
 
#endif  // CEF_INCLUDE_CEF_WEB_EXTENSION_TAB_API_HANDLER_H_