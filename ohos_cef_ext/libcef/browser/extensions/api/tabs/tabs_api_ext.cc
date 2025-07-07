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

#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "libcef/browser/extensions/web_extension_tab_manager.h"
#include "libcef/browser/extensions/tab_extensions_util.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/extensions/extension_function_details.h"
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
#include "ohos_nweb/src/cef_delegate/nweb_handler_delegate.h"

namespace extensions {

namespace tabs = api::tabs;

namespace {

const char kNotImplementedError[] = "Not implemented";
const char kJavaScriptUrlsNotAllowedInTabsUpdate[] =
    "JavaScript URLs are not allowed in chrome.tabs.update. Use "
    "chrome.tabs.executeScript instead.";
const char kSetOpenerError[] = "Cannot set a tab's opener to itself.";

#define TABS_API_WINDOW_ID_CURRENT (-2)
void SetQueryInfoCurrentWindowId(content::WebContents* webcontents,
                                 NWebExtensionTabQueryInfo& queryInfo) {
  if (!webcontents) {
    queryInfo.currentWindowId = TABS_API_WINDOW_ID_CURRENT;
    return;
  }

  int32_t tab_id = webcontents->ExtensionGetTabId();
  if (tab_id > 0) {
    std::unique_ptr<NWebExtensionTab> tab =
        CefWebExtensionTabManager::GetInstance()->GetTab(tab_id);
    if (!tab || tab->nwebId < 0) {
      queryInfo.currentWindowId = TABS_API_WINDOW_ID_CURRENT;
    } else {
      queryInfo.currentWindowId = tab->windowId;
    }
  } else {
    int nweb_id = webcontents->GetNWebId();
    int window_id =
        extensions::CefExtensionWindowIdManager::GetWindowId(nweb_id);
    if (window_id < 0) {
      queryInfo.currentWindowId = TABS_API_WINDOW_ID_CURRENT;
    } else {
      queryInfo.currentWindowId = window_id;
    }
  }
}

std::optional<NWebExtensionTabStatus> GetQueryStatus(api::tabs::TabStatus status) {
  std::optional<NWebExtensionTabStatus> tabStatus = std::nullopt;
  switch (status) {
    case api::tabs::TabStatus::kUnloaded:
      tabStatus = NWebExtensionTabStatus::UNLOADED;
    break;
    case api::tabs::TabStatus::kLoading:
      tabStatus = NWebExtensionTabStatus::LOADING;
    break;
    case api::tabs::TabStatus::kComplete:
      tabStatus = NWebExtensionTabStatus::COMPLETE;
    break;
    default:
    break;
  }
  return tabStatus;
}

std::optional<WebExtensionWindowType> GetQueryWindowType(api::tabs::WindowType window_type) {
  std::optional<WebExtensionWindowType> queryWindowType;
  switch (window_type) {
    case api::tabs::WindowType::kNormal:
      queryWindowType = WebExtensionWindowType::NORMAL;
    break;
    case api::tabs::WindowType::kPopup:
      queryWindowType = WebExtensionWindowType::POPUP;
    break;
    case api::tabs::WindowType::kPanel:
      queryWindowType = WebExtensionWindowType::PANEL;
    break;
    case api::tabs::WindowType::kApp:
      queryWindowType = WebExtensionWindowType::APP;
    break;
    case api::tabs::WindowType::kDevtools:
      queryWindowType = WebExtensionWindowType::DEVTOOLS;
    break;
    default:
    break;
  }
  return queryWindowType;
}

bool GetTabUpdateProperties(TabsUpdateFunction* self,
    int tab_id, std::optional<tabs::Update::Params>& params,
    NWebExtensionTabUpdateProperties& update_properties, bool& no_change,
    std::string& error) {
  if (params->update_properties.url.has_value()) {
    GURL url;
    auto url_expected = ExtensionTabUtil::PrepareURLForNavigation(
        *params->update_properties.url, self->extension(), self->browser_context());
    if (!url_expected.has_value()) {
      error = std::move(url_expected.error());
      return false;
    }
    url = *url_expected;
    const bool is_javascript_scheme = url.SchemeIs(url::kJavaScriptScheme);
    // JavaScript URLs are forbidden in chrome.tabs.update().
    if (is_javascript_scheme) {
      error = kJavaScriptUrlsNotAllowedInTabsUpdate;
      return false;
    }
    update_properties.url = url.spec();
    no_change = false;
  }
  if (params->update_properties.active.has_value()) {
    update_properties.active = *params->update_properties.active;
    no_change = false;
  }
  if (params->update_properties.highlighted.has_value()) {
    update_properties.highlighted = *params->update_properties.highlighted;
    no_change = false;
  }
  if (params->update_properties.pinned.has_value()) {
    update_properties.pinned = *params->update_properties.pinned;
    no_change = false;
  }
  if (params->update_properties.muted.has_value()) {
    update_properties.muted = *params->update_properties.muted;
    no_change = false;
  }
  if (params->update_properties.opener_tab_id.has_value()) {
    int opener_id = *params->update_properties.opener_tab_id;
    if (opener_id == tab_id) {
      error = kSetOpenerError;
      return false;
    }
    update_properties.openerTabId = *params->update_properties.opener_tab_id;
    no_change = false;
  }
  if (params->update_properties.auto_discardable.has_value()) {
    update_properties.autoDiscardable =
        *params->update_properties.auto_discardable;
    no_change = false;
  }
 
  return true;
}

content::WebContents* GetClientAvailableContent() {
  content::WebContents* contents = nullptr;
  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser = browser_info->browser()->AsAlloyBrowserHostImpl();
    if (current_browser && current_browser->web_contents() &&
        current_browser->client()) {
       contents = current_browser->web_contents();
       break;
    }
  }
  return contents;
}

}  // namespace

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  LOG(DEBUG) << "TabsGetFunction Run";
  std::optional<tabs::Get::Params> params =
      tabs::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int tab_id = params->tab_id;
  std::unique_ptr<NWebExtensionTab> web_extension_tab =
      CefWebExtensionTabManager::GetInstance()->GetTab(tab_id);
  if (!web_extension_tab) {
    return RespondNow(Error(kNotImplementedError));
  }
  if (web_extension_tab->nwebId <= 0) {
    LOG(DEBUG) << "tab no arguments";
    return RespondNow(NoArguments());
  }

  base::Value::List get_results;
  get_results.reserve(1);
  get_results.Append(GetTabValue(*web_extension_tab));

  return RespondNow(ArgumentList(std::move(get_results)));
}

