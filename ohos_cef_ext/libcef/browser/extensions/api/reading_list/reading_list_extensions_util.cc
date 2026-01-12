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

#include "libcef/browser/extensions/api/reading_list/reading_list_extensions_util.h"

namespace extensions {

api::reading_list::ReadingListEntry ParseEntry(
    const OHOS::NWeb::NWebReadingListEntry& entry) {
  api::reading_list::ReadingListEntry reading_list_entry;
  reading_list_entry.url = entry.url;
  reading_list_entry.title = entry.title;
  reading_list_entry.has_been_read = entry.hasBeenRead;
  reading_list_entry.creation_time = entry.creationTime;
  reading_list_entry.last_update_time = entry.lastUpdateTime;
  return reading_list_entry;
}

}  // namespace extensions
