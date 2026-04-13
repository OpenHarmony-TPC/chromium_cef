  /*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_MENU_MANAGER_EXT_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_MENU_MANAGER_EXT_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "cef/libcef/browser/menu_model_impl.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_DEVTOOLS)
class CefMenuManager;
class CefMenuManagerExt {
  public:
    explicit CefMenuManagerExt(raw_ptr<CefMenuManager> menu_manager);
    CefMenuManagerExt(const CefMenuManagerExt&) = delete;
    CefMenuManagerExt& operator = (const CefMenuManagerExt&) = delete;

    ~CefMenuManagerExt() = default;

    void SetMenuItems(content::WebContents* contents,
                      const content::ContextMenuParams& params);
    CefRefPtr<CefMenuModelImpl> GetContextMenuModel();
    void onContextMenuSelected(int command_id);
    void onContextMenuClosed();

  private:
    raw_ptr<CefMenuManager> menu_manager_;
    CefRefPtr<CefMenuModelImpl> model_;
    content::WebContents* contents_;
    GURL link_followed_;
};
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
#endif // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_MENU_MANAGER_EXT_H_