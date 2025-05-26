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

#include "chrome/browser/extensions/api/cookies/cookies_api.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "cef/ohos_cef_ext/libcef/browser/extensions/web_extension_tab_manager.h"

namespace extensions {

ExtensionFunction::ResponseAction CookiesGetAllCookieStoresFunction::Run() {
  Profile* original_profile = Profile::FromBrowserContext(browser_context());
  DCHECK(original_profile);
  base::Value::List original_tab_ids;
 
  NWebExtensionTabQueryInfo queryInfo;
  std::vector<NWebExtensionTab> tabs = CefWebExtensionTabManager::GetInstance()->QueryTab(queryInfo);
  for (NWebExtensionTab tab : tabs) {
    original_tab_ids.Append(*tab.id);
  }
  // Return a list of all cookie stores with at least one open tab.
  std::vector<api::cookies::CookieStore> cookie_stores;
  if (!original_tab_ids.empty()) {
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        original_profile, std::move(original_tab_ids)));
  }
  return RespondNow(ArgumentList(
      api::cookies::GetAllCookieStores::Results::Create(cookie_stores)));
}

}  // namespace extensions