void GetCreateParams(
    std::optional<tabs::Create::Params>& params,
    NWebTabCreateInfo& create_info) {
  if (params->create_properties.active) {
    create_info.active = params->create_properties.active.value();
  }

  if (params->create_properties.index) {
    create_info.index = params->create_properties.index.value();
  }

  if (params->create_properties.opener_tab_id) {
    create_info.openerTabId = params->create_properties.opener_tab_id.value();
  }

  if (params->create_properties.pinned) {
    create_info.pinned = params->create_properties.pinned.value();
  }

  if (params->create_properties.window_id) {
    create_info.windowId = params->create_properties.window_id.value();
  }
}

void TabsCreateFunction::OnTabCreated(
    const base::WeakPtr<TabsCreateFunction>& function,
    const NWebExtensionTab* tab) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnTabCreated is empty!!!!";
    return;
  }
  if (tab->nwebId <= 0) {
    function->Respond(function->Error("create error"));
  } else {
  function->Respond(function->has_callback()
          ? function->WithArguments(base::Value(GetTabValue(*tab)))
          : function->NoArguments());
  }

  if (!function->call_create_tab_) {
    LOG(INFO) << "TabsCreateFunction Release";
    function->Release();
  }
}

void OnCreateTabForExtension(const NWebExtensionTab* tab) {
}

