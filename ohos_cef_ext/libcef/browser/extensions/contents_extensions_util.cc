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

#include "ohos_cef_ext/libcef/browser/extensions/contents_extensions_util.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/browser_info_manager.h"
namespace extensions {

content::WebContents* GetWebContentByTabId(int tab_id) {
  CefRefPtr<AlloyBrowserHostImpl> browser = nullptr;
  for (const auto& browser_info :
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser = browser_info->browser()->AsAlloyBrowserHostImpl();
    if (current_browser &&
        current_browser->ExtensionGetTabId() == tab_id && tab_id >= 0) {
      browser = current_browser;
      break;
    }
  }
 
  if (browser) {
    return browser->web_contents();
  } else {
    return nullptr;
  }
}

}