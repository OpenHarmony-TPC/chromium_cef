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

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    const std::string& url) {
  auto browser_context = contents->GetBrowserContext();
  if (!browser_context || browser_context->IsOffTheRecord()) {
    // extensions not supported in OTR modes yet
    return;
  }

  tabs_event_router()->DispatchTabUpdatedEvent(tab_id, contents,
                                               changed_property_names, url);
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) {
  auto browser_context = contents->GetBrowserContext();
  if (!browser_context || browser_context->IsOffTheRecord()) {
    // extensions not supported in OTR modes yet
    return;
  }

  tabs_event_router()->DispatchTabUpdatedEvent(
      tab_id, contents, changed_property_names, std::move(changeInfo));
}

void TabsWindowsAPI::TabActived(int tab_id,
                                int window_id,
                                content::WebContents* contents) {
  auto browser_context = contents->GetBrowserContext();
  if (!browser_context || browser_context->IsOffTheRecord()) {
    // extensions not supported in OTR modes yet
    return;
  }
  tabs_event_router()->DispatchTabActiveEvent(tab_id, window_id, contents);
}

}  // namespace extensions
