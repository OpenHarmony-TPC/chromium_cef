// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/browser_context_keyed_service_factories.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/permissions/features.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "extensions/browser/api/web_request/web_request_api.h"
#endif
#include "extensions/browser/renderer_startup_helper.h"
#include "services/network/public/cpp/features.h"
#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "libcef/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "extensions/browser/api/management/management_api.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "libcef/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "libcef/browser/extensions/api/cookies/cookies_api.h"
#include "extensions/browser/api/system_info/system_info_api.h"
#endif
#ifdef OHOS_NOTIFICATION
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#endif // OHOS_NOTIFICATION

namespace cef {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  CookieSettingsFactory::GetInstance();
  media_router::ChromeMediaRouterFactory::GetInstance();
  PluginPrefsFactory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
  RendererUpdaterFactory::GetInstance();
  SpellcheckServiceFactory::GetInstance();
  ThemeServiceFactory::GetInstance();

  if (extensions::ExtensionsEnabled()) {
    extensions::AlarmManager::GetFactoryInstance();
    extensions::RendererStartupHelperFactory::GetInstance();
    extensions::StorageFrontend::GetFactoryInstance();
#if defined(OHOS_ARKWEB_EXTENSIONS)
    extensions::DeveloperPrivateAPI::GetFactoryInstance();
    extensions::ManagementAPI::GetFactoryInstance();
    extensions::WebRequestAPI::GetFactoryInstance();
    extensions::CommandService::GetFactoryInstance();
    extensions::RuntimeAPI::GetFactoryInstance();
    extensions::cef::TabsWindowsAPI::GetFactoryInstance();
    extensions::cef::WebNavigationAPI::GetFactoryInstance();
    extensions::declarative_net_request::RulesMonitorService::GetFactoryInstance();
    extensions::MenuManagerFactory::GetInstance();
    extensions::cef::CookiesAPI::GetFactoryInstance();
    extensions::SystemInfoAPI::GetFactoryInstance();
#endif
#ifdef OHOS_NOTIFICATION
    extensions::ExtensionNotificationDisplayHelperFactory::GetInstance();
#endif // OHOS_NOTIFICATION
  }

  if (base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage)) {
    ReduceAcceptLanguageFactory::GetInstance();
  }
}

}  // namespace cef
