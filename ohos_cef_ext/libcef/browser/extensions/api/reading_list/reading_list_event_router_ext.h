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
#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_

#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_reading_list_cef_delegate.h"

namespace extensions {

class CefReadingListEventRouter {
 public:
  static CefReadingListEventRouter& GetInstance();

  void ReadingListDidAddEntry(const OHOS::NWeb::NWebReadingListEntry& entry,
                              content::BrowserContext* browser_context);

  void ReadingListWillRemoveEntry(const OHOS::NWeb::NWebReadingListEntry& entry,
                                  content::BrowserContext* browser_context);

  void ReadingListDidUpdateEntry(const OHOS::NWeb::NWebReadingListEntry& entry,
                                 content::BrowserContext* browser_context);

 private:
  CefReadingListEventRouter() = default;
  ~CefReadingListEventRouter() = default;

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List event_args,
                     content::BrowserContext* browser_context);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_
