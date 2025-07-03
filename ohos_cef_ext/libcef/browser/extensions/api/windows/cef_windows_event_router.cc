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

#include "cef_windows_event_router.h"

#include "base/containers/contains.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/features/feature.h"
#include "libcef/browser/extensions/window_extensions_util.h"

namespace extensions {

namespace {

constexpr char kWindowTypesKey[] = "windowTypes";

std::vector<std::string> GetWindowTypeFilter(const base::Value::Dict* listener_filter) {
  std::vector<std::string> default_window_type_filter{"normal", "popup"};
  if (!listener_filter ||
      !listener_filter->contains(kWindowTypesKey)) {
    return default_window_type_filter;
  }
  const base::Value::List* window_types = listener_filter->FindList(kWindowTypesKey);
  std::vector<std::string> window_type_filter;
  for (const base::Value& type : *window_types) {
    if (type.is_string()) {
        window_type_filter.push_back(type.GetString());
    }
  }
  if (window_type_filter.size() == 0) {
    return default_window_type_filter;
  }
  return window_type_filter;
}

bool WillDispatchWindowEvent(
    std::string window_type,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  std::vector<std::string> window_type_filter = GetWindowTypeFilter(listener_filter);
  return base::Contains(window_type_filter, window_type);
}
}

//static
CefWindowsEventRouter* CefWindowsEventRouter::GetInstance() {
  static CefWindowsEventRouter instance;
  return &instance;
}

void CefWindowsEventRouter::DispatchWindowCreatedEvent(content::BrowserContext* browser_context,
                                                      const WebExtensionWindow& window) {
  LOG(DEBUG) << "CefWindowsEventRouter::DispatchWindowCreatedEvent";
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return;
  }
  base::Value::List args;
  args.Append(GetWindowValue(window));
  auto event = std::make_unique<Event>(events::WINDOWS_ON_CREATED,
                                       api::windows::OnCreated::kEventName,
                                       std::move(args),
                                       browser_context);
  event->will_dispatch_callback = base::BindRepeating(&WillDispatchWindowEvent, *window.type);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));
}

void CefWindowsEventRouter::DispatchWindowRemovedEvent(content::BrowserContext* browser_context,
                                                      const WebExtensionWindow& window) {
  LOG(DEBUG) << "CefWindowsEventRouter::DispatchWindowRemovedEvent";
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return;
  }
  base::Value::List args;
  args.Append(*window.id);
  auto event = std::make_unique<Event>(events::WINDOWS_ON_REMOVED,
                                       api::windows::OnRemoved::kEventName,
                                       std::move(args),
                                       browser_context);
  event->will_dispatch_callback = base::BindRepeating(&WillDispatchWindowEvent, *window.type);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));
}

void CefWindowsEventRouter::DispatchWindowBoundsChangedEvent(content::BrowserContext* browser_context,
                                                             const WebExtensionWindow& window) {
  LOG(DEBUG) << "CefWindowsEventRouter::DispatchWindowBoundsChangedEvent";
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return;
  }
  base::Value::List args;
  args.Append(GetWindowValue(window));
  auto event = std::make_unique<Event>(events::WINDOWS_ON_BOUNDS_CHANGED,
                                       api::windows::OnBoundsChanged::kEventName,
                                       std::move(args),
                                       browser_context);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));
}

void CefWindowsEventRouter::DispatchWindowFocusChangedEvent(content::BrowserContext* browser_context,
                                                            const WebExtensionWindow& window) {
  LOG(DEBUG) << "CefWindowsEventRouter::DispatchWindowFocusChangedEvent";
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return;
  }
  base::Value::List args;
  args.Append(*window.id);
  auto event = std::make_unique<Event>(events::WINDOWS_ON_FOCUS_CHANGED,
                                       api::windows::OnFocusChanged::kEventName,
                                       std::move(args),
                                       browser_context);
  event->will_dispatch_callback = base::BindRepeating(&WillDispatchWindowEvent, *window.type);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));
}

}  //  namespace extensions