// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/build/features/features.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/context.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/prefs/pref_registrar.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/locale_settings.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/os_crypt/sync/os_crypt.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#endif

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
#include "components/prefs/segregated_pref_store.h"
#include "ohos_cef_ext/libcef/browser/predictors/predictor_database.h"
#endif  // BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)

#if BUILDFLAG(ARKWEB_EDM_POLICY)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif

#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "libcef/browser/ohos_safe_browsing/ohos_sb_prefs.h"
#endif

#if BUILDFLAG(ARKWEB_ADBLOCK)
#include "components/subresource_filter/core/browser/ruleset_version.h"
#include "components/subresource_filter/core/browser/user_ruleset_version.h"
#endif

#if BUILDFLAG(ARKWEB_EDM_POLICY)
#include "arkweb/chromium_ext/components/policy/core/common/policy_loader_ohos.h"
#endif

#include "ohos_nweb_ex/overrides/cef/libcef/browser/cloud_control/model/changed_config_list_cache.h"

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
#include "ohos_cef_ext/libcef/browser/predictors/predictor_database.h"
#endif

namespace arkweb_browser_prefs_ext {

const char kUserPrefsFileName[] = "UserPrefs.json";
const char kLocalPrefsFileName[] = "LocalPrefs.json";


#if defined(ARKWEB_CLOUD_CONTROL) && !BUILDFLAG(ARKWEB_NWEB_EX)
void RegisterCloudControlProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

void RegisterCloudControlPersistentPrefs(PrefNameSet& pref_name_set) {}
#endif

#if BUILDFLAG(ARKWEB_ADBLOCK)
void RegisterSubresourceFilterPersistentPrefs(PrefNameSet& pref_name_set) {
  pref_name_set.insert("subresource_filter.ruleset_version.content");
  pref_name_set.insert("subresource_filter.ruleset_version.format");
  pref_name_set.insert("subresource_filter.ruleset_version.checksum");
}

void RegisterUserSubresourceFilterPersistentPrefs(PrefNameSet& pref_name_set) {
  pref_name_set.insert("subresource_filter.user_ruleset_version.content");
  pref_name_set.insert("subresource_filter.user_ruleset_version.format");
  pref_name_set.insert("subresource_filter.user_ruleset_version.checksum");
  pref_name_set.insert("subresource_filter.user_easylist.path");
  pref_name_set.insert("subresource_filter.user_easylist.replace");
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  if (!registry) {
    LOG(INFO) << "pref registry syncable is nullptr";
    return;
  }

  ohos_safe_browsing::RegisterProfilePrefs(registry);
  cloud_control::ChangedConfigListCache::RegisterCloudControlPrefs(registry);
#if BUILDFLAG(ARKWEB_EDM_POLICY)
  policy::RegisterBrowserPolicyProfilePrefs(registry);
#endif
#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
  predictor::PredictorDatabase::RegisterPrefs(registry);
#endif  // IS_OHOS
}

}  // namespace arkweb_browser_prefs_ext
