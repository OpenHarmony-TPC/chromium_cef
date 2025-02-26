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

#include "cef/libcef/browser/extensions/api/windows/windows_api.h"

#include "chrome/browser/extensions/chrome_extension_function_details.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser_command_controller.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#endif

#include "libcef/browser/windows_manager.h"
#include "libcef/browser/extensions/tab_extensions_util.h"


using content::WebContents;

namespace extensions {

namespace cef {

namespace windows = api::windows;

namespace {

template <typename T>
class ApiParameterExtractor {
 public:
  explicit ApiParameterExtractor(absl::optional<T>& params)
      : params_(*params) {}
  ~ApiParameterExtractor() = default;

  bool populate_tabs() {
    if (params_->query_options && params_->query_options->populate)
      return *params_->query_options->populate;
    return false;
  }

  WindowController::TypeFilter type_filters() {
    if (params_->query_options && params_->query_options->window_types)
      return WindowController::GetFilterFromWindowTypes(
          *params_->query_options->window_types);
    return WindowController::kNoWindowFilter;
  }

 private:
  raw_ref<T> params_;
};

bool ExtensionHasLockedFullscreenPermission(const Extension* extension) {
  return extension && extension->permissions_data()->HasAPIPermission(
                          mojom::APIPermissionID::kLockWindowFullscreenPrivate);
}

std::optional<std::vector<std::string>> GetWindowTypeStrs(
  absl::optional<std::vector<api::windows::WindowType>> window_types) {
  if (!window_types) {
    return std::nullopt;
  }
  std::vector<std::string> window_type_strs;
  for (api::windows::WindowType type : *window_types) {
    switch (type) {
      case api::windows::WindowType::WINDOW_TYPE_NORMAL: window_type_strs.push_back("normal"); continue;
      case api::windows::WindowType::WINDOW_TYPE_POPUP: window_type_strs.push_back("popup"); continue;
      case api::windows::WindowType::WINDOW_TYPE_PANEL: window_type_strs.push_back("panel"); continue;
      case api::windows::WindowType::WINDOW_TYPE_APP: window_type_strs.push_back("app"); continue;
      case api::windows::WindowType::WINDOW_TYPE_DEVTOOLS: window_type_strs.push_back("devtools"); continue;
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
      dict.Set(tabs_constants::kIdKey, *window.id);
    if (window.type)
      dict.Set(tabs_constants::kWindowTypeKey, *window.type);
    dict.Set(tabs_constants::kFocusedKey, window.focused);
    dict.Set(tabs_constants::kIncognitoKey, window.incognito);
    dict.Set(tabs_constants::kAlwaysOnTopKey, window.alwaysOnTop);
    if (window.state)
      dict.Set(tabs_constants::kShowStateKey, *window.state);
    if (window.left)
      dict.Set(tabs_constants::kLeftKey, *window.left);
    if (window.top)
      dict.Set(tabs_constants::kTopKey, *window.top);
    if (window.width)
      dict.Set(tabs_constants::kWidthKey, *window.width);
    if (window.height)
      dict.Set(tabs_constants::kHeightKey, *window.height);
    if (window.sessionId)
      dict.Set("sessionId", *window.sessionId);
    dict.Set(tabs_constants::kTabsKey, GetTabValueList(window.tabs));

    window_list.Append(std::move(dict));
  }
  return window_list;
}

}  // namespace

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  absl::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::Get::Params> extractor(params);
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            extractor.type_filters(), &browser,
                                            &error)) {
    return RespondNow(Error(std::move(error)));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  absl::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::GetCurrent::Params> extractor(params);
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(
          this, extension_misc::kCurrentWindowId, extractor.type_filters(),
          &browser, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  absl::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::GetLastFocused::Params> extractor(params);
  // The WindowControllerList should contain a list of application,
  // browser and devtools windows.
  Browser* browser = nullptr;
  for (auto* controller : WindowControllerList::GetInstance()->windows()) {
    if (controller->GetBrowser() &&
        windows_util::CanOperateOnWindow(this, controller,
                                         extractor.type_filters())) {
      // TODO(devlin): Doesn't this mean that we'll use the last window in the
      // list if there is no active window? That seems wrong.
      // See https://crbug.com/809822.
      browser = controller->GetBrowser();
      if (controller->window()->IsActive())
        break;  // Use focused window.
    }
  }
  if (!browser)
    return RespondNow(Error(tabs_constants::kNoLastFocusedWindowError));

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  absl::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebExtensionWindowQueryOptions windowQueryOptions;
  absl::optional<api::windows::QueryOptions> query_options = std::move(params->query_options);
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

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  absl::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            WindowController::kNoWindowFilter,
                                            &browser, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (platform_util::IsBrowserLockedFullscreen(browser) &&
      !ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
  }

  WindowController* controller = browser->extension_window_controller();
  WindowController::Reason reason;
  if (!controller->CanClose(&reason)) {
    return RespondNow(Error(reason == WindowController::REASON_NOT_EDITABLE
                                ? tabs_constants::kTabStripNotEditableError
                                : kUnknownErrorDoNotUse));
  }
  controller->window()->Close();
  return RespondNow(NoArguments());
}

}  // namespace cef
}  // namespace extensions
