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

#include "ohos_cef_ext/libcef/browser/extensions/api/tabGroups/tab_groups_extensions_util.h"

namespace extensions {

base::Value::Dict GetTabGroupValue(const NWebExtensionTabGroup& tabGroup) {
  base::Value::Dict dict;
  dict.Set("collapsed", tabGroup.collapsed);
  dict.Set("color", tabGroup.color);
  dict.Set("id", tabGroup.id);
  if (tabGroup.shared) {
    dict.Set("shared", *tabGroup.shared);
  }
  if (tabGroup.title) {
    dict.Set("title", *tabGroup.title);
  }
  dict.Set("windowId", tabGroup.window_id);
  return dict;
}

base::Value::List GetTabGroupValueList(const std::vector<NWebExtensionTabGroup>& tabGroups) {
  base::Value::List tab_group_list;
  for (const NWebExtensionTabGroup& tabGroup : tabGroups) {
    tab_group_list.Append(GetTabGroupValue(tabGroup));
  }
  return tab_group_list;
}

}  // namespace extensions