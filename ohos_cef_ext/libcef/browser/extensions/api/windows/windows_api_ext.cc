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

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "libcef/browser/extensions/window_extensions_util.h"
#include "ohos_nweb/src/nweb_common.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_window_cef_delegate.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_window_delegate_handler.h"

namespace extensions {

typedef OHOS::NWeb::NweExtensionWindowCefDelegate NweExtensionWindowCefDelegate;
typedef OHOS::NWeb::NweExtensionWindowDelegateHandler NweExtensionWindowDelegateHandler;

namespace windows = api::windows;

namespace {

std::optional<std::vector<std::string>> GetWindowTypeStrs(
    std::optional<std::vector<api::windows::WindowType>> window_types) {
  std::vector<std::string> default_window_type_strs{"normal", "popup"};
  if (!window_types) {
    return default_window_type_strs;
  }
  std::vector<std::string> window_type_strs;
  for (api::windows::WindowType type : *window_types) {
    switch (type) {
      case api::windows::WindowType::kNormal: window_type_strs.push_back("normal"); continue;
      case api::windows::WindowType::kPopup: window_type_strs.push_back("popup"); continue;
      case api::windows::WindowType::kPanel: window_type_strs.push_back("panel"); continue;
      case api::windows::WindowType::kApp: window_type_strs.push_back("app"); continue;
      case api::windows::WindowType::kDevtools: window_type_strs.push_back("devtools"); continue;
      default: continue;
    }
  }
  if (window_type_strs.size() == 0) {
    return default_window_type_strs;
  }
  return window_type_strs;
}

WebExtensionWindowQueryOptions GetWindowQueryOptions(std::optional<api::windows::QueryOptions>& query_options) {
  WebExtensionWindowQueryOptions options;
  if (!query_options) {
    return options;
  }
  if (query_options->populate) {
    options.populate = *(query_options->populate);
  }
  options.windowTypes = GetWindowTypeStrs(query_options->window_types);
  return options;
}

std::optional<std::string> GetCreateTypeStr(api::windows::CreateType type) {
  switch (type) {
  case windows::CreateType::kNormal:
    return "normal";
  case windows::CreateType::kPopup:
    return "popup";  
  case windows::CreateType::kPanel:
    return "panel";  
  default:
    return std::nullopt;
  }
}

std::optional<std::string> GetWindowStateStr(api::windows::WindowState state) {
  switch (state) {
    case windows::WindowState::kNormal:
      return "normal";
    case windows::WindowState::kMinimized:
      return "minimized";
    case windows::WindowState::kMaximized:
      return "maximized";
    case windows::WindowState::kFullscreen:
      return "fullscreen";
    case windows::WindowState::kLockedFullscreen:
      return "locked-fullscreen";
    default:
      return std::nullopt;
  }
}

std::optional<std::vector<std::string>> GetWindowUrls(
    std::optional<windows::Create::Params::CreateData>& create_data) {
  if (!create_data->url) {
    return std::nullopt;
  }
  std::vector<std::string> urls;
  // First, get all the URLs the client wants to open.
  if (create_data->url->as_string) {
    urls.push_back(std::move(*create_data->url->as_string));
  } else if (create_data->url->as_strings) {
    urls = std::move(*create_data->url->as_strings);
  }
  return urls;
}

WebExtensionWindowCreateData GetWindowCreateData(std::optional<windows::Create::Params::CreateData>& create_data) {
  WebExtensionWindowCreateData data;
  if (!create_data) {
    return data;
  }
  if (create_data->focused) {
    data.focused = *create_data->focused;
  }
  if (create_data->height) {
    data.height = *create_data->height;
  }
  if (create_data->incognito) {
    data.incognito = *create_data->incognito;
  }
  if (create_data->left) {
    data.left = *create_data->left;
  }
  if (create_data->set_self_as_opener) {
    data.setSelfAsOpener = *create_data->set_self_as_opener;
  }
  data.stateStr = GetWindowStateStr(create_data->state);
  if (create_data->tab_id) {
    data.tabId = *create_data->tab_id;
  }
  if (create_data->top) {
    data.top = *create_data->top;
  }
  data.typeStr = GetCreateTypeStr(create_data->type);
  data.urls = GetWindowUrls(create_data);
  if (create_data->width) {
    data.width = *create_data->width;
  }
  return data;
}

WebExtensionWindowUpdateInfo GetWindowUpdateInfo(windows::Update::Params::UpdateInfo& update_info) {
  WebExtensionWindowUpdateInfo info;
  if (update_info.draw_attention) {
    info.drawAttention = *update_info.draw_attention;
  }
  if (update_info.focused) {
    info.focused = *update_info.focused;
  }
  if (update_info.height) {
    info.height = *update_info.height;
  }
  if (update_info.left) {
    info.left = *update_info.left;
  }
  info.stateStr = GetWindowStateStr(update_info.state);
  if (update_info.top) {
    info.top = *update_info.top;
  }
  if (update_info.width) {
    info.width = *update_info.width;
  }
  return info;
}

}  // namespace

std::optional<WebExtensionWindow> getWindow(int id, std::optional<api::windows::QueryOptions>& query_options,
                                            content::WebContents* webcontents) {
  int window_id = id;
  if (window_id == extension_misc::kCurrentWindowId) {
    window_id = GetCurrentWindowId(webcontents, extension_misc::kCurrentWindowId);
  }
  WebExtensionWindowQueryOptions windowQueryOptions = GetWindowQueryOptions(query_options);

  if (IsNativeApiEnable()) {
    return NweExtensionWindowCefDelegate::GetInstance()->OnGetWindow(window_id, windowQueryOptions);
  }
  return NweExtensionWindowDelegateHandler::GetInstance()->OnGetWindow(window_id, windowQueryOptions);
}

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int window_id = params->window_id;
  WebExtensionWindowQueryOptions windowQueryOptions = GetWindowQueryOptions(params->query_options);

  std::optional<WebExtensionWindow> window = getWindow(window_id, params->query_options,
                                                       GetSenderWebContents());
  if (!window) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(extensions::ExtensionTabUtil::kWindowNotFoundError,
                                                           base::NumberToString(window_id))));
  }
  return RespondNow(WithArguments(GetWindowValue(*window)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);  

  std::optional<WebExtensionWindow> window = getWindow(extension_misc::kCurrentWindowId, params->query_options,
                                                       GetSenderWebContents());
  if (!window) {
    return RespondNow(Error(extensions::ExtensionTabUtil::kNoCurrentWindowError));
  }
  return RespondNow(WithArguments(GetWindowValue(*window)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);  

  WebExtensionWindowQueryOptions windowQueryOptions = GetWindowQueryOptions(params->query_options);
  std::optional<WebExtensionWindow> window;

  if (IsNativeApiEnable()) {
    window = NweExtensionWindowCefDelegate::GetInstance()->OnGetLastFocusedWindow(windowQueryOptions);
  } else {
    window = NweExtensionWindowDelegateHandler::GetInstance()->OnGetLastFocusedWindow(windowQueryOptions);
  }
  if (!window) {
    return RespondNow(Error(extensions::tabs_constants::kNoLastFocusedWindowError));
  }
  return RespondNow(WithArguments(GetWindowValue(*window)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebExtensionWindowQueryOptions windowQueryOptions = GetWindowQueryOptions(params->query_options);
  std::vector<WebExtensionWindow> windows;
  if (IsNativeApiEnable()) {
    windows = NweExtensionWindowCefDelegate::GetInstance()->OnGetAllWindows(windowQueryOptions);
  } else {
    windows = NweExtensionWindowDelegateHandler::GetInstance()->OnGetAllWindows(windowQueryOptions);
  }
  base::Value::List window_list = GettWindowValueList(windows);
  return RespondNow(WithArguments(std::move(window_list)));
}

// static
void WindowsCreateFunction::OnCreateWindow(
    const base::WeakPtr<WindowsCreateFunction>& function,
    const std::optional<WebExtensionWindow>& window,
    const std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnCreateWindow is empty!!!!";
    return;
  }

  if (!window || error) {
    std::string errorMessage = error ? *error : "create window fail";
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->has_callback()
            ? function->WithArguments(GetWindowValue(*window))
            : function->NoArguments());
  }

  if (!function->call_create_window_) {
    LOG(INFO) << "WindowsCreateFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DCHECK(extension() || source_context_type() == mojom::ContextType::kWebUi ||
         source_context_type() == mojom::ContextType::kUntrustedWebUi);
  std::optional<windows::Create::Params::CreateData>& create_data =
      params->create_data;

  WebExtensionWindowCreateData data = GetWindowCreateData(create_data);
  // Second, resolve, validate and convert them to GURLs.
  if (data.urls) {
    std::vector<std::string> urls;
    for (auto& url_string : *data.urls) {
      GURL url;
      auto url_expected = ExtensionTabUtil::PrepareURLForNavigation(
          url_string, extension(), browser_context());
      if (!url_expected.has_value()) {
        return RespondNow(Error(std::move(url_expected.error())));
      }
      url = *url_expected;
      urls.push_back(url.spec());
    }
    data.urls = urls;
  }

  call_create_window_ = true;
  bool success;
  if (IsNativeApiEnable()) {
    success = NweExtensionWindowCefDelegate::GetInstance()->OnCreateWindow(
        data, base::BindRepeating(&WindowsCreateFunction::OnCreateWindow,
                                  weak_ptr_factory_.GetWeakPtr()));
  } else {
    success = NweExtensionWindowDelegateHandler::GetInstance()->OnCreateWindow(
        data, base::BindRepeating(&WindowsCreateFunction::OnCreateWindow,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  call_create_window_ = false;

  if (did_respond()) {
    LOG(INFO) << "WindowsCreateFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "WindowsCreateFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support windows.create"));
  }
}

// static
void WindowsUpdateFunction::OnUpdateWindow(
    const base::WeakPtr<WindowsUpdateFunction>& function,
    const std::optional<WebExtensionWindow>& window,
    const std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnUpdateWindow is empty!!!!";
    return;
  }

  if (!window || error) {
    std::string errorMessage = error ? *error : "update window fail";
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->has_callback()
            ? function->WithArguments(GetWindowValue(*window))
            : function->NoArguments());
  }

  if (!function->call_update_window_) {
    LOG(INFO) << "WindowsUpdateFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::optional<windows::Update::Params> params =
      windows::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  WebExtensionWindowUpdateInfo info = GetWindowUpdateInfo(params->update_info);

  int window_id = params->window_id;
  if (window_id == extension_misc::kCurrentWindowId) {
    window_id = GetCurrentWindowId(GetSenderWebContents(), extension_misc::kCurrentWindowId);
  }

  call_update_window_ = true;
  bool success;
  if (IsNativeApiEnable()) {
    success = NweExtensionWindowCefDelegate::GetInstance()->OnUpdateWindow(
        window_id, info, base::BindRepeating(&WindowsUpdateFunction::OnUpdateWindow,
                                             weak_ptr_factory_.GetWeakPtr()));
  } else {
    success = NweExtensionWindowDelegateHandler::GetInstance()->OnUpdateWindow(
        window_id, info, base::BindRepeating(&WindowsUpdateFunction::OnUpdateWindow,
                                             weak_ptr_factory_.GetWeakPtr()));
  }
  call_update_window_ = false;

  if (did_respond()) {
    LOG(INFO) << "WindowsUpdateFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "WindowsUpdateFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support windows.update"));
  }
}

// static
void WindowsRemoveFunction::OnRemoveWindow(
    const base::WeakPtr<WindowsRemoveFunction>& function,
    const std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnRemoveWindow is empty!!!!";
    return;
  }

  if (error) {
    function->Respond(function->Error(*error));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_remove_window_) {
    LOG(INFO) << "WindowsRemoveFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int window_id = params->window_id;
  if (window_id == extension_misc::kCurrentWindowId) {
    window_id = GetCurrentWindowId(GetSenderWebContents(), extension_misc::kCurrentWindowId);
  }

  call_remove_window_ = true;
  bool success;
  if (IsNativeApiEnable()) {
    success = NweExtensionWindowCefDelegate::GetInstance()->OnRemoveWindow(
        window_id, base::BindRepeating(&WindowsRemoveFunction::OnRemoveWindow,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    success = NweExtensionWindowDelegateHandler::GetInstance()->OnRemoveWindow(
        window_id, base::BindRepeating(&WindowsRemoveFunction::OnRemoveWindow,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
  call_remove_window_ = false;

  if (did_respond()) {
    LOG(INFO) << "WindowsRemoveFunction did_respond";
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    LOG(INFO) << "WindowsRemoveFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support windows.remove"));
  }
}

}  // namespace extensions
