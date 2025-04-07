// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "extensions/browser/extension_web_contents_observer.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "components/sessions/content/session_tab_helper.h"
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
#include "ohos_nweb/src/cef_delegate/nweb_handler_delegate.h"
#include "libcef/browser/extensions/contents_extensions_util.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/extensions/tab_extensions_util.h"
#include "ohos_cef_ext/libcef/browser/extensions/web_extension_tab_manager.h"
#include "ohos_cef_ext/libcef/browser/extensions/browser_extensions_util.h"

using zoom::ZoomController;

namespace extensions {
namespace cef {

namespace keys = extensions::tabs_constants;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

const char kNotImplementedError[] = "Not implemented";
const char kJavaScriptUrlsNotAllowedInTabsUpdate[] =
    "JavaScript URLs are not allowed in chrome.tabs.update. Use "
    "chrome.tabs.executeScript instead.";

void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case zoom::ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kAutomatic;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerOrigin;
      break;
    case zoom::ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kAutomatic;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
    case zoom::ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kManual;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
    case zoom::ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kDisabled;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
  }
}

#define TABS_API_WINDOW_ID_CURRENT   -2
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

  base::Value::List create_results;
  create_results.reserve(1);
  create_results.Append(GetTabValue(*web_extension_tab));

  return RespondNow(ArgumentList(std::move(create_results)));
}

TabsCreateFunction::TabsCreateFunction() {}


void TabsCreateFunction::GetCreateParams(
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

void TabsCreateFunction::OnTabCreated(const base::WeakPtr<TabsCreateFunction>& function,
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

BaseAPIFunction::BaseAPIFunction() : cef_details_(this) {}

content::WebContents* BaseAPIFunction::GetWebContents(int tab_id) {
  if (tab_id >= 0) {
    content::WebContents* web_contents = GetWebContentByTabId(tab_id);
    if (!web_contents) {
      LOG(INFO) << "cannot find WebContents by tabId:" << tab_id;
    } else {
      return web_contents;
    }
  } else {
    CefRefPtr<AlloyBrowserHostImpl> browser = cef_details_.GetCurrentBrowser();
    if (browser && browser->web_contents()) {
      LOG(INFO) << "find WebContents by GetCurrentBrowser tabId:" << tab_id;
      return browser->web_contents();
    }
  }
  
  LOG(INFO) << "GetWebContents fallback to use Identifier tabId:" << tab_id;
  if (tab_id >= 0) {
    // May be an invalid tabId or in the wrong BrowserContext.
    CefRefPtr<AlloyBrowserHostImpl> browser =
        GetBrowserByTabIdForExtension(tab_id, browser_context());
    if (!browser || !browser->web_contents() ||
        !cef_details_.CanAccessBrowser(browser)) {
      LOG(INFO) << "cannot find WebContents by Identifier:" << tab_id;
      error_ = ErrorUtils::FormatErrorMessage(
            ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id));
      return nullptr;
    }
    return browser->web_contents();
  }

  LOG(INFO) << "cannot find WebContents by tabId:" << tab_id;
  error_ = ErrorUtils::FormatErrorMessage(
        ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id));
  return nullptr;
}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tab_id_ = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id_);
  if (!web_contents) {
    for (const auto& browser_info :
         CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
      CefRefPtr<AlloyBrowserHostImpl> current_browser = browser_info->browser()->AsAlloyBrowserHostImpl();
      if (current_browser && current_browser->web_contents() &&
          current_browser->client()) {
         web_contents = current_browser->web_contents();
         break;
      }
    }
  }
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }
  web_contents_ = web_contents;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // compatible with phone
  if (!OHOS::NWeb::NWebHandlerDelegate::HasExtensionListener()) {
    if (params->update_properties.url.has_value()) {
      std::string updated_url = *params->update_properties.url;
      if (!UpdateURL(updated_url, tab_id_, &error_)) {
        return RespondNow(Error(std::move(error_)));
      }
    }
    return RespondNow(GetResult());
  }

  NWebExtensionTabUpdateProperties update_properties;
  bool no_change = true;

  if (params->update_properties.url.has_value()) {
    GURL url;
    auto url_expected = ExtensionTabUtil::PrepareURLForNavigation(
        *params->update_properties.url, extension(), browser_context());
    if (!url_expected.has_value()) {
      error_ = std::move(url_expected.error());
      return RespondNow(Error(std::move(error_)));
    }
    url = *url_expected;

    const bool is_javascript_scheme = url.SchemeIs(url::kJavaScriptScheme);
    // JavaScript URLs are forbidden in chrome.tabs.update().
    if (is_javascript_scheme) {
      error_ = kJavaScriptUrlsNotAllowedInTabsUpdate;
      return RespondNow(Error(std::move(error_)));
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
    if (opener_id == tab_id_) {
      return RespondNow(Error("Cannot set a tab's opener to itself."));
    }

    update_properties.openerTabId = *params->update_properties.opener_tab_id;
    no_change = false;
  }

  if (params->update_properties.auto_discardable.has_value()) {
    update_properties.autoDiscardable =
        *params->update_properties.auto_discardable;
    no_change = false;
  }

  if (!no_change) {
    web_contents_->WebExtensionUpdateTab(tab_id_, &update_properties);
  }
  return RespondNow(GetResult());
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  GURL url;
  auto url_expected = ExtensionTabUtil::PrepareURLForNavigation(
      url_string, extension(), browser_context());
  if (url_expected.has_value()) {
    url = *url_expected;
  } else {
    *error = std::move(url_expected.error());
    return false;
  }

  const bool is_javascript_scheme = url.SchemeIs(url::kJavaScriptScheme);
  // JavaScript URLs are forbidden in chrome.tabs.update().
  if (is_javascript_scheme) {
    *error = kJavaScriptUrlsNotAllowedInTabsUpdate;
    return false;
  }

  content::NavigationController::LoadURLParams load_params(url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = extension()->origin();
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents_->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  // Marking the navigation as initiated via an API means that the focus
  // will stay in the omnibox - see https://crbug.com/1085779.
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  web_contents_->GetController().LoadURLWithParams(load_params);

  DCHECK_EQ(url,
            web_contents_->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback()) {
    return NoArguments();
  }

  return ArgumentList(tabs::Get::Results::Create(cef_details_.CreateTabObject(
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents_),
      /*opener_browser_id=*/-1, /*active=*/true, tab_id_)));
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::optional<tabs::SetZoom::Params> params =
      tabs::SetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_)) {
    return RespondNow(Error(std::move(error_)));
  }

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level =
      params->zoom_factor > 0
          ? blink::ZoomFactorToZoomLevel(params->zoom_factor)
          : zoom_controller->GetDefaultZoomLevel();

  auto client = base::MakeRefCounted<ExtensionZoomRequestClient>(extension());
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(tabs_constants::kCannotZoomDisabledTabError));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::optional<tabs::GetZoom::Params> params =
      tabs::GetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }

  double zoom_level =
      zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = blink::ZoomFactorToZoomLevel(zoom_level);

  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  std::optional<tabs::SetZoomSettings::Params> params =
      tabs::SetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }

  GURL url(web_contents->GetVisibleURL());
  std::string error;
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_)) {
    return RespondNow(Error(std::move(error_)));
  }

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZoomSettingsScope::kPerOrigin &&
      params->zoom_settings.mode != tabs::ZoomSettingsMode::kAutomatic &&
      params->zoom_settings.mode != tabs::ZoomSettingsMode::kNone) {
    return RespondNow(Error(tabs_constants::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZoomSettingsMode::kNone:
    case tabs::ZoomSettingsMode::kAutomatic:
      switch (params->zoom_settings.scope) {
        case tabs::ZoomSettingsScope::kNone:
        case tabs::ZoomSettingsScope::kPerOrigin:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZoomSettingsScope::kPerTab:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZoomSettingsMode::kManual:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZoomSettingsMode::kDisabled:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::optional<tabs::GetZoomSettings::Params> params =
      tabs::GetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetDefaultZoomLevel());

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  int tab_id = -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  std::optional<tabs::CaptureVisibleTab::Params> params =
      tabs::CaptureVisibleTab::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  window_id_ = params->window_id;
  LOG(INFO) << "WebExtension TabsCaptureVisibleTabFunction window id " << *window_id_;

  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tab_id_ = params->tab_id;
  LOG(INFO) << "WebExtension TabsDiscardFunction tab_id " << *tab_id_;

  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::optional<tabs::Duplicate::Params> params =
      tabs::Duplicate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tab_id_ = params->tab_id;
  LOG(INFO) << "WebExtension TabsDuplicateFunction tab_id " << tab_id_;

  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsGoBackFunction::Run() {
  std::optional<tabs::GoBack::Params> params =
      tabs::GoBack::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsGoForwardFunction::Run() {
  std::optional<tabs::GoForward::Params> params =
      tabs::GoForward::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsGroupFunction::Run() {
  std::optional<tabs::Group::Params> params =
      tabs::Group::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::optional<tabs::Move::Params> params =
      tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
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

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  std::optional<tabs::Remove::Params> params =
      tabs::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsUngroupFunction::Run() {
  std::optional<tabs::Ungroup::Params> params =
      tabs::Ungroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kNotImplementedError));
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::optional<tabs::Reload::Params> params =
      tabs::Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool bypass_cache = false;
  if (params->reload_properties && params->reload_properties->bypass_cache) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents) {
    return RespondNow(Error(std::move(error_)));
  }

  web_contents->GetController().Reload(
      bypass_cache ? content::ReloadType::BYPASSING_CACHE
                   : content::ReloadType::NORMAL,
      true);

  return RespondNow(NoArguments());
}

}  // namespace cef
}  // namespace extensions
