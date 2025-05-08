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
 
#ifndef CEF_LIBCEF_BROWSER_WEBEXTENSION_TAB_MANAGER_H_
#define CEF_LIBCEF_BROWSER_WEBEXTENSION_TAB_MANAGER_H_
#pragma once
 
#include <string>
#include <mutex>
#include <memory>
 
#include "include/cef_web_extension_tab_api_handler.h"
#include "base/memory/raw_ptr.h"

class CefWebExtensionTabManager {
 public:
  static CefWebExtensionTabManager* GetInstance();
  void SetTabApiHandle(CefWebExtensionTabApiHandler* handle);
  std::unique_ptr<NWebExtensionTab> GetTab(int tab_id);
  std::vector<NWebExtensionTab> QueryTab(
      const NWebExtensionTabQueryInfo& queryInfo);
 
 private:
  static std::unique_ptr<CefWebExtensionTabManager> instance;
  static std::mutex mtx;
 
  raw_ptr<CefWebExtensionTabApiHandler> cef_webext_tab_handler_;
};
 
#endif  // CEF_LIBCEF_BROWSER_WEBEXTENSION_TAB_MANAGER_H_