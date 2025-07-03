/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/browser_context.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

namespace extensions {

bool IsOffTheRecord(content::WebContents* contents) {
  if (!contents) {
    return true;
  }
  auto browser_context = contents->GetBrowserContext();
  if (!browser_context || browser_context->IsOffTheRecord()) {
    // extensions not supported in OTR modes yet
    return true;
  }
  return false;
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    const std::string& url) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabUpdatedEvent(tab_id, contents,
                                               changed_property_names, url);
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabUpdatedEvent(
      tab_id, contents, changed_property_names, std::move(changeInfo));
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo,
    std::unique_ptr<NWebExtensionTab> tab) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabUpdatedEvent(
      tab_id, contents, std::move(changeInfo), std::move(changeInfo));
}

void TabsWindowsAPI::TabActived(int tab_id,
                                int window_id,
                                content::WebContents* contents) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabActiveEvent(tab_id, window_id, contents);
}

void TabsWindowsAPI::TabRemoved(int tab_id,
                                bool isWindowClosing
                                int windowId,
                                content::WebContents* contents) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabRemovedvent(tab_id, isWindowClosing, windowId, contents);
}

void TabsWindowsAPI::TabAttached(int tab_id,
                                content::WebContents* contents,
                                int32_t newPosition,
                                int32_t newWindowId) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabAttachedEvent(tab_id, contents, newPosition, newWindowId);
}

void TabsWindowsAPI::TabCreated(int tab_id,
                                content::WebContents* contents,
                                std::unique_ptr<NWebExtensionTab> tab) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabCreatedEvent(tab_id, contents, std::move(tab));
}

void TabsWindowsAPI::TabDetached(content::WebContents* contents,
                                 int tab_id,
                                 int oldPosition,
                                 int oldWindowId) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabDetachedEvent(contents, tab_id, oldPosition, oldWindowId);
}

void TabsWindowsAPI::TabHighlighted(content::WebContents* contents,
                                    NWebExtensionTabHighlightInfo& highlightInfo) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabHighlightedEvent(contents, highlightInfo);
}

void TabsWindowsAPI::TabMoved(content::WebContents* contents,
                              int tab_id,
                              std::unique_ptr<NWebExtensionTabMoveInfo> moveInfo) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabMovedEvent(contents, tab_id, std::move(moveInfo));
}

void TabsWindowsAPI::TabReplaced(content::WebContents* contents,
                                 int32_t addedTabId,
                                 int32_t removedTabId) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabReplacedEvent(contents, addedTabId, removedTabId);
}

void TabsWindowsAPI::TabZoomChange(content::WebContents* contents,
                                   std::unique_ptr<NWebExtensionTabZoomChangeInfo> tabZoomChangeInfo) {
  if (IsOffTheRecord(contents)) {
    return;
  }

  tabs_event_router()->DispatchTabZoomChangeEvent(contents, std::move(tabZoomChangeInfo));
}

}  // namespace extensions
