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

#include "libcef/browser/extensions/contents_extensions_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"

namespace extensions {

namespace {

bool InsertWebContents(std::vector<content::WebContents*>* vector,
                       content::WebContents* web_contents) {
  vector->push_back(web_contents);
  return false;  // Continue iterating.
}

}  // namespace

void GetAllGuestsForOwnerContents(content::WebContents* owner,
                                  std::vector<content::WebContents*>* guests) {
  content::BrowserPluginGuestManager* plugin_guest_manager =
      owner->GetBrowserContext()->GetGuestManager();
  plugin_guest_manager->ForEachGuest(
      owner, base::BindRepeating(InsertWebContents, guests));
}

content::WebContents* GetOwnerForGuestContents(content::WebContents* guest) {
  content::WebContentsImpl* guest_impl =
      static_cast<content::WebContentsImpl*>(guest);
  content::BrowserPluginGuest* plugin_guest =
      guest_impl->GetBrowserPluginGuest();
  if (plugin_guest) {
    return plugin_guest->owner_web_contents();
  }

  // Maybe it's a print preview dialog.
  auto print_preview_controller =
      g_browser_process->print_preview_dialog_controller();
  return print_preview_controller->GetInitiator(guest);
}


#if defined(OHOS_ARKWEB_EXTENSIONS)
content::WebContents* GetWebContentByTabId(int tab_id) {
  CefRefPtr<AlloyBrowserHostImpl> browser = nullptr;
  for (const auto& browser_info :
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser =
        static_cast<AlloyBrowserHostImpl*>(browser_info->browser().get());
    if (current_browser &&
        current_browser->GetTabId() == tab_id && tab_id >= 0) {
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
#endif

}  // namespace extensions
