/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ohos_cef_ext/libcef/browser/extensions/api/tabGroups/tab_groups_event_router_ext.h"
#include "ohos_cef_ext/libcef/browser/extensions/api/tabGroups/tab_groups_extensions_util.h"
#include "ohos_nweb/src/capi/browser_service/nweb_extension_tab_groups_types.h"

namespace extensions {

// static
CefTabGroupsEventRouter& CefTabGroupsEventRouter::GetInstance() {
  static CefTabGroupsEventRouter instance;
  return instance;
}

void CefTabGroupsEventRouter::DispatchTabGroupsCreatedEvent(
    content::BrowserContext* browserContext,
    const NWebExtensionTabGroup& tabGroup) {
  LOG(DEBUG) << "CefTabGroupsEventRouter::DispatchTabGroupsCreatedEvent";
  base::Value::List args;
  args.Append(GetTabGroupValue(tabGroup));

  DispatchEvent(events::TAB_GROUPS_ON_CREATED,
                api::tab_groups::OnCreated::kEventName,
                std::move(args), browserContext);
}

void CefTabGroupsEventRouter::DispatchTabGroupsMovedEvent(
    content::BrowserContext* browserContext,
    const NWebExtensionTabGroup& tabGroup) {
  LOG(DEBUG) << "CefTabGroupsEventRouter::DispatchTabGroupsMovedEvent";
  base::Value::List args;
  args.Append(GetTabGroupValue(tabGroup));

  DispatchEvent(events::TAB_GROUPS_ON_MOVED,
                api::tab_groups::OnMoved::kEventName,
                std::move(args), browserContext);
}

void CefTabGroupsEventRouter::DispatchTabGroupsRemovedEvent(
    content::BrowserContext* browserContext,
    const NWebExtensionTabGroup& tabGroup) {
  LOG(DEBUG) << "CefTabGroupsEventRouter::DispatchTabGroupsRemovedEvent";
  base::Value::List args;
  args.Append(GetTabGroupValue(tabGroup));

  DispatchEvent(events::TAB_GROUPS_ON_REMOVED,
                api::tab_groups::OnRemoved::kEventName,
                std::move(args), browserContext);
}

void CefTabGroupsEventRouter::DispatchTabGroupsUpdatedEvent(
    content::BrowserContext* browserContext,
    const NWebExtensionTabGroup& tabGroup) {
  LOG(DEBUG) << "CefTabGroupsEventRouter::DispatchTabGroupsUpdatedEvent";
  base::Value::List args;
  args.Append(GetTabGroupValue(tabGroup));

  DispatchEvent(events::TAB_GROUPS_ON_UPDATED,
                api::tab_groups::OnUpdated::kEventName,
                std::move(args), browserContext);
}

void CefTabGroupsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                            const std::string& event_name,
                                            base::Value::List args,
                                            content::BrowserContext* browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context);
  auto event = std::make_unique<Event>(histogram_value, event_name, std::move(args), browser_context);
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace extensions