// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/storage/web_storage_impl.h"

#include <sstream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "libcef/common/time_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
#include "base/task/bind_post_task.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
static const int kMaxDataCount = 10000;
#endif

namespace {
// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  CEF_REQUIRE_UIT();

  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}
}  // namespace
class GetStorageKeysTask
    : public base::RefCountedThreadSafe<GetStorageKeysTask> {
 public:
  GetStorageKeysTask(CefWebStorageImpl::GetOriginsCallback callback,
                     storage::QuotaManager* quota_manager);

  GetStorageKeysTask(const GetStorageKeysTask&) = delete;
  GetStorageKeysTask& operator=(const GetStorageKeysTask&) = delete;

  void Run();

 private:
  friend class base::RefCountedThreadSafe<GetStorageKeysTask>;
  ~GetStorageKeysTask();
  void OnStorageKeysObtained(blink::mojom::StorageType type,
                             const std::set<blink::StorageKey>& storage_keys);
  void OnUsageAndQuotaObtained(const blink::StorageKey& storage_key,
                               blink::mojom::QuotaStatusCode status_code,
                               int64_t usage,
                               int64_t quota);
  void CheckDone();
  void DoneOnUIThread();

  CefWebStorageImpl::GetOriginsCallback ui_callback_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  std::vector<std::string> origin_;
  std::vector<int64_t> usage_;
  std::vector<int64_t> quota_;
  size_t num_callbacks_to_wait_;
  size_t num_callbacks_received_;
};

GetStorageKeysTask::GetStorageKeysTask(
    CefWebStorageImpl::GetOriginsCallback callback,
    storage::QuotaManager* quota_manager)
    : ui_callback_(std::move(callback)), quota_manager_(quota_manager) {
  DCHECK(CEF_CURRENTLY_ON_UIT());
}

GetStorageKeysTask::~GetStorageKeysTask() {}

void GetStorageKeysTask::Run() {
  DCHECK(CEF_CURRENTLY_ON_UIT());
  CEF_POST_TASK(
      CEF_IOT,
      base::BindOnce(
          &storage::QuotaManager::GetStorageKeysForType, quota_manager_,
          blink::mojom::StorageType::kTemporary,
          base::BindOnce(&GetStorageKeysTask::OnStorageKeysObtained, this,
                         blink::mojom::StorageType::kTemporary)));
}

void GetStorageKeysTask::OnStorageKeysObtained(
    blink::mojom::StorageType type,
    const std::set<blink::StorageKey>& storage_keys) {
  DCHECK(CEF_CURRENTLY_ON_IOT());
  LOG(INFO) << "OnStorageKeysObtained storage_keys size: "
            << storage_keys.size();
  num_callbacks_to_wait_ = storage_keys.size();
  num_callbacks_received_ = 0u;
  for (const blink::StorageKey& storage_key : storage_keys) {
    quota_manager_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&GetStorageKeysTask::OnUsageAndQuotaObtained, this,
                       storage_key));
  }
  CheckDone();
}

void GetStorageKeysTask::OnUsageAndQuotaObtained(
    const blink::StorageKey& storage_key,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  DCHECK(CEF_CURRENTLY_ON_IOT());
  if (status_code == blink::mojom::QuotaStatusCode::kOk) {
    origin_.push_back(storage_key.origin().GetURL().spec());
    usage_.push_back(usage);
    quota_.push_back(quota);
  }
  ++num_callbacks_received_;
  CheckDone();
}

void GetStorageKeysTask::CheckDone() {
  DCHECK(CEF_CURRENTLY_ON_IOT());
  if (num_callbacks_received_ == num_callbacks_to_wait_) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&GetStorageKeysTask::DoneOnUIThread, this));
  } else if (num_callbacks_received_ > num_callbacks_to_wait_) {
    NOTREACHED();
  }
}

// This method is to avoid copying the 3 vector arguments into a bound callback.
void GetStorageKeysTask::DoneOnUIThread() {
  DCHECK(CEF_CURRENTLY_ON_UIT());
  if (ui_callback_) {
    std::move(ui_callback_).Run(origin_, usage_, quota_);
  }
}

content::StoragePartition* GetStoragePartitionDelegate(
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  return browser_context->AsBrowserContext()->GetDefaultStoragePartition();
}

storage::QuotaManager* GetQuotaManager(CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  return GetStoragePartitionDelegate(browser_context)->GetQuotaManager();
}

void CefWebStorageImpl::DeleteAllData() {
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::DeleteAllDataInternal), this));
    return;
  }
  return DeleteAllDataInternal();
}

void CefWebStorageImpl::DeleteAllDataInternal() {
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return;
  }
  GetStoragePartitionDelegate(browser_context)
      ->ClearData(
          // Clear all web storage data except cookies.
          content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
              content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
              content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
              content::StoragePartition::REMOVE_DATA_MASK_WEBSQL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
          blink::StorageKey(), base::Time(), base::Time::Max(),
          base::DoNothing());
}

