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

class TabGroupsGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.get", TAB_GROUPS_GET)
  ~TabGroupsGetFunction() override {}
  ResponseAction Run() override;

  static void OnGetTabGroup(const base::WeakPtr<TabGroupsGetFunction>& function,
                            const NWebExtensionTabGroup& tabGroup,
                            std::optional<std::string>& error);
  bool call_get_tab_group_ = false;
  base::WeakPtrFactory<TabGroupsGetFunction> weak_ptr_factory_get_{this};
};

class TabGroupsQueryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.query", TAB_GROUPS_QUERY)
  ~TabGroupsQueryFunction() override {}
  ResponseAction Run() override;

  static void OnQueryTabGroup(const base::WeakPtr<TabGroupsQueryFunction>& function,
                              const std::vector<NWebExtensionTabGroup>& tabGroups,
                              std::optional<std::string>& error);
  bool call_query_tab_group_ = false;
  base::WeakPtrFactory<TabGroupsQueryFunction> weak_ptr_factory_query_{this};
};

class TabGroupsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.update", TAB_GROUPS_UPDATE)
  ~TabGroupsUpdateFunction() override {}
  ResponseAction Run() override;

  static void OnUpdateTabGroup(const base::WeakPtr<TabGroupsUpdateFunction>& function,
                               const NWebExtensionTabGroup& tabGroup,
                               std::optional<std::string>& error);
  bool call_update_tab_group_ = false;
  base::WeakPtrFactory<TabGroupsUpdateFunction> weak_ptr_factory_update_{this};
};

class TabGroupsMoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.move", TAB_GROUPS_MOVE)
  ~TabGroupsMoveFunction() override {}
  ResponseAction Run() override;

  static void OnMoveTabGroup(const base::WeakPtr<TabGroupsMoveFunction>& function,
                             const NWebExtensionTabGroup& tabGroup,
                             std::optional<std::string>& error);
  bool call_move_tab_group_ = false;
  base::WeakPtrFactory<TabGroupsMoveFunction> weak_ptr_factory_move_{this};
};