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

#include "chrome/browser/extensions/api/reading_list/reading_list_api.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_api_constants.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "reading_list_extensions_util.h"
#include "url/gurl.h"

namespace extensions {

//////////////////////////////////////////////////////////////////////////////
/////////////////////// ReadingListAddEntryFunction //////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListAddEntryFunction::ReadingListAddEntryFunction() = default;
ReadingListAddEntryFunction::~ReadingListAddEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListAddEntryFunction::Run() {
  auto params = api::reading_list::AddEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto url = GURL(params->entry.url);
  if (!url.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
  }

  OHOS::NWeb::NWebAddEntryOptions options = {
      .hasBeenRead = params->entry.has_been_read,
      .url = params->entry.url,
      .title = params->entry.title};
  if (OHOS::NWeb::NWebExtensionReadingListCefDelegate::GetInstance().AddEntry(
          options,
          base::BindRepeating(&ReadingListAddEntryFunction::OnEntryAdded,
                              weak_ptr_factory_.GetWeakPtr()))) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support readingList.addEntry"));
}

void ReadingListAddEntryFunction::DoEntryAdded(const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }

  Release();
}

// static
void ReadingListAddEntryFunction::OnEntryAdded(
    const base::WeakPtr<ReadingListAddEntryFunction>& function,
    const std::string& error) {
  if (!function) {
    return;
  }

  function->DoEntryAdded(error);
}

//////////////////////////////////////////////////////////////////////////////
///////////////////// ReadingListRemoveEntryFunction /////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListRemoveEntryFunction::ReadingListRemoveEntryFunction() = default;
ReadingListRemoveEntryFunction::~ReadingListRemoveEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListRemoveEntryFunction::Run() {
  auto params = api::reading_list::RemoveEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto url = GURL(params->info.url);
  if (!url.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
  }

  if (OHOS::NWeb::NWebExtensionReadingListCefDelegate::GetInstance()
          .RemoveEntry(params->info.url,
                       base::BindRepeating(
                           &ReadingListRemoveEntryFunction::OnEntryRemoved,
                           weak_ptr_factory_.GetWeakPtr()))) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support readingList.removeEntry"));
}

void ReadingListRemoveEntryFunction::DoEntryRemoved(const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }

  Release();
}

// static
void ReadingListRemoveEntryFunction::OnEntryRemoved(
    const base::WeakPtr<ReadingListRemoveEntryFunction>& function,
    const std::string& error) {
  if (!function) {
    return;
  }

  function->DoEntryRemoved(error);
}

//////////////////////////////////////////////////////////////////////////////
///////////////////// ReadingListUpdateEntryFunction /////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListUpdateEntryFunction::ReadingListUpdateEntryFunction() = default;
ReadingListUpdateEntryFunction::~ReadingListUpdateEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListUpdateEntryFunction::Run() {
  auto params = api::reading_list::UpdateEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->info.title.has_value() &&
      !params->info.has_been_read.has_value()) {
    return RespondNow(Error(reading_list_api_constants::kNoUpdateProvided));
  }

  auto url = GURL(params->info.url);
  if (!url.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
  }

  OHOS::NWeb::NWebUpdateEntryOptions options = {
      .url = params->info.url,
      .hasBeenRead = params->info.has_been_read,
      .title = params->info.title};
  if (OHOS::NWeb::NWebExtensionReadingListCefDelegate::GetInstance()
          .UpdateEntry(options,
                       base::BindRepeating(
                           &ReadingListUpdateEntryFunction::OnEntryUpdated,
                           weak_ptr_factory_.GetWeakPtr()))) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support readingList.updateEntry"));
}

void ReadingListUpdateEntryFunction::DoEntryUpdated(const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }

  Release();
}

// static
void ReadingListUpdateEntryFunction::OnEntryUpdated(
    const base::WeakPtr<ReadingListUpdateEntryFunction>& function,
    const std::string& error) {
  if (!function) {
    return;
  }

  function->DoEntryUpdated(error);
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// ReadingListQueryFunction ///////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListQueryFunction::ReadingListQueryFunction() = default;
ReadingListQueryFunction::~ReadingListQueryFunction() = default;

ExtensionFunction::ResponseAction ReadingListQueryFunction::Run() {
  auto params = api::reading_list::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->info.url.has_value()) {
    auto url = GURL(params->info.url.value());
    if (!url.is_valid()) {
      return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
    }
  }

  OHOS::NWeb::NWebQueryEntryOptions options = {
      .hasBeenRead = params->info.has_been_read,
      .url = params->info.url,
      .title = params->info.title};
  if (OHOS::NWeb::NWebExtensionReadingListCefDelegate::GetInstance().QueryEntry(
          options,
          base::BindRepeating(&ReadingListQueryFunction::OnEntryQueried,
                              weak_ptr_factory_.GetWeakPtr()))) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support readingList.query"));
}

void ReadingListQueryFunction::DoEntryQueried(
    const std::string& error,
    const std::vector<OHOS::NWeb::NWebReadingListEntry>& entries) {
  if (!error.empty()) {
    Respond(Error(error));
    Release();
    return;
  }

  std::vector<api::reading_list::ReadingListEntry> matching_entries;
  for (const auto& entry : entries) {
    matching_entries.emplace_back(ParseEntry(entry));
  }

  auto response = ArgumentList(
      api::reading_list::Query::Results::Create(std::move(matching_entries)));
  Respond(std::move(response));
  Release();
}

// static
void ReadingListQueryFunction::OnEntryQueried(
    const base::WeakPtr<ReadingListQueryFunction>& function,
    const std::string& error,
    const std::vector<OHOS::NWeb::NWebReadingListEntry>& entries) {
  if (!function) {
    return;
  }

  function->DoEntryQueried(error, entries);
}

}  // namespace extensions