void TabsCreateFunction::CreateTabForExtension(std::string& url) {
  NWebTabCreateInfo create_info;
  create_info.url = url;

  OHOS::NWeb::NWebHandlerDelegate::OnCreateTab(
    create_info, base::BindRepeating(&OnCreateTabForExtension));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NWebTabCreateInfo create_info;
  GetCreateParams(params, create_info);

  GURL url;
  if (params->create_properties.url) {
    auto result = ExtensionTabUtil::PrepareURLForNavigation(
        *params->create_properties.url, extension(), browser_context());
    if (!result.has_value()) {
      return RespondNow(Error(result.error()));
    }
    url = std::move(*result);
  }
  create_info.url = url.spec();

  call_create_tab_ =true;
  bool success = OHOS::NWeb::NWebHandlerDelegate::OnCreateTab(
      create_info, base::BindRepeating(&TabsCreateFunction::OnTabCreated,
                                       weak_ptr_factory_.GetWeakPtr()));
  call_create_tab_ =false;

  if (did_respond()) {
    LOG(INFO) << "TabsCreateFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "TabsCreateFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support tabs.create"));
  }
}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* contents = nullptr;
  std::string error;

  if (tab_id > 0) {
    if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
        include_incognito_information(), &contents)) {
      error = ErrorUtils::FormatErrorMessage(
          ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id));
      return RespondNow(Error(std::move(error)));
    }
  } else {
    contents = GetClientAvailableContent();
  }

  if (!contents) {
    error = ErrorUtils::FormatErrorMessage(
        ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id));
    return RespondNow(Error(std::move(error)));
  }
  web_contents_ = contents;

  // compatible with phone
  if (!OHOS::NWeb::NWebHandlerDelegate::HasExtensionListener()) {
    if (params->update_properties.url.has_value()) {
      std::string updated_url = *params->update_properties.url;
      if (!UpdateURL(updated_url, tab_id, &error)) {
        return RespondNow(Error(std::move(error)));
      }
    }
    return RespondNow(GetResult());
  }

  NWebExtensionTabUpdateProperties update_properties;
  bool no_change = true;
  if (!GetTabUpdateProperties(this, tab_id, params, update_properties,
      no_change, error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (!no_change) {
    web_contents_->WebExtensionUpdateTab(tab_id, &update_properties);
  }
  return RespondNow(GetResult());
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  LOG(DEBUG) << "TabsQueryFunction Run";

  NWebExtensionTabQueryInfo queryInfo;
  content::WebContents* webcontents = GetSenderWebContents();
  SetQueryInfoCurrentWindowId(webcontents, queryInfo);

  queryInfo.currentWindow = params->query_info.current_window;
  queryInfo.active = params->query_info.active;
  queryInfo.audible = params->query_info.audible;
  queryInfo.autoDiscardable = params->query_info.auto_discardable;
  queryInfo.windowId = params->query_info.window_id;
  queryInfo.discarded = params->query_info.discarded;
  queryInfo.groupId = params->query_info.group_id;
  queryInfo.highlighted = params->query_info.highlighted;
  queryInfo.index = params->query_info.index;
  queryInfo.lastFocusedWindow = params->query_info.last_focused_window;
  queryInfo.muted = params->query_info.muted;
  queryInfo.pinned = params->query_info.pinned;
  queryInfo.status = GetQueryStatus(params->query_info.status);
  queryInfo.title = params->query_info.title;
  if (params->query_info.url) {
    std::vector<std::string> urls;
    if (params->query_info.url->as_string) {
      urls.push_back(*params->query_info.url->as_string);
    } else if (params->query_info.url->as_strings) {
      urls.swap(*params->query_info.url->as_strings);
    }
    queryInfo.url = urls;
  } else {
    queryInfo.url = std::nullopt;
  }

  queryInfo.windowType = GetQueryWindowType(params->query_info.window_type);
  std::vector<NWebExtensionTab> tabs = CefWebExtensionTabManager::GetInstance()->QueryTab(queryInfo);
  base::Value::List tab_list = GetTabValueList(tabs);
  return RespondNow(WithArguments(std::move(tab_list)));
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback()) {
    return NoArguments();
  }

  return ArgumentList(tabs::Get::Results::Create(
      CefExtensionFunctionDetails::CreateTabObject(
          AlloyBrowserHostImpl::GetBrowserForContents(web_contents_),
          -1, true, web_contents_->ExtensionGetTabId())));
}

}  // namespace extensions
