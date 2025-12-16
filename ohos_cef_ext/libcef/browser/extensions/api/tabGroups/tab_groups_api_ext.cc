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

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "components/tab_groups/tab_group_color.h"
#include "ohos_cef_ext/libcef/browser/extensions/api/tabGroups/tab_groups_extensions_util.h"
#include "ohos_cef_ext/libcef/browser/extensions/window_extensions_util.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_tab_groups_cef_delegate.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_utils.h"

namespace extensions {
namespace {
std::optional<NWebExtensionTabGroupsColor> GetTabGroupColor(api::tab_groups::Color color) {
  std::optional<NWebExtensionTabGroupsColor> tab_groups_color = std::nullopt;
  switch (color) {
    case api::tab_groups::Color::kGrey:
      tab_groups_color = NWebExtensionTabGroupsColor::GREY;
    break;
    case api::tab_groups::Color::kBlue:
      tab_groups_color = NWebExtensionTabGroupsColor::BLUE;
    break;
    case api::tab_groups::Color::kRed:
      tab_groups_color = NWebExtensionTabGroupsColor::RED;
    break;
    case api::tab_groups::Color::kYellow:
      tab_groups_color = NWebExtensionTabGroupsColor::YELLOW;
    break;
    case api::tab_groups::Color::kGreen:
      tab_groups_color = NWebExtensionTabGroupsColor::GREEN;
    break;
    case api::tab_groups::Color::kPink:
      tab_groups_color = NWebExtensionTabGroupsColor::PINK;
    break;
    case api::tab_groups::Color::kPurple:
      tab_groups_color = NWebExtensionTabGroupsColor::PURPLE;
    break;
    case api::tab_groups::Color::kCyan:
      tab_groups_color = NWebExtensionTabGroupsColor::CYAN;
    break;
    case api::tab_groups::Color::kOrange:
      tab_groups_color = NWebExtensionTabGroupsColor::ORANGE;
    break;
    default:
    break;
  }
  return tab_groups_color;
}

}

ExtensionFunction::ResponseAction TabGroupsGetFunction::Run() {
  LOG(DEBUG) << "TabGroupsGetFunction Run";
  std::optional<api::tab_groups::Get::Params> params =
      api::tab_groups::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int group_id = params->group_id;
  NWebExtensionFunctionContext default_context;
  NWebExtensionFunctionContext function_context = OHOS::NWeb::GetExtensionFunctionContext(
      extension_id(), browser_context(), include_incognito_information()).value_or(default_context);

  call_get_tab_group_ = true;
  bool success = OHOS::NWeb::NWebExtensionTabGroupsCefDelegate::GetTabGroup(
      base::BindRepeating(&TabGroupsGetFunction::OnGetTabGroup, weak_ptr_factory_get_.GetWeakPtr()),
      group_id, function_context);
  call_get_tab_group_ = true;

  if (did_respond()) {
    LOG(INFO) << "TabGroupsGetFunction did_respond";
    return AlreadyResponded();
  }
  if (success) {
    AddRef();
    LOG(INFO) << "TabGroupsGetFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support tabGroups.get"));
  }

}

void TabGroupsGetFunction::OnGetTabGroup(
    const base::WeakPtr<TabGroupsGetFunction>& function,
    const NWebExtensionTabGroup& tabGroup,
    std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnGetTabGroup is empty!!!!";
    return;
  }

  if (error) {
    function->Respond(function->Error(error.value()));
  } else {
    function->Respond(function->has_callback()
        ? function->WithArguments(base::Value(GetTabGroupValue(tabGroup)))
        : function->NoArguments());
  }

  if (!function->call_get_tab_group_) {
    LOG(INFO) << "TabGroupsGetFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction TabGroupsMoveFunction::Run() {
  LOG(DEBUG) << "TabGroupsMoveFunction Run";
  std::optional<api::tab_groups::Move::Params> params =
      api::tab_groups::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NWebExtensionTabGroupsMoveProperties move_properties;
  int group_id = params->group_id;
  move_properties.index = params->move_properties.index;
  if (params->move_properties.window_id) {
    move_properties.window_id = params->move_properties.window_id;
  }
  move_properties.contextType = OHOS::NWeb::GetExtensionContextType(browser_context());
  move_properties.includeIncognitoInfo = include_incognito_information();

  call_move_tab_group_ = true;
  bool success = OHOS::NWeb::NWebExtensionTabGroupsCefDelegate::MoveTabGroup(
      base::BindRepeating(&TabGroupsMoveFunction::OnMoveTabGroup, weak_ptr_factory_move_.GetWeakPtr()),
      group_id,
      move_properties);
  call_move_tab_group_ = false;

  if (did_respond()) {
    LOG(INFO) << "TabGroupsMoveFunction did_respond";
    return AlreadyResponded();
  }
  if (success) {
    AddRef();
    LOG(INFO) << "TabGroupsMoveFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support tabGroups.move"));
  }

}

