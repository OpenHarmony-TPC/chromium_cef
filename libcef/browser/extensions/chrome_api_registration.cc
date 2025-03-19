// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// APIs must also be registered in
// libcef/common/extensions/api/_*_features.json files and possibly
// CefExtensionsDispatcherDelegate::PopulateSourceMap. See
// libcef/common/extensions/api/README.txt for additional details.

#include "libcef/browser/extensions/chrome_api_registration.h"

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"
#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"
#include "extensions/browser/api/alarms/alarms_api.h"
#include "extensions/browser/api/storage/storage_api.h"
#include "extensions/browser/extension_function_registry.h"
#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"
#include "libcef/browser/extensions/api/scripting/scripting_api.h"
#include "libcef/browser/extensions/api/context_menus/context_menus_api.h"
#include "libcef/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "libcef/browser/extensions/api/windows/windows_api.h"
#include "libcef/browser/extensions/api/cookies/cookies_api.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#endif
#ifdef OHOS_NOTIFICATION
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#endif // OHOS_NOTIFICATION

namespace extensions {
namespace api {
namespace cef {

namespace cefimpl = extensions::cef;

#define EXTENSION_FUNCTION_NAME(classname) classname::static_function_name()

// Maintain the same order as https://developer.chrome.com/extensions/api_index
// so chrome://extensions-support looks nice.
const char* const kSupportedAPIs[] = {
    "alarms",
    EXTENSION_FUNCTION_NAME(AlarmsCreateFunction),
    EXTENSION_FUNCTION_NAME(AlarmsGetFunction),
    EXTENSION_FUNCTION_NAME(AlarmsGetAllFunction),
    EXTENSION_FUNCTION_NAME(AlarmsClearFunction),
    EXTENSION_FUNCTION_NAME(AlarmsClearAllFunction),
    "contentSettings",
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingClearFunction),
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingGetFunction),
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingSetFunction),
    EXTENSION_FUNCTION_NAME(
        ContentSettingsContentSettingGetResourceIdentifiersFunction),
    "pdfViewerPrivate",
    EXTENSION_FUNCTION_NAME(PdfViewerPrivateIsAllowedLocalFileAccessFunction),
    EXTENSION_FUNCTION_NAME(PdfViewerPrivateIsPdfOcrAlwaysActiveFunction),
    "resourcesPrivate",
    EXTENSION_FUNCTION_NAME(ResourcesPrivateGetStringsFunction),
    "storage",
    EXTENSION_FUNCTION_NAME(StorageStorageAreaGetFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaSetFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaRemoveFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaClearFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaGetBytesInUseFunction),
    "tabs",
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsCreateFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsUpdateFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsExecuteScriptFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsInsertCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsRemoveCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsSetZoomFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetZoomFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsSetZoomSettingsFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetZoomSettingsFunction),
#if defined(OHOS_ARKWEB_EXTENSIONS)
    EXTENSION_FUNCTION_NAME(cefimpl::TabsQueryFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsReloadFunction),
    "webNavigation",
    EXTENSION_FUNCTION_NAME(cefimpl::WebNavigationGetFrameFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::WebNavigationGetAllFramesFunction),
    // chrome.extension.getURL()'s implementation is in the renderer.
    "extension",
    "extension.getURL",
    "developerPrivate",
    EXTENSION_FUNCTION_NAME(DeveloperPrivateGetExtensionsInfoFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateGetExtensionSizeFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateGetItemsInfoFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateGetProfileConfigurationFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateUpdateProfileConfigurationFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateShowPermissionsDialogFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateReloadFunction),
    EXTENSION_FUNCTION_NAME(
        DeveloperPrivateUpdateExtensionConfigurationFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateLoadUnpackedFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateInstallDroppedFileFunction),
    EXTENSION_FUNCTION_NAME(
        DeveloperPrivateNotifyDragInstallInProgressFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateLoadDirectoryFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateChoosePathFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivatePackDirectoryFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateIsProfileManagedFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateRequestFileSourceFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateOpenDevToolsFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateOpenUrlFunction),
    EXTENSION_FUNCTION_NAME(DeveloperPrivateDeleteExtensionErrorsFunction),
    "scripting",
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingExecuteScriptFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingInsertCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingRemoveCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingRegisterContentScriptsFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingGetRegisteredContentScriptsFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingUnregisterContentScriptsFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::ScriptingUpdateContentScriptsFunction),
    "contextMenus",
    EXTENSION_FUNCTION_NAME(cefimpl::ContextMenusCreateFunction),
    "sidePanel",
    EXTENSION_FUNCTION_NAME(SidePanelOpenFunction),
    EXTENSION_FUNCTION_NAME(SidePanelSetOptionsFunction),
    "action",
    EXTENSION_FUNCTION_NAME(ActionSetIconFunction),
    "windows",
    EXTENSION_FUNCTION_NAME(cefimpl::WindowsGetAllFunction),
    "cookies",
    EXTENSION_FUNCTION_NAME(cefimpl::CookiesGetFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::CookiesGetAllFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::CookiesSetFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::CookiesRemoveFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::CookiesGetAllCookieStoresFunction),
    "system",
    "system.display",
    EXTENSION_FUNCTION_NAME(SystemDisplayGetInfoFunction),
