/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "reading_list_event_router_ext.h"

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/event_router.h"
#include "reading_list_extensions_util.h"

namespace extensions {

CefReadingListEventRouter& CefReadingListEventRouter::GetInstance() {
  static CefReadingListEventRouter instance;
  return instance;
}

void CefReadingListEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    content::BrowserContext* browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context);
  if (!event_router) {
    LOG(INFO) << "failed to get event router";
    return;
  }

  event_router->BroadcastEvent(std::make_unique<Event>(
      histogram_value, event_name, std::move(event_args),
      Profile::FromBrowserContext(browser_context)));
}

void CefReadingListEventRouter::ReadingListDidAddEntry(
    const OHOS::NWeb::NWebReadingListEntry& entry,
    content::BrowserContext* browser_context) {
  auto args(api::reading_list::OnEntryAdded::Create(ParseEntry(entry)));

  DispatchEvent(events::READING_LIST_ON_ENTRY_ADDED,
                api::reading_list::OnEntryAdded::kEventName, std::move(args),
                browser_context);
}

void CefReadingListEventRouter::ReadingListWillRemoveEntry(
    const OHOS::NWeb::NWebReadingListEntry& entry,
    content::BrowserContext* browser_context) {
  auto args(api::reading_list::OnEntryRemoved::Create(ParseEntry(entry)));

  // Even though we dispatch the event in ReadingListWillRemoveEntry() (i.e.,
  // the entry is still in the model at this point), we can safely dispatch it
  // as `onEntryRemoved` (past tense) to the extension. The entry is removed
  // synchronously after this, so there's no way the extension could still
  // see the entry in the list.
  DispatchEvent(events::READING_LIST_ON_ENTRY_REMOVED,
                api::reading_list::OnEntryRemoved::kEventName, std::move(args),
                browser_context);
}

void CefReadingListEventRouter::ReadingListDidUpdateEntry(
    const OHOS::NWeb::NWebReadingListEntry& entry,
    content::BrowserContext* browser_context) {
  auto args(api::reading_list::OnEntryUpdated::Create(ParseEntry(entry)));

  DispatchEvent(events::READING_LIST_ON_ENTRY_UPDATED,
                api::reading_list::OnEntryUpdated::kEventName, std::move(args),
                browser_context);
}

}  // namespace extensions
