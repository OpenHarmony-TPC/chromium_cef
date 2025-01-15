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
 
#include "web_extension_tab_manager.h"
 
#include "base/logging.h"
 
std::unique_ptr<CefWebExtensionTabManager> CefWebExtensionTabManager::instance = nullptr;
std::mutex CefWebExtensionTabManager::mtx;
 
CefWebExtensionTabManager* CefWebExtensionTabManager::GetInstance() {
  std::lock_guard<std::mutex> lock(mtx);
  if (!instance) {
    instance = std::make_unique<CefWebExtensionTabManager>();
  }
  return instance.get();
}
 
void CefWebExtensionTabManager::SetTabApiHandle(CefWebExtensionTabApiHandler* handler) {
  cef_webext_tab_handler_ = handler;
}
 
std::unique_ptr<NWebExtensionTab> CefWebExtensionTabManager::GetTab(int tab_id) {
  if (!cef_webext_tab_handler_) {
    LOG(ERROR) << "cef GetTab handler is null";
    return nullptr;
  }
  return cef_webext_tab_handler_->GetTab(tab_id);
}
 
std::vector<NWebExtensionTab> CefWebExtensionTabManager::QueryTab(const NWebExtensionTabQueryInfo& queryInfo) {
  if (!cef_webext_tab_handler_) {
    LOG(ERROR) << "cef QueryTab handler is null";
    return {};
  }
  return cef_webext_tab_handler_->QueryTab(queryInfo);
}