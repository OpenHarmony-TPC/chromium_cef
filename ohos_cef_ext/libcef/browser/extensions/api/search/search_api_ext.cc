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

#include "chrome/browser/extensions/api/search/search_api.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/api/search.h"
#include "components/search_engines/util.h"
#include "libcef/browser/extensions/window_extensions_util.h"
#include "ohos_nweb/src/capi/browser_service/nweb_extension_search_types.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_search_cef_delegate.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_utils.h"

namespace extensions {
 
using extensions::api::search::Disposition;
 
ExtensionFunction::ResponseAction SearchQueryFunction::Run() {
  std::optional<api::search::Query::Params> params =
      api::search::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NWebExtensionSearchQueryInfo queryInfo;

  queryInfo.text = params->query_info.text;
  queryInfo.tab_id = params->query_info.tab_id;
  Disposition disposition = params->query_info.disposition;
  if (params->query_info.disposition > Disposition::kNone && disposition <= Disposition::kMaxValue) {
    queryInfo.disposition = static_cast<int>(params->query_info.disposition);
  }

  if (queryInfo.text.empty()) {
    return RespondNow(Error("Empty text parameter."));
  }
  if (queryInfo.tab_id && disposition != Disposition::kNone) {
    return RespondNow(Error("Cannot set both 'disposition' and 'tabId'."));
  }
  queryInfo.contextType = OHOS::NWeb::GetExtensionContextType(browser_context());
  queryInfo.includeIncognitoInfo = include_incognito_information();

  call_query_ = true;
  bool success = OHOS::NWeb::NWebExtensionSearchCefDelegate::Query(
      base::BindRepeating(&SearchQueryFunction::OnQuery, weak_ptr_factory_.GetWeakPtr()),
      queryInfo);
  call_query_ = false;

  if (did_respond()) {
    LOG(INFO) << "SearchQueryFunction did_respond";
    return AlreadyResponded();
  }
  if (success) {
    AddRef();
    LOG(INFO) << "SearchQueryFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support search.query"));
  }
}
 
void SearchQueryFunction::OnQuery(
    const base::WeakPtr<SearchQueryFunction>& function,
    const std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "SearchQueryFunction::OnQuery is empty!!!!";
    return;
  }

  if (error.has_value() && !error.value().empty()) {
    function->Respond(function->Error(error.value()));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_query_) {
    LOG(INFO) << "SearchQueryFunction Release";
    function->Release();
  }
}

}  // namespace extensions