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

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_CONTENTS_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_CONTENTS_EXTENSIONS_UTIL_H_

#include <vector>

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Populates |guests| with all guest WebContents with the specified |owner|.
void GetAllGuestsForOwnerContents(content::WebContents* owner,
                                  std::vector<content::WebContents*>* guests);

// Returns the WebContents that owns the specified |guest|, if any.
content::WebContents* GetOwnerForGuestContents(content::WebContents* guest);

#if defined(OHOS_ARKWEB_EXTENSIONS)
content::WebContents* GetWebContentByTabId(int tab_id);
#endif

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_CONTENTS_EXTENSIONS_UTIL_H_
