// Copyright (c) 2023 Huawei Device Co., Ltd.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_off_the_record_browser_context.h"

#include <map>
#include <utility>

#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/ssl_host_state_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/user_prefs/user_prefs.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

#if BUILDFLAG(IS_OHOS)
#include "libcef/browser/permission/alloy_permission_manager.h"
#endif

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_builder.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#endif

#if defined(OHOS_INCOGNITO_MODE)
#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"
#endif

#ifdef OHOS_NOTIFICATION
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#endif // OHOS_NOTIFICATION

using content::BrowserThread;

AlloyOffTheRecordBrowserContext::AlloyOffTheRecordBrowserContext(
    CefBrowserContext* origin_browser_context,
    const CefRequestContextSettings& settings)
    : ChromeOffTheRecordProfileAlloy(origin_browser_context->AsProfile()),
      CefBrowserContext(settings) {}

AlloyOffTheRecordBrowserContext::~AlloyOffTheRecordBrowserContext() {
  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }
}

bool AlloyOffTheRecordBrowserContext::IsInitialized() const {
  CEF_REQUIRE_UIT();
  return !!key_;
}

void AlloyOffTheRecordBrowserContext::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  CEF_REQUIRE_UIT();
  // Initialization is always synchronous.
  std::move(callback).Run();
}

void AlloyOffTheRecordBrowserContext::Initialize() {
  CefBrowserContext::Initialize();
  LOG(INFO) << "OffTheRecordBrowserContext Initialize cache_path_ is empty:"
            << cache_path_.empty();

  Profile* original_profile = AsProfile()->GetOriginalProfile();
  DCHECK(original_profile);
  key_ = std::make_unique<ProfileKey>(cache_path_,
      original_profile->GetProfileKey());
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  // Initialize the PrefService object.
  pref_service_ = browser_prefs::CreatePrefService(this, cache_path_, false);

  // This must be called before creating any services to avoid hitting
  // DependencyManager::AssertContextWasntDestroyed when creating/destroying
  // multiple browser contexts (due to pointer address reuse).
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  const bool extensions_enabled = false;
  if (extensions_enabled) {
    // Create the custom ExtensionSystem first because other KeyedServices
    // depend on it.
#if defined(OHOS_ARKWEB_EXTENSIONS)
    policy::ChromeBrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    schema_registry_service_ = BuildSchemaRegistryServiceForProfile(
        this, connector->GetChromeSchema(), connector->GetSchemaRegistry());

    // If we are creating the profile synchronously, then we should load the
    // policy data immediately.
    bool force_immediate_policy_load = false;  //! async_prefs;

    policy::UserCloudPolicyManager* user_cloud_policy_manager;
    policy::ConfigurationPolicyProvider* policy_provider;
    {
      user_cloud_policy_manager_ = policy::UserCloudPolicyManager::Create(
          GetPath(), GetPolicySchemaRegistryService()->registry(),
          force_immediate_policy_load, content::GetIOThreadTaskRunner({}),
          base::BindRepeating(&content::GetNetworkConnectionTracker));
      user_cloud_policy_manager = user_cloud_policy_manager_.get();
      policy_provider = user_cloud_policy_manager;
    }

    profile_policy_connector_ =
        policy::CreateProfilePolicyConnectorForBrowserContext(
            schema_registry_service_->registry(), user_cloud_policy_manager,
            policy_provider, g_browser_process->browser_policy_connector(),
            force_immediate_policy_load, this);
#endif
    extension_system_ = static_cast<extensions::CefExtensionSystem*>(
        extensions::ExtensionSystem::Get(this));

    extension_system_->InitForRegularProfile(true);
    // Make sure the ProcessManager is created so that it receives extension
    // load notifications. This is necessary for the proper initialization of
    // background/event pages.
    extensions::ProcessManager::Get(this);
  }

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
      GetPrefs(), content::GetIOThreadTaskRunner({})));

  if (extensions_enabled) {
    extension_system_->Init();
  }

  PrefService* pref_service = GetPrefs();
  DCHECK(pref_service);
  user_prefs::UserPrefs::Set(this, pref_service);
  key_->SetPrefs(pref_service);

  media_device_id_salt_ = new MediaDeviceIDSalt(pref_service);

  TrackZoomLevelsFromParent();
}

void AlloyOffTheRecordBrowserContext::Shutdown() {
  CefBrowserContext::Shutdown();

  // Send notifications to clean up objects associated with this Profile.
  MaybeSendDestroyedNotification();

  // Remove any BrowserContextKeyedServiceFactory associations. This must be
  // called before the ProxyService owned by AlloyOffTheRecordBrowserContext is destroyed.
  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  key_.reset();
  SimpleKeyMap::GetInstance()->Dissociate(this);

  // Shuts down the storage partitions associated with this browser context.
  // This must be called before the browser context is actually destroyed
  // and before a clean-up task for its corresponding IO thread residents
  // (e.g. ResourceContext) is posted, so that the classes that hung on
  // StoragePartition can have time to do necessary cleanups on IO thread.
  ShutdownStoragePartitions();

  // The FontFamilyCache references the ProxyService so delete it before the
  // ProxyService is deleted.
  SetUserData(&kFontFamilyCacheKey, nullptr);

  pref_proxy_config_tracker_->DetachFromPrefService();

  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_) {
    download_manager_delegate_.reset(nullptr);
  }
}

void AlloyOffTheRecordBrowserContext::RemoveCefRequestContext(
    CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();

  if (extensions::ExtensionsEnabled()) {
    extension_system()->OnRequestContextDeleted(context);
  }

  // May result in |this| being deleted.
  CefBrowserContext::RemoveCefRequestContext(context);
}

void AlloyOffTheRecordBrowserContext::LoadExtension(
    const CefString& root_directory,
    CefRefPtr<CefDictionaryValue> manifest,
    CefRefPtr<CefExtensionHandler> handler,
    CefRefPtr<CefRequestContext> loader_context) {}

bool AlloyOffTheRecordBrowserContext::GetExtensions(std::vector<CefString>& extension_ids) {
  return false;
}

CefRefPtr<CefExtension> AlloyOffTheRecordBrowserContext::GetExtension(
    const CefString& extension_id) {
  return nullptr;
}

bool AlloyOffTheRecordBrowserContext::UnloadExtension(const CefString& extension_id) {
  DCHECK(extensions::ExtensionsEnabled());
  return true;
}

bool AlloyOffTheRecordBrowserContext::IsPrintPreviewSupported() const {
  CEF_REQUIRE_UIT();
  if (!extensions::PrintPreviewEnabled()) {
    return false;
  }

  return !GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled);
}

content::ResourceContext* AlloyOffTheRecordBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_ = std::make_unique<content::ResourceContext>();
  }
  return resource_context_.get();
}

content::ClientHintsControllerDelegate*
AlloyOffTheRecordBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

ChromeZoomLevelPrefs* AlloyOffTheRecordBrowserContext::GetZoomLevelPrefs() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
AlloyOffTheRecordBrowserContext::GetURLLoaderFactory() {
  return GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
}

base::FilePath AlloyOffTheRecordBrowserContext::GetPath() {
  return base::FilePath();
}

base::FilePath AlloyOffTheRecordBrowserContext::GetPath() const {
  return base::FilePath();
}

std::unique_ptr<content::ZoomLevelDelegate>
AlloyOffTheRecordBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelOTRDelegate>(
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}

content::DownloadManagerDelegate*
AlloyOffTheRecordBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    download_manager_delegate_.reset(
        new CefDownloadManagerDelegate(GetDownloadManager()));
  }
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* AlloyOffTheRecordBrowserContext::GetGuestManager() {
  if (!extensions::ExtensionsEnabled()) {
    return nullptr;
  }
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* AlloyOffTheRecordBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
AlloyOffTheRecordBrowserContext::GetPlatformNotificationService() {
#ifdef OHOS_NOTIFICATION
  return PlatformNotificationServiceFactory::GetForProfile(this);
#else
  return nullptr;
#endif // OHOS_NOTIFICATION
}

content::PushMessagingService* AlloyOffTheRecordBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
AlloyOffTheRecordBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* AlloyOffTheRecordBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_) {
    ssl_host_state_delegate_.reset(new CefSSLHostStateDelegate());
  }
  return ssl_host_state_delegate_.get();
}

content::PermissionControllerDelegate*
AlloyOffTheRecordBrowserContext::GetPermissionControllerDelegate() {
#if BUILDFLAG(IS_OHOS)
  if (!permission_manager_.get()) {
    permission_manager_.reset(new AlloyPermissionManager());
  }
  return permission_manager_.get();
#else
  return PermissionManagerFactory::GetForProfile(this);
#endif
}

content::BackgroundFetchDelegate*
AlloyOffTheRecordBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AlloyOffTheRecordBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AlloyOffTheRecordBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
AlloyOffTheRecordBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return ReduceAcceptLanguageFactory::GetForProfile(this);
}

std::string AlloyOffTheRecordBrowserContext::GetMediaDeviceIDSalt() {
  return media_device_id_salt_->GetSalt();
}

PrefService* AlloyOffTheRecordBrowserContext::GetPrefs() {
  return pref_service_.get();
}

const PrefService* AlloyOffTheRecordBrowserContext::GetPrefs() const {
  return pref_service_.get();
}

ProfileKey* AlloyOffTheRecordBrowserContext::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

policy::SchemaRegistryService*
AlloyOffTheRecordBrowserContext::GetPolicySchemaRegistryService() {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  return schema_registry_service_.get();
#else
  DCHECK(false);
  return nullptr;
#endif
}

policy::UserCloudPolicyManager*
AlloyOffTheRecordBrowserContext::GetUserCloudPolicyManager() {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  return user_cloud_policy_manager_.get();
#else
  DCHECK(false);
  return nullptr;
#endif
}

policy::ProfileCloudPolicyManager*
AlloyOffTheRecordBrowserContext::GetProfileCloudPolicyManager() {
  DCHECK(false);
  return nullptr;
}

policy::ProfilePolicyConnector*
AlloyOffTheRecordBrowserContext::GetProfilePolicyConnector() {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  return profile_policy_connector_.get();
#else
  DCHECK(false);
  return nullptr;
#endif
}

const policy::ProfilePolicyConnector*
AlloyOffTheRecordBrowserContext::GetProfilePolicyConnector() const {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  return profile_policy_connector_.get();
#else
  DCHECK(false);
  return nullptr;
#endif
}

bool AlloyOffTheRecordBrowserContext::IsNewProfile() const {
  DCHECK(false);
  return false;
}

void AlloyOffTheRecordBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Called when visited links will not or cannot be loaded from disk.
  enumerator->OnComplete(true);
}

DownloadPrefs* AlloyOffTheRecordBrowserContext::GetDownloadPrefs() {
  CEF_REQUIRE_UIT();
  if (!download_prefs_) {
    download_prefs_.reset(new DownloadPrefs(this));
  }
  return download_prefs_.get();
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
ExtensionSpecialStoragePolicy*
AlloyOffTheRecordBrowserContext::GetExtensionSpecialStoragePolicy() {
  return NULL;
}
#endif
