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

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EXTENSIONS_UTIL_H_

#include "chrome/common/extensions/api/reading_list.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_reading_list_cef_delegate.h"

namespace extensions {

api::reading_list::ReadingListEntry ParseEntry(
    const OHOS::NWeb::NWebReadingListEntry& entry);

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EXTENSIONS_UTIL_H_