void CefWebStorageImpl::DeleteOrigin(const CefString& origin) {
  GURL gurl = GURL(origin.ToString16());
  if (!gurl.is_empty() && !gurl.is_valid()) {
    return;
  }
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::DeleteOriginInternal), this,
        gurl));
    return;
  }
  return DeleteOriginInternal(gurl);
}

void CefWebStorageImpl::DeleteOriginInternal(const GURL& origin) {
  DCHECK(ValidContext());
  DCHECK(origin.is_empty() || origin.is_valid());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    LOG(ERROR) << "CefWebStorageImpl::DeleteOriginInternal can not get "
                  "browser_context.";
    return;
  }
  GetStoragePartitionDelegate(browser_context)
      ->ClearDataForOrigin(
          // All (temporary) QuotaClient types.
          content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
              content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
              content::StoragePartition::REMOVE_DATA_MASK_WEBSQL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
          origin, base::DoNothing());
}

void CefWebStorageImpl::GetOrigins(CefRefPtr<CefGetOriginsCallback> callback) {
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::GetOriginsInternal), this,
        callback));
    return;
  }
  return GetOriginsInternal(callback);
}

void CefWebStorageImpl::GetOriginsInternal(
    CefRefPtr<CefGetOriginsCallback> callback) {
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    LOG(ERROR)
        << "CefWebStorageImpl::GetOriginsInternal can not get browser_context.";
    return;
  }
  GetOriginsCallback ui_callback =
      base::BindOnce(&CefWebStorageImpl::GetOriginsCallbackImpl,
                     weak_factory_.GetWeakPtr(), callback);
  base::MakeRefCounted<GetStorageKeysTask>(std::move(ui_callback),
                                           GetQuotaManager(browser_context))
      ->Run();
}

void CefWebStorageImpl::GetOriginQuota(
    const CefString& origin,
    CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback) {
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::GetOriginUsageAndQuotaInternal),
        this, origin, true, callback));
    return;
  }
  return GetOriginUsageAndQuotaInternal(origin, true, callback);
}

void CefWebStorageImpl::GetOriginUsage(
    const CefString& origin,
    CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback) {
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::GetOriginUsageAndQuotaInternal),
        this, origin, false, callback));
    return;
  }
  return GetOriginUsageAndQuotaInternal(origin, false, callback);
}

void CefWebStorageImpl::OnUsageAndQuotaObtained(
    CefWebStorageImpl::QuotaUsageCallback ui_callback,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  DCHECK(CEF_CURRENTLY_ON_IOT());
  if (status_code != blink::mojom::QuotaStatusCode::kOk) {
    usage = 0;
    quota = 0;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(std::move(ui_callback), usage, quota));
}

void CefWebStorageImpl::GetOriginUsageAndQuotaInternal(
    const CefString& origin,
    bool is_quota,
    CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback) {
  DCHECK(CEF_CURRENTLY_ON_UIT());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    LOG(ERROR)
        << "CefWebStorageImpl::GetOriginsInternal can not get browser_context.";
    return;
  }
  CefWebStorageImpl::QuotaUsageCallback ui_callback =
      base::BindOnce(&CefWebStorageImpl::CefGetOriginUsageOrQuotaCallbackImpl,
                     weak_factory_.GetWeakPtr(), callback, is_quota);
  CEF_POST_TASK(
      CEF_IOT,
      base::BindOnce(&storage::QuotaManager::GetUsageAndQuota,
                     GetQuotaManager(browser_context),
                     blink::StorageKey::CreateFirstParty(
                         url::Origin::Create(GURL(origin.ToString16()))),
                     blink::mojom::StorageType::kTemporary,
                     base::BindOnce(&CefWebStorageImpl::OnUsageAndQuotaObtained,
                                    this, std::move(ui_callback))));
}

void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}

void CefWebStorageImpl::Initialize(
    CefBrowserContext::Getter browser_context_getter,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();
  DCHECK(!initialized_);
  DCHECK(!browser_context_getter.is_null());
  DCHECK(browser_context_getter_.is_null());
  browser_context_getter_ = browser_context_getter;
  initialized_ = true;
  if (!init_callbacks_.empty()) {
    for (auto& cb : init_callbacks_) {
      std::move(cb).Run();
    }
    init_callbacks_.clear();
  }
  RunAsyncCompletionOnUIThread(callback);
}

void CefWebStorageImpl::StoreOrTriggerUIInitCallback(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(
                               &CefWebStorageImpl::StoreOrTriggerUIInitCallback,
                               this, std::move(callback)));
    return;
  }
  if (initialized_) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

bool CefWebStorageImpl::ValidContext() const {
  return CEF_CURRENTLY_ON_UIT() && initialized_;
}

void CefWebStorageImpl::GetOriginsCallbackImpl(
    CefRefPtr<CefGetOriginsCallback> callback,
    const std::vector<std::string>& origin,
    const std::vector<int64_t>& usage,
    const std::vector<int64_t>& quota) {
  if (!callback.get()) {
    return;
  }
  int32_t nums = origin.size();
  std::vector<CefString> origins;
  std::vector<CefString> usages;
  std::vector<CefString> quotas;
  std::string res;
  for (int32_t i = 0; i < nums; i++) {
    origins.push_back(CefString(origin[i]));
    res = std::to_string(usage[i]);
    usages.push_back(CefString(res));
    res = std::to_string(quota[i]);
    quotas.push_back(CefString(res));
  }

  callback.get()->OnOrigins(origins);
  callback.get()->OnQuotas(quotas);
  callback.get()->OnUsages(usages);
  callback.get()->OnComplete();
}

void CefWebStorageImpl::CefGetOriginUsageOrQuotaCallbackImpl(
    CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback,
    bool is_quota,
    int64_t usage,
    int64_t quota) {
  if (!callback.get()) {
    return;
  }
  int64_t result = is_quota == true ? quota : usage;
  callback.get()->OnComplete(result);
}

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
void CefWebStorageImpl::GetSavedPasswordsInfoInternal(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
  // oh_password_consumer_.RequestAutofillableLogins(callback);
}

CefWebStorageImpl::OhPasswordStoreConsumer::OhPasswordStoreConsumer(
    CefWebStorageImpl* web_storage_impl)
    : web_storage_impl_(web_storage_impl) {}

CefWebStorageImpl::OhPasswordStoreConsumer::~OhPasswordStoreConsumer() {}

void CefWebStorageImpl::OhPasswordStoreConsumer::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void CefWebStorageImpl::OhPasswordStoreConsumer::OnGetPasswordStoreResultsFrom(
    password_manager::PasswordStoreInterface* store,
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
}

void CefWebStorageImpl::OhPasswordStoreConsumer::RequestAutofillableLogins(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
}

void CefWebStorageImpl::OnGetAutofillableLogins(
    CefRefPtr<CefGetSavedPasswordsCallback> callback,
    const std::vector<CefString>& url,
    const std::vector<CefString>& username,
    int iter_number) {
  // GetSavedPasswordsCallbackImpl(callback, url, username);
}

void CefWebStorageImpl::GetPasswordInternal(
    const CefString& url,
    const CefString& username,
    CefRefPtr<CefGetPasswordCallback> callback) {
}

void CefWebStorageImpl::GetPasswordCallbackImpl(
    CefRefPtr<CefGetPasswordCallback> callback,
    const std::u16string& password) {
}

void CefWebStorageImpl::GetSavedPasswordsCallbackImpl(
    CefRefPtr<CefGetSavedPasswordsCallback> callback,
    const std::vector<CefString>& url,
    const std::vector<CefString>& username) {
 }

password_manager::PasswordStore* CefWebStorageImpl::GetPasswordStore() {
}

std::vector<std::unique_ptr<password_manager::PasswordForm>>
CefWebStorageImpl::GetPasswordFormsByUrl(
    password_manager::PasswordStore* password_store,
    const std::u16string& url_string) {
}

void CefWebStorageImpl::ClearPasswordInternal() {
  password_manager::PasswordStore* password_store = GetPasswordStore();
}

void CefWebStorageImpl::RemovePasswordInternal(const CefString& url_string,
                                               const CefString& username) {
}

void CefWebStorageImpl::ModifyPasswordInternal(const CefString& url,
                                               const CefString& old_username,
                                               const CefString& new_username,
                                               const CefString& new_password) {
}

void CefWebStorageImpl::RemovePasswordByUrlInternal(
    const CefString& url_string) {
}
#endif  // BUILDFLAG(ARKWEB_EXT_PASSWORD)

// static
CefRefPtr<CefWebStorage> CefWebStorage::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  return context ? context->GetWebStorage(callback) : nullptr;
}

#if BUILDFLAG(ARKWEB_INCOGNITO_MODE)
// static
CefRefPtr<CefWebStorage> CefWebStorage::GetGlobalIncognitoManager(
    CefRefPtr<CefCompletionCallback> callback) {
  LOG(INFO) << "CefWebStorage::GetGlobalIncognitoManage.";
  CefRefPtr<CefRequestContext> context =
      CefRequestContext::GetGlobalOTRContext();
  return context ? context->GetWebStorage(callback) : nullptr;
}
#endif

void CefWebStorageImpl::GetPassword(
    const CefString& url,
    const CefString& username,
    CefRefPtr<CefGetPasswordCallback> callback) {
}

void CefWebStorageImpl::GetSavedPasswordsInfo(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
}

void CefWebStorageImpl::ClearPassword() {
}

void CefWebStorageImpl::RemovePassword(const CefString& url,
                                       const CefString& username) {
}

void CefWebStorageImpl::ModifyPassword(const CefString& url,
                                       const CefString& old_username,
                                       const CefString& new_username,
                                       const CefString& new_password) {
}

void CefWebStorageImpl::RemovePasswordByUrl(const CefString& url) {
}