#endif
#ifdef OHOS_NOTIFICATION
  "notifications",
  EXTENSION_FUNCTION_NAME(NotificationsCreateFunction),
  EXTENSION_FUNCTION_NAME(NotificationsUpdateFunction),
  EXTENSION_FUNCTION_NAME(NotificationsClearFunction),
  EXTENSION_FUNCTION_NAME(NotificationsGetAllFunction),
  EXTENSION_FUNCTION_NAME(NotificationsGetPermissionLevelFunction),
#endif // OHOS_NOTIFICATION
    nullptr,  // Indicates end of array.
};

// Only add APIs to this list that have been tested in CEF.
// static
bool ChromeFunctionRegistry::IsSupported(const std::string& name) {
  for (size_t i = 0; kSupportedAPIs[i] != nullptr; ++i) {
    if (name == kSupportedAPIs[i]) {
      return true;
    }
  }
  return false;
}

// Only add APIs to this list that have been tested in CEF.
// static
void ChromeFunctionRegistry::RegisterAll(ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<AlarmsCreateFunction>();
  registry->RegisterFunction<AlarmsGetFunction>();
  registry->RegisterFunction<AlarmsGetAllFunction>();
  registry->RegisterFunction<AlarmsClearFunction>();
  registry->RegisterFunction<AlarmsClearAllFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingClearFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingGetFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingSetFunction>();
  registry->RegisterFunction<
      ContentSettingsContentSettingGetResourceIdentifiersFunction>();
  registry
      ->RegisterFunction<PdfViewerPrivateIsAllowedLocalFileAccessFunction>();
  registry->RegisterFunction<PdfViewerPrivateIsPdfOcrAlwaysActiveFunction>();
  registry->RegisterFunction<ResourcesPrivateGetStringsFunction>();
  registry->RegisterFunction<StorageStorageAreaGetFunction>();
  registry->RegisterFunction<StorageStorageAreaSetFunction>();
  registry->RegisterFunction<StorageStorageAreaRemoveFunction>();
  registry->RegisterFunction<StorageStorageAreaClearFunction>();
  registry->RegisterFunction<StorageStorageAreaGetBytesInUseFunction>();
  registry->RegisterFunction<cefimpl::TabsExecuteScriptFunction>();
  registry->RegisterFunction<cefimpl::TabsInsertCSSFunction>();
  registry->RegisterFunction<cefimpl::TabsRemoveCSSFunction>();
  registry->RegisterFunction<cefimpl::TabsGetFunction>();
  registry->RegisterFunction<cefimpl::TabsCreateFunction>();
  registry->RegisterFunction<cefimpl::TabsUpdateFunction>();
  registry->RegisterFunction<cefimpl::TabsSetZoomFunction>();
  registry->RegisterFunction<cefimpl::TabsGetZoomFunction>();
  registry->RegisterFunction<cefimpl::TabsSetZoomSettingsFunction>();
  registry->RegisterFunction<cefimpl::TabsGetZoomSettingsFunction>();
#if defined(OHOS_ARKWEB_EXTENSIONS)
  registry->RegisterFunction<cefimpl::TabsQueryFunction>();
  registry->RegisterFunction<cefimpl::TabsReloadFunction>();
  registry->RegisterFunction<DeveloperPrivateGetExtensionsInfoFunction>();
  registry->RegisterFunction<DeveloperPrivateGetExtensionSizeFunction>();
  registry->RegisterFunction<DeveloperPrivateGetItemsInfoFunction>();
  registry->RegisterFunction<DeveloperPrivateGetProfileConfigurationFunction>();
  registry
      ->RegisterFunction<DeveloperPrivateUpdateProfileConfigurationFunction>();
  registry->RegisterFunction<DeveloperPrivateShowPermissionsDialogFunction>();
  registry->RegisterFunction<DeveloperPrivateReloadFunction>();
  registry->RegisterFunction<DeveloperPrivateLoadUnpackedFunction>();
  registry->RegisterFunction<DeveloperPrivateInstallDroppedFileFunction>();
  registry
      ->RegisterFunction<DeveloperPrivateNotifyDragInstallInProgressFunction>();
  registry->RegisterFunction<DeveloperPrivateLoadDirectoryFunction>();
  registry->RegisterFunction<DeveloperPrivateChoosePathFunction>();
  registry->RegisterFunction<DeveloperPrivatePackDirectoryFunction>();
  registry->RegisterFunction<DeveloperPrivateIsProfileManagedFunction>();
  registry->RegisterFunction<DeveloperPrivateRequestFileSourceFunction>();
  registry->RegisterFunction<DeveloperPrivateOpenDevToolsFunction>();
  registry->RegisterFunction<DeveloperPrivateOpenUrlFunction>();
  registry->RegisterFunction<DeveloperPrivateDeleteExtensionErrorsFunction>();
  // webNavigation
  registry->RegisterFunction<cefimpl::WebNavigationGetFrameFunction>();
  registry->RegisterFunction<cefimpl::WebNavigationGetAllFramesFunction>();
  // scripting
  registry->RegisterFunction<cefimpl::ScriptingExecuteScriptFunction>();
  registry->RegisterFunction<cefimpl::ScriptingInsertCSSFunction>();
  registry->RegisterFunction<cefimpl::ScriptingRemoveCSSFunction>();
  registry->RegisterFunction<cefimpl::ScriptingRegisterContentScriptsFunction>();
  registry->RegisterFunction<cefimpl::ScriptingGetRegisteredContentScriptsFunction>();
  registry->RegisterFunction<cefimpl::ScriptingUnregisterContentScriptsFunction>();
  registry->RegisterFunction<cefimpl::ScriptingUpdateContentScriptsFunction>();
  // contextMenus
  registry->RegisterFunction<cefimpl::ContextMenusCreateFunction>();
  // sidePanel
  registry->RegisterFunction<SidePanelGetOptionsFunction>();
  registry->RegisterFunction<SidePanelSetOptionsFunction>();
  registry->RegisterFunction<SidePanelSetPanelBehaviorFunction>();
  registry->RegisterFunction<SidePanelGetPanelBehaviorFunction>();
  registry->RegisterFunction<SidePanelOpenFunction>();
  // action methods
  registry->RegisterFunction<ActionSetIconFunction>();
  // windows
  registry->RegisterFunction<cefimpl::WindowsGetAllFunction>();
  // cookies
  registry->RegisterFunction<cefimpl::CookiesGetFunction>();
  registry->RegisterFunction<cefimpl::CookiesGetAllFunction>();
  registry->RegisterFunction<cefimpl::CookiesSetFunction>();
  registry->RegisterFunction<cefimpl::CookiesRemoveFunction>();
  registry->RegisterFunction<cefimpl::CookiesGetAllCookieStoresFunction>();
  // system.display
  registry->RegisterFunction<SystemDisplayGetInfoFunction>();
#endif
#ifdef OHOS_NOTIFICATION
  registry->RegisterFunction<NotificationsCreateFunction>();
  registry->RegisterFunction<NotificationsUpdateFunction>();
  registry->RegisterFunction<NotificationsClearFunction>();
  registry->RegisterFunction<NotificationsGetAllFunction>();
  registry->RegisterFunction<NotificationsGetPermissionLevelFunction>();
#endif // OHOS_NOTIFICATION
}

}  // namespace cef
}  // namespace api
}  // namespace extensions
