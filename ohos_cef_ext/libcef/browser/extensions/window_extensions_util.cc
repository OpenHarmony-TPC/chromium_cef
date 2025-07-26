/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "base/logging.h"
#include "libcef/browser/extensions/window_extensions_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "libcef/browser/extensions/tab_extensions_util.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_tab_cef_delegate.h"

namespace extensions {

const char kIdKey[] = "id";
const char kWindowTypeKey[] = "type";
const char kFocusedKey[] = "focused";
const char kIncognitoKey[] = "incognito";
const char kAlwaysOnTopKey[] = "alwaysOnTop";
const char kShowStateKey[] = "state";
const char kLeftKey[] = "left";
const char kTopKey[] = "top";
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
const char kTabsKey[] = "tabs";

base::Value::Dict GetWindowValue(const WebExtensionWindow& window) {
  base::Value::Dict dict;
  if (window.id)
    dict.Set(kIdKey, *window.id);
  if (window.type)
    dict.Set(kWindowTypeKey, *window.type);
  dict.Set(kFocusedKey, window.focused);
  dict.Set(kIncognitoKey, window.incognito);
  dict.Set(kAlwaysOnTopKey, window.alwaysOnTop);
  if (window.state)
    dict.Set(kShowStateKey, *window.state);
  if (window.left)
    dict.Set(kLeftKey, *window.left);
  if (window.top)
    dict.Set(kTopKey, *window.top);
  if (window.width)
    dict.Set(kWidthKey, *window.width);
  if (window.height)
    dict.Set(kHeightKey, *window.height);
  if (window.sessionId)
    dict.Set("sessionId", *window.sessionId);
  dict.Set(kTabsKey, GetTabValueList(window.tabs));
  return dict;
}

base::Value::List GettWindowValueList(const std::vector<WebExtensionWindow>& windows) {
  base::Value::List window_list;
  for (WebExtensionWindow window : windows) {
    window_list.Append(GetWindowValue(window));
  }
  return window_list;
}

int32_t GetCurrentWindowId(content::WebContents* webcontents, int32_t default_window_id) {
  if (!webcontents) {
    return default_window_id;
  }
  int32_t tab_id = webcontents->ExtensionGetTabId();
  if (tab_id > 0) {
    std::unique_ptr<NWebExtensionTab> tab =
        OHOS::NWeb::NWebExtensionTabCefDelegate::GetTab(tab_id);
    if (!tab || tab->nwebId  < 0) {
      return default_window_id;
    }
    return tab->windowId;
  } else {
    int nweb_id = webcontents->GetNWebId();
    int window_id =
        extensions::CefExtensionWindowIdManager::GetWindowId(nweb_id);
    if (window_id < 0) {
      return default_window_id;
    }
    return window_id;
  }
}

}  // namespace extensions