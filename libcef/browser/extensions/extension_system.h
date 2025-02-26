// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_

#include <map>
#include <memory>

#include "include/cef_extension_handler.h"
#include "include/cef_request_context.h"

#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "extensions/browser/extension_system.h"

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "extensions/browser/extension_registry_info_manager.h"
#endif // OHOS_ARKWEB_EXTENSIONS

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;
class ProcessManager;
class RendererStartupHelper;

#if defined(OHOS_ARKWEB_EXTENSIONS)
class NavigationObserver;
class UninstallPingSender;
class InstallGate;
#endif

// Used to manage extensions.
class CefExtensionSystem : public ExtensionSystem {
 public:
  explicit CefExtensionSystem(content::BrowserContext* browser_context);

  CefExtensionSystem(const CefExtensionSystem&) = delete;
  CefExtensionSystem& operator=(const CefExtensionSystem&) = delete;

  ~CefExtensionSystem() override;

  // Initializes the extension system.
  void Init();

  // Load an extension. For internal (built-in) extensions set |internal| to
  // true and |loader_context| and |handler| to NULL. For external extensions
  // set |internal| to false and |loader_context| must be the request context
  // that loaded the extension. |handler| is optional for internal extensions
  // and, if specified, will receive extension-related callbacks.
  void LoadExtension(const base::FilePath& root_directory,
                     bool internal,
                     CefRefPtr<CefRequestContext> loader_context,
                     CefRefPtr<CefExtensionHandler> handler);
  void LoadExtension(const std::string& manifest_contents,
                     const base::FilePath& root_directory,
                     bool internal,
                     CefRefPtr<CefRequestContext> loader_context,
                     CefRefPtr<CefExtensionHandler> handler);
  void LoadExtension(base::Value::Dict manifest,
                     const base::FilePath& root_directory,
                     bool internal,
                     CefRefPtr<CefRequestContext> loader_context,
                     CefRefPtr<CefExtensionHandler> handler);

  // Unload the external extension identified by |extension_id|.
  bool UnloadExtension(const std::string& extension_id);

  // Returns true if an extension matching |extension_id| is loaded.
  bool HasExtension(const std::string& extension_id) const;

  // Returns the loaded extention matching |extension_id| or NULL if not found.
  CefRefPtr<CefExtension> GetExtension(const std::string& extension_id) const;

  using ExtensionMap = std::map<std::string, CefRefPtr<CefExtension>>;

  // Returns the map of all loaded extensions.
  ExtensionMap GetExtensions() const;

  // Called when a request context is deleted. Unregisters any external
  // extensions that were registered with this context.
  void OnRequestContextDeleted(CefRequestContext* context);

  // KeyedService implementation:
  void Shutdown() override;

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  UserScriptManager* user_script_manager() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  StateStore* dynamic_user_scripts_store() override;
  scoped_refptr<value_store::ValueStoreFactory> store_factory() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  const base::OneShotEvent& ready() const override;
  bool is_ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  void PerformActionBasedOnOmahaAttributes(
      const std::string& extension_id,
      const base::Value& attributes) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

  bool initialized() const { return initialized_; }

#if defined(OHOS_ARKWEB_EXTENSIONS)
  void SetGlobalRequestContext(CefRequestContext* context);

  ExtensionRegistryInfoManager* GetExtensionRegistryInfoManager() override;
#endif

 private:
  virtual void InitPrefs();

  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(base::Value::Dict manifest,
                           const base::FilePath& root_directory,
                           bool internal);

    // The parsed contents of the extensions's manifest file.
    base::Value::Dict manifest;

    // Directory where the extension is stored.
    base::FilePath root_directory;

    // True if the extension is an internal (built-in) component.
    bool internal;
  };

  scoped_refptr<const Extension> CreateExtension(
      const ComponentExtensionInfo& info,
      std::string* utf8_error);

  // Loads a registered component extension.
  const Extension* LoadExtension(const ComponentExtensionInfo& info,
                                 CefRefPtr<CefRequestContext> loader_context,
                                 CefRefPtr<CefExtensionHandler> handler);

  // Unload the specified extension.
  void UnloadExtension(const std::string& extension_id,
                       extensions::UnloadedExtensionReason reason);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(const Extension* extension,
                               UnloadedExtensionReason reason);
#if defined(OHOS_ARKWEB_EXTENSIONS)
  void InitInstallGates();
  void MaybeSpinUpLazyContext(const Extension* extension, bool is_newly_added);
  void NotifyExtensionLoadedFromInternal(
      const Extension* extension) override;
  void NotifyExtensionUnLoadedFromInternal(
      const Extension* extension) override;
#endif

  content::BrowserContext* browser_context_;  // Not owned.

  bool initialized_;

  std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<AppSorting> app_sorting_;

  std::unique_ptr<StateStore> state_store_;
  std::unique_ptr<StateStore> rules_store_;
  scoped_refptr<value_store::ValueStoreFactory> store_factory_;

  // Signaled when the extension system has completed its startup tasks.
  base::OneShotEvent ready_;

  // Sets of enabled/disabled/terminated/blacklisted extensions. Not owned.
  ExtensionRegistry* registry_;

  // The associated RendererStartupHelper. Guaranteed to outlive the
  // ExtensionSystem, and thus us.
  extensions::RendererStartupHelper* renderer_helper_;

  // Map of extension ID to CEF extension object.
  ExtensionMap extension_map_;
#if defined(OHOS_ARKWEB_EXTENSIONS)
  std::unique_ptr<ExtensionService> extension_service_;
  std::unique_ptr<ManagementPolicy> management_policy_;

  std::unique_ptr<StateStore> dynamic_user_scripts_store_;
  std::unique_ptr<NavigationObserver> navigation_observer_;
  // Shared memory region manager for scripts statically declared in extension
  // manifests. This region is shared between all extensions.
  std::unique_ptr<UserScriptManager> user_script_manager_;
  std::unique_ptr<InstallGate> update_install_gate_;
  // For verifying the contents of extensions read from disk.
  scoped_refptr<ContentVerifier> content_verifier_;
  std::unique_ptr<UninstallPingSender> uninstall_ping_sender_;

  CefRefPtr<CefRequestContext> global_request_context_;

  std::unique_ptr<ExtensionRegistryInfoManager> extension_registry_info_manager_;
#endif

  // Must be the last member.
  base::WeakPtrFactory<CefExtensionSystem> weak_ptr_factory_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
