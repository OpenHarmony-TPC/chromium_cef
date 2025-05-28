// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
#define CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_

#ifdef OHOS_ARKWEB_ADBLOCK
#include "components/prefs/pref_name_set.h"
#endif

#include <memory>

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "components/prefs/pref_name_set.h"
#endif

namespace base {
class FilePath;
}

class CefBrowserContext;
class CefBrowserHostBase;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace browser_prefs {

// Name for the user prefs JSON file.
extern const char kUserPrefsFileName[];
#if defined(OHOS_EX_PASSWORD)
extern const char kMigratePasswordsReady[];
extern const char kMigratePasswordsToPasswordVault[];
extern const char kMigrationCounct[];
extern const char kMigrationDataBackupCompletion[];
extern const char kMigrationQueryAssetfailure[];
#endif

// Register preferences specific to CEF.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

#if defined(OHOS_ARKWEB_EXTENSIONS)
void RegisterExtensionPersistentPrefs(PrefNameSet& pref_name_set);
#endif

// Create the PrefService used to manage pref registration and storage.
// |profile| will be nullptr for the system-level PrefService. Used with the
// Alloy runtime only.
std::unique_ptr<PrefService> CreatePrefService(Profile* profile,
                                               const base::FilePath& cache_path,
                                               bool persist_user_preferences);

// Returns the value for populating the accept-language HTTP request header.
// |browser_context| and/or |browser| may be nullptr. If |expand| is true then
// base languages and Q values may be added.
std::string GetAcceptLanguageList(CefBrowserContext* browser_context,
                                  CefBrowserHostBase* browser,
                                  bool expand);

#ifdef OHOS_ARKWEB_ADBLOCK
void RegisterSubresourceFilterPersistentPrefs(PrefNameSet& pref_name_set);
void RegisterUserSubresourceFilterPersistentPrefs(PrefNameSet& pref_name_set);
#endif

}  // namespace browser_prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
