// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#define CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#pragma once

#include "libcef/browser/menu_model_impl.h"

#include "libcef/browser/menu_runner.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_observer.h"

#ifdef OHOS_ARKWEB_EXTENSIONS
#include "chrome/browser/extensions/menu_manager.h"
#include "ohos_nweb/src/capi/nweb_context_menus_on_clicked_data.h"
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
#include "include/cef_extension_context_menus_handler.h"
#endif // OHOS_ARKWEB_EXTENSIONS

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

class AlloyBrowserHostImpl;
class CefRunContextMenuCallback;

class CefMenuManager : public CefMenuModelImpl::Delegate,
                       public content::WebContentsObserver {
 public:
#ifdef OHOS_ARKWEB_EXTENSIONS
  static void OnClickedExtensionContextMenus(const std::string& extension_id,
                                             ContextMenusOnClickedData& data,
                                             std::optional<NWebExtensionTab>& tab);
  static std::vector<NWebContextMenusItem> GetAllExtensionContextMenus(const std::vector<std::string>& extension_ids);
  static void SetContextMenusHandler(CefExtensionContextMenusHandler* handler);
  static void OnContextMenusCreate(const std::string& extension_id, extensions::MenuItem* menu_item);
  static void OnContextMenusUpdate(const std::string& extension_id, extensions::MenuItem* menu_item);
  static void OnContextMenusRemove(const std::string& extension_id, const std::string& menu_item_id);
  static void OnContextMenusRemoveAll(const std::string& extension_id);
#endif // OHOS_ARKWEB_EXTENSIONS

  CefMenuManager(AlloyBrowserHostImpl* browser,
                 std::unique_ptr<CefMenuRunner> runner);

  CefMenuManager(const CefMenuManager&) = delete;
  CefMenuManager& operator=(const CefMenuManager&) = delete;

  ~CefMenuManager() override;

  // Delete the runner to free any platform constructs.
  void Destroy();

  // Returns true if the context menu is currently showing.
  bool IsShowingContextMenu();

  // Create the context menu.
  bool CreateContextMenu(const content::ContextMenuParams& params);
  void CancelContextMenu();

 private:
#ifdef OHOS_ARKWEB_EXTENSIONS
  static void GetFlattenedMenuItemSubtree(std::vector<NWebContextMenusItem>& items,
                                           const std::unique_ptr<extensions::MenuItem>& item);
#endif // OHOS_ARKWEB_EXTENSIONS

  // CefMenuModelImpl::Delegate methods.
  void ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                      int command_id,
                      cef_event_flags_t event_flags) override;
  void MenuWillShow(CefRefPtr<CefMenuModelImpl> source) override;
  void MenuClosed(CefRefPtr<CefMenuModelImpl> source) override;
  bool FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                   std::u16string& label) override;

  void ExecuteCommandCallback(int command_id, cef_event_flags_t event_flags);

  // Create the default menu model.
  void CreateDefaultModel();
  // Execute the default command handling.
  void ExecuteDefaultCommand(int command_id);

  // Returns true if the specified id is a custom context menu command.
  bool IsCustomContextMenuCommand(int command_id);
#ifdef OHOS_CLIPBOARD
  bool IsCommandIdEnabled(int command_id,
                          content::ContextMenuParams& params) const;
  void UpdateMenuEditStateFlags(content::ContextMenuParams& params);
#endif  // #ifdef OHOS_CLIPBOARD
#ifdef OHOS_ARKWEB_EXTENSIONS
  static CefExtensionContextMenusHandler* context_menus_handler;
#endif // OHOS_ARKWEB_EXTENSIONS

  // AlloyBrowserHostImpl pointer is guaranteed to outlive this object.
  AlloyBrowserHostImpl* browser_;

  std::unique_ptr<CefMenuRunner> runner_;

  CefRefPtr<CefMenuModelImpl> model_;
  content::ContextMenuParams params_;

  // Not owned by this class.
  CefRunContextMenuCallback* custom_menu_callback_;

  // Must be the last member.
  base::WeakPtrFactory<CefMenuManager> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
