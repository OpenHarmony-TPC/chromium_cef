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

#include "libcef/browser/extensions/windows_manager.h"
#include "libcef/browser/extensions/tab_extensions_util.h"


using content::WebContents;

namespace extensions {

namespace windows = api::windows;

namespace {

std::optional<std::vector<std::string>> GetWindowTypeStrs(
    std::optional<std::vector<api::windows::WindowType>> window_types) {
  if (!window_types) {
    return std::nullopt;
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
  return window_type_strs;
}

base::Value::List GetWindowList(std::vector<WebExtensionWindow> windows) {
  base::Value::List window_list;
  for (WebExtensionWindow window : windows) {
    base::Value::Dict dict;
    if (window.id)
      dict.Set("id", *window.id);
    if (window.type)
      dict.Set("type", *window.type);
    dict.Set("focused", window.focused);
    dict.Set("incognito", window.incognito);
    dict.Set("alwaysOnTop", window.alwaysOnTop);
    if (window.state)
      dict.Set("state", *window.state);
    if (window.left)
      dict.Set("left", *window.left);
    if (window.top)
      dict.Set("top", *window.top);
    if (window.width)
      dict.Set("width", *window.width);
    if (window.height)
      dict.Set("height", *window.height);
    if (window.sessionId)
      dict.Set("sessionId", *window.sessionId);
    dict.Set("tabs", GetTabValueList(window.tabs));

    window_list.Append(std::move(dict));
  }
  return window_list;
}

}  // namespace

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebExtensionWindowQueryOptions windowQueryOptions;
  std::optional<api::windows::QueryOptions> query_options = std::move(params->query_options);
  if (query_options) {
    if (query_options->populate) {
      windowQueryOptions.populate = *(query_options->populate);
    }
    windowQueryOptions.windowTypes = GetWindowTypeStrs(query_options->window_types);
  }
  std::vector<WebExtensionWindow> windows = CefWindowsManager::GetInstance()->GetAllWindows(windowQueryOptions);
  base::Value::List window_list = GetWindowList(windows);
  return RespondNow(WithArguments(std::move(window_list)));
}

}  // namespace extensions