void TabGroupsMoveFunction::OnMoveTabGroup(
    const base::WeakPtr<TabGroupsMoveFunction>& function,
    const NWebExtensionTabGroup& tabGroup,
    std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnMoveTabGroup is empty!!!!";
    return;
  }

  if (error) {
    function->Respond(function->Error(error.value()));
  } else {
    function->Respond(function->has_callback()
        ? function->WithArguments(base::Value(GetTabGroupValue(tabGroup)))
        : function->NoArguments());
  }

  if (!function->call_move_tab_group_) {
    LOG(INFO) << "TabGroupsMoveFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction TabGroupsQueryFunction::Run() {
  LOG(DEBUG) << "TabGroupsQueryFunction Run";
  std::optional<api::tab_groups::Query::Params> params =
      api::tab_groups::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NWebExtensionTabGroupsQueryInfo query_info;
  if (params->query_info.collapsed) {
    query_info.collapsed = params->query_info.collapsed;
  }
  if (params->query_info.color != api::tab_groups::Color::kNone) {
    query_info.color = GetTabGroupColor(params->query_info.color);
    LOG(INFO) << "zd_test: query_info.color = " << static_cast<int>(query_info.color.value());
  }
  if (params->query_info.title) {
    query_info.title = params->query_info.title;
  }
  if (params->query_info.window_id) {
    query_info.window_id = params->query_info.window_id;
    if (query_info.window_id == extension_misc::kCurrentWindowId) {
      query_info.window_id = GetCurrentWindowId(GetSenderWebContents(), extension_misc::kCurrentWindowId);
    }
  }
  query_info.contextType = OHOS::NWeb::GetExtensionContextType(browser_context());
  query_info.includeIncognitoInfo = include_incognito_information();

  call_query_tab_group_ = true;
  bool success = OHOS::NWeb::NWebExtensionTabGroupsCefDelegate::QueryTabGroup(
      base::BindRepeating(&TabGroupsQueryFunction::OnQueryTabGroup, weak_ptr_factory_query_.GetWeakPtr()),
      query_info);
  call_query_tab_group_ = false;

  if (did_respond()) {
    LOG(INFO) << "TabGroupsQueryFunction did_respond";
    return AlreadyResponded();
  }
  if (success) {
    AddRef();
    LOG(INFO) << "TabGroupsQueryFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support tabGroups.query"));
  }

}

void TabGroupsQueryFunction::OnQueryTabGroup(
    const base::WeakPtr<TabGroupsQueryFunction>& function,
    const std::vector<NWebExtensionTabGroup>& tabGroups,
    std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnQueryTabGroup is empty!!!!";
    return;
  }

  if (error) {
    function->Respond(function->Error(error.value()));
  } else {
    function->Respond(function->has_callback()
        ? function->WithArguments(base::Value(GetTabGroupValueList(tabGroups)))
        : function->NoArguments());
  }

  if (!function->call_query_tab_group_) {
    LOG(INFO) << "TabGroupsQueryFunction Release";
    function->Release();
  }
}

ExtensionFunction::ResponseAction TabGroupsUpdateFunction::Run() {
  LOG(DEBUG) << "TabGroupsUpdateFunction Run";
  std::optional<api::tab_groups::Update::Params> params =
      api::tab_groups::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NWebExtensionTabGroupsUpdateProperties update_properties;
  int group_id = params->group_id;
  if (params->update_properties.collapsed) {
    update_properties.collapsed = params->update_properties.collapsed;
  }
  if (params->update_properties.color != api::tab_groups::Color::kNone) {
    update_properties.color = GetTabGroupColor(params->update_properties.color);
  }
  if (params->update_properties.title) {
    update_properties.title = params->update_properties.title;
  }
  update_properties.contextType = OHOS::NWeb::GetExtensionContextType(browser_context());
  update_properties.includeIncognitoInfo = include_incognito_information();

  call_update_tab_group_ = true;
  bool success = OHOS::NWeb::NWebExtensionTabGroupsCefDelegate::UpdateTabGroup(
      base::BindRepeating(&TabGroupsUpdateFunction::OnUpdateTabGroup, weak_ptr_factory_update_.GetWeakPtr()),
      group_id,
      update_properties);
  call_update_tab_group_ = false;

  if (did_respond()) {
    LOG(INFO) << "TabGroupsUpdateFunction did_respond";
    return AlreadyResponded();
  }
  if (success) {
    AddRef();
    LOG(INFO) << "TabGroupsUpdateFunction AddRef";
    return RespondLater();
  } else {
    return RespondNow(Error("not support tabGroups.update"));
  }

}
 
void TabGroupsUpdateFunction::OnUpdateTabGroup(
    const base::WeakPtr<TabGroupsUpdateFunction>& function,
    const NWebExtensionTabGroup& tabGroup,
    std::optional<std::string>& error) {
  DCHECK(function);
  if (!function) {
    LOG(ERROR) << "OnUpdateTabGroup is empty!!!!";
    return;
  }

  if (error) {
    function->Respond(function->Error(error.value()));
  } else {
    function->Respond(function->has_callback()
        ? function->WithArguments(base::Value(GetTabGroupValue(tabGroup)))
        : function->NoArguments());
  }

  if (!function->call_update_tab_group_) {
    LOG(INFO) << "TabGroupsUpdateFunction Release";
    function->Release();
  }
}
}  // namespace extensions