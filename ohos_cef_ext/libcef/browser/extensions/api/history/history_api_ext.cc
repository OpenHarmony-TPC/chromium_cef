/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/history/history_api.h"
#include "components/history/core/browser/history_types.h"
#include "extensions/browser/extension_function.h"
#include "ohos_nweb/src/nweb_common.h"

namespace extensions {

typedef OHOS::NWeb::NWebExtensionHistoryCefDelegate
    NWebExtensionHistoryCefDelegate;
typedef std::vector<api::history::VisitItem> VisitItemList;
typedef std::vector<api::history::HistoryItem> HistoryItemList;

namespace history = api::history;

namespace AddUrl = api::history::AddUrl;
namespace DeleteUrl = api::history::DeleteUrl;
namespace DeleteRange = api::history::DeleteRange;
namespace GetVisits = api::history::GetVisits;
namespace OnVisited = api::history::OnVisited;
namespace OnVisitRemoved = api::history::OnVisitRemoved;
namespace Search = api::history::Search;

namespace {
const char kInvalidUrlError[] = "Url is invalid.";

#define NWEB_BOOKMARKS_SAFE_FREE(ptr) \
  do {                                \
    if (ptr) {                        \
      free(ptr);                      \
      ptr = nullptr;                  \
    }                                 \
  } while (0)

void NWebExtensionHistoryParamRelease(NWebExtensionHistoryQueryInfo* param) {
  if (!param) {
    return;
  }

  NWEB_BOOKMARKS_SAFE_FREE(param->endTime);

  NWEB_BOOKMARKS_SAFE_FREE(param->maxResults);

  NWEB_BOOKMARKS_SAFE_FREE(param->startTime);

  NWEB_BOOKMARKS_SAFE_FREE(param->text);
}

// Note: If returns false, the caller MUST call
// NWebExtensionHistoryQueryInfoRelease to free any partially allocated memory
// in data
bool HistoryBuildParams(
    const std::optional<api::history::Search::Params>& params,
    NWebExtensionHistoryQueryInfo* data) {
  if (!params || !data) {
    return false;
  }

  if (params->query.start_time) {
    data->startTime = static_cast<double*>(calloc(1, sizeof(double)));
    if (!data->startTime) {
      return false;
    }
    *data->startTime = *(params->query.start_time);
    LOG(INFO) << "HistoryBuildParams, startTime:" << *data->startTime;
  }

  if (params->query.end_time) {
    data->endTime = static_cast<double*>(calloc(1, sizeof(double)));
    if (!data->endTime) {
      return false;
    }
    *data->endTime = *(params->query.end_time);
    LOG(INFO) << "HistoryBuildParams, endTime:" << *data->endTime;
  }

  if (!(params->query.text.empty())) {
    data->text = strdup(params->query.text.c_str());
    if (!data->text) {
      return false;
    }
  }

  if (params->query.max_results) {
    data->maxResults = static_cast<int*>(calloc(1, sizeof(int)));
    if (!data->maxResults) {
      return false;
    }
    *data->maxResults = *(params->query.max_results);
    LOG(INFO) << "HistoryBuildParams, maxResults:" << *data->maxResults;
  }

  return true;
}

api::history::VisitItem GetVisitItem(
    const OHOS::NWeb::NWebExtensionVisitItem& item) {
  api::history::VisitItem visit_item;
  visit_item.id = item.id;
  visit_item.is_local = item.isLocal;
  visit_item.visit_id = item.visitId;
  visit_item.referring_visit_id = item.referringVisitId;
  if (item.visitTime) {
    visit_item.visit_time = *item.visitTime;
  }

  api::history::TransitionType transition = api::history::TransitionType::kLink;
  switch (item.transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      transition = api::history::TransitionType::kLink;
      break;
    case ui::PAGE_TRANSITION_TYPED:
      transition = api::history::TransitionType::kTyped;
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      transition = api::history::TransitionType::kAutoBookmark;
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      transition = api::history::TransitionType::kAutoSubframe;
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      transition = api::history::TransitionType::kManualSubframe;
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      transition = api::history::TransitionType::kGenerated;
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      transition = api::history::TransitionType::kAutoToplevel;
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      transition = api::history::TransitionType::kFormSubmit;
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      transition = api::history::TransitionType::kReload;
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      transition = api::history::TransitionType::kKeyword;
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      transition = api::history::TransitionType::kKeywordGenerated;
      break;
    default:
      DCHECK(false);
  }

  visit_item.transition = transition;
  return visit_item;
}

api::history::HistoryItem GetHistoryItem(const NWebExtensionHistoryItem* item) {
  api::history::HistoryItem history_item;
  if (!item) {
    return history_item;
  }

  if (item->id) {
    history_item.id = std::string(item->id);
    LOG(INFO) << "GetHistoryItem, id:" << item->id;
  }

  if (item->url) {
    history_item.url = std::string(item->url);
  }

  if (item->title) {
    history_item.title = std::string(item->title);
  }

  if (item->lastVisitTime) {
    history_item.last_visit_time = *(item->lastVisitTime);
    LOG(INFO) << "GetHistoryItem, lastVisitTime:" << *(item->lastVisitTime);
  }

  if (item->typedCount) {
    history_item.typed_count = *(item->typedCount);
    LOG(INFO) << "GetHistoryItem, typedCount:" << *(item->typedCount);
  }

  if (item->visitCount) {
    history_item.visit_count = *(item->visitCount);
    LOG(INFO) << "GetHistoryItem, visitCount:" << *(item->visitCount);
  }

  return history_item;
}

bool ValidateUrl(const std::string& url_string, GURL* url, std::string* error) {
  GURL temp_url(url_string);
  if (!temp_url.is_valid()) {
    *error = kInvalidUrlError;
    return false;
  }
  url->Swap(&temp_url);
  return true;
}

}  // namespace

ExtensionFunction::ResponseAction HistoryGetVisitsFunction::Run() {
  std::optional<GetVisits::Params> params = GetVisits::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  call_history_get_visits_ = true;
  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->GetVisits(
      params->details.url,
      base::BindRepeating(&HistoryGetVisitsFunction::OnHistoryGetVisits,
                          weak_ptr_factory_.GetWeakPtr()));
  call_history_get_visits_ = false;

  if (did_respond()) {
    LOG(INFO) << "HistoryGetVistisFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistoryGetVistisFunction AddRef";
    return RespondLater();
  }

  return RespondNow(Error("not support history.getVisits"));
}

void HistoryGetVisitsFunction::OnHistoryGetVisits(
    const base::WeakPtr<HistoryGetVisitsFunction>& function,
    const std::string& error,
    const std::vector<OHOS::NWeb::NWebExtensionVisitItem>& results) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "HistoryGetVisitsFunction is empty!!!!";
    return;
  }

  if (error.empty()) {
    LOG(INFO) << "OnHistoryGetVisits, count:" << results.size();

    VisitItemList visit_item_vec;
    for (const auto& result : results) {
      visit_item_vec.push_back(GetVisitItem(result));
    }

    function->Respond(
        function->ArgumentList(GetVisits::Results::Create(visit_item_vec)));
  } else {
    function->Respond(function->Error(error));
  }

  if (!function->call_history_get_visits_) {
    LOG(INFO) << "HistoryGetVisitsFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction HistorySearchFunction::Run() {
  std::optional<api::history::Search::Params> params =
      api::history::Search::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  NWebExtensionHistoryQueryInfo param = {0};
  if (!HistoryBuildParams(params, &param)) {
    NWebExtensionHistoryParamRelease(&param);
    LOG(INFO) << "HistorySearchFunction failed to build parameters";
    return RespondNow(BadMessage());
  }
  call_history_search_ = true;

  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->Search(
      &param, base::BindRepeating(&HistorySearchFunction::OnHistorySearched,
                                  weak_ptr_factory_.GetWeakPtr()));

  call_history_search_ = false;
  NWebExtensionHistoryParamRelease(&param);

  if (did_respond()) {
    LOG(INFO) << "HistorySearchFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistorySearchFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support history.search"));
  }
}

// static
void HistorySearchFunction::OnHistorySearched(
    const base::WeakPtr<HistorySearchFunction>& function,
    const NWebExtensionHistoryItems* items) {
  DCHECK(function);
  if (!function || !items) {
    LOG(ERROR) << "OnHistorySearched is empty!!!!";
    return;
  }

  if (items->error) {
    LOG(WARNING) << "OnHistorySearched, error:" << items->error;
    function->Respond(function->Error(std::string(items->error)));
  } else {
    HistoryItemList history_item_vec;
    LOG(INFO) << "OnHistorySearched, count:" << items->count;
    for (uint32_t i = 0; i < items->count; i++) {
      history_item_vec.push_back(GetHistoryItem(&(items->items[i])));
    }

    function->Respond(function->ArgumentList(
        api::history::Search::Results::Create(history_item_vec)));
  }

  if (!function->call_history_search_) {
    LOG(INFO) << "OnHistorySearched Release";
    function->Release();
  }
}

void HistorySearchFunction::SearchComplete(::history::QueryResults results) {}

ExtensionFunction::ResponseAction HistoryAddUrlFunction::Run() {
  std::optional<AddUrl::Params> params = AddUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  call_history_add_url_ = true;
  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->AddUrl(
      url.spec().c_str(),
      base::BindRepeating(&HistoryAddUrlFunction::OnHistoryAddUrl,
                          weak_ptr_factory_.GetWeakPtr()));
  call_history_add_url_ = false;

  if (did_respond()) {
    LOG(INFO) << "HistoryAddUrlFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistoryAddUrlFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support history.search"));
  }
}

// static
void HistoryAddUrlFunction::OnHistoryAddUrl(
    const base::WeakPtr<HistoryAddUrlFunction>& function,
    const char* error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "HistoryAddUrlFunction is empty!!!!";
    return;
  }

  if (error && (strlen(error) > 0)) {
    function->Respond(function->Error(std::string(error)));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_history_add_url_) {
    LOG(INFO) << "HistoryAddUrlFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction HistoryDeleteUrlFunction::Run() {
  std::optional<DeleteUrl::Params> params = DeleteUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  call_history_delete_url_ = true;
  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->DeleteUrl(
      url.spec().c_str(),
      base::BindRepeating(&HistoryDeleteUrlFunction::OnHistoryDeleteUrl,
                          weak_ptr_factory_.GetWeakPtr()));
  call_history_delete_url_ = false;

  if (did_respond()) {
    LOG(INFO) << "HistoryDeleteUrlFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistoryDeleteUrlFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support history.search"));
  }
}

// static
void HistoryDeleteUrlFunction::OnHistoryDeleteUrl(
    const base::WeakPtr<HistoryDeleteUrlFunction>& function,
    const char* error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "HistoryDeleteUrlFunction is empty!!!!";
    return;
  }

  if (error && (strlen(error) > 0)) {
    function->Respond(function->Error(std::string(error)));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_history_delete_url_) {
    LOG(INFO) << "HistoryDeleteUrlFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction HistoryDeleteAllFunction::Run() {
  call_history_delete_all_ = true;
  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->DeleteAll(
      base::BindRepeating(&HistoryDeleteAllFunction::OnHistoryDeleteAll,
                          weak_ptr_factory_.GetWeakPtr()));
  call_history_delete_all_ = false;

  if (did_respond()) {
    LOG(INFO) << "HistoryDeleteAllFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistoryDeleteAllFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support history.search"));
  }
}

// static
void HistoryDeleteAllFunction::OnHistoryDeleteAll(
    const base::WeakPtr<HistoryDeleteAllFunction>& function,
    const char* error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "HistoryDeleteAllFunction is empty!!!!";
    return;
  }

  if (error && (strlen(error) > 0)) {
    function->Respond(function->Error(std::string(error)));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_history_delete_all_) {
    LOG(INFO) << "HistoryDeleteAllFunction Release";
    function->Release();
  }
}

void HistoryDeleteAllFunction::DeleteComplete() {}

ExtensionFunction::ResponseAction HistoryDeleteRangeFunction::Run() {
  std::optional<DeleteRange::Params> params =
      DeleteRange::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  if (!VerifyDeleteAllowed(&error)) {
    return RespondNow(Error(std::move(error)));
  }

  call_history_delete_range_ = true;
  bool success = NWebExtensionHistoryCefDelegate::GetInstance()->DeleteRange(
      params->range.start_time, params->range.end_time,
      base::BindRepeating(&HistoryDeleteRangeFunction::OnHistoryDeleteRange,
                          weak_ptr_factory_.GetWeakPtr()));
  call_history_delete_range_ = false;

  if (did_respond()) {
    LOG(INFO) << "HistoryDeleteRangeFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "HistoryDeleteRangeFunction AddRef";
    return RespondLater();
  }

  return RespondNow(Error("not support history.deleteRange"));
}

void HistoryDeleteRangeFunction::OnHistoryDeleteRange(
    const base::WeakPtr<HistoryDeleteRangeFunction>& function,
    const std::string& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "HistoryDeleteRangeFunction is empty!!!!";
    return;
  }

  if (error.empty()) {
    function->Respond(function->NoArguments());
  } else {
    function->Respond(function->Error(error));
  }

  if (!function->call_history_delete_range_) {
    LOG(INFO) << "HistoryDeleteRangeFunction Release";
    function->Release();
  }
}

}  // namespace extensions