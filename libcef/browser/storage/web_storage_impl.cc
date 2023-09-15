// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/storage/web_storage_impl.h"

#include <sstream>
#include "libcef/common/time_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OHOS)
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "libcef/browser/password/oh_password_store_factory.h"
#endif

#if BUILDFLAG(IS_OHOS)
static const int kMaxDataCount = 10000;
#endif
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
  LOG(INFO) << "OnStorageKeysObtained storage_keys size: " << storage_keys.size();
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

CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  DCHECK(!getter.is_null());
  // Will return nullptr if the BrowserContext has been destroyed.
  return getter.Run();
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
          content::StoragePartition::REMOVE_DATA_MASK_APPCACHE_DEPRECATED |
              content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
              content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
              content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
              content::StoragePartition::REMOVE_DATA_MASK_WEBSQL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
          GURL(), base::Time(), base::Time::Max(), base::DoNothing());
}

void CefWebStorageImpl::DeleteOrigin(const CefString& origin) {
  GURL gurl = GURL(origin.ToString16());
  if (!gurl.is_empty() && !gurl.is_valid())
    return;
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
          content::StoragePartition::REMOVE_DATA_MASK_APPCACHE_DEPRECATED |
              content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
              content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
              content::StoragePartition::REMOVE_DATA_MASK_WEBSQL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
          origin, base::DoNothing());
};

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
      base::BindOnce(
          &storage::QuotaManager::GetUsageAndQuota,
          GetQuotaManager(browser_context),
          blink::StorageKey(url::Origin::Create(GURL(origin.ToString16()))),
          blink::mojom::StorageType::kTemporary,
          base::BindOnce(&CefWebStorageImpl::OnUsageAndQuotaObtained, this,
                         std::move(ui_callback))));
}

void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get())
    return;
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
    for (auto& callback : init_callbacks_) {
      std::move(callback).Run();
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

#if BUILDFLAG(IS_OHOS)
void CefWebStorageImpl::GetPassword(
    const CefString& url,
    const CefString& username,
    CefRefPtr<CefGetPasswordCallback> callback) {
#if defined(OHOS_NWEB_EX)
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::GetPasswordInternal), this, url,
        username, callback));
    return;
  }
  GetPasswordInternal(url, username, callback);
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::GetSavedPasswordsInfo(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
#if defined(OHOS_NWEB_EX)
  CEF_POST_TASK(
      CEF_IOT, base::BindOnce(&CefWebStorageImpl::GetSavedPasswordsInfoInternal,
                              weak_factory_.GetWeakPtr(), callback));
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::GetSavedPasswordsInfoInternal(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
  oh_password_consumer_.RequestAutofillableLogins(callback);
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
  std::vector<CefString> url;
  std::vector<CefString> username;
  int count = 0;
  for (auto& form : results) {
    if (form->password_value.empty())
      continue;

    url.push_back(CefString(form->url.spec()));
    const std::string& name = base::UTF16ToUTF8(form->username_value);
    username.push_back(CefString(name));
    count++;
    if (count == kMaxDataCount)
      break;
  }

  if (!web_storage_impl_->callback_queue_.empty()) {
    CefRefPtr<CefGetSavedPasswordsCallback> callback =
        web_storage_impl_->callback_queue_.front();
    web_storage_impl_->callback_queue_.pop();
    web_storage_impl_->OnGetAutofillableLogins(callback, url, username, count);
  }
}

void CefWebStorageImpl::OhPasswordStoreConsumer::RequestAutofillableLogins(
    CefRefPtr<CefGetSavedPasswordsCallback> callback) {
  password_manager::PasswordStore* password_store =
      web_storage_impl_->GetPasswordStore();
  if (!password_store)
    return;
  web_storage_impl_->callback_queue_.push(callback);
  password_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void CefWebStorageImpl::OnGetAutofillableLogins(
    CefRefPtr<CefGetSavedPasswordsCallback> callback,
    const std::vector<CefString>& url,
    const std::vector<CefString>& username,
    int iter_number) {
  GetSavedPasswordsCallbackImpl(callback, url, username);
}

void CefWebStorageImpl::GetPasswordInternal(
    const CefString& url,
    const CefString& username,
    CefRefPtr<CefGetPasswordCallback> callback) {
  DCHECK(CEF_CURRENTLY_ON_UIT());
  password_manager::PasswordStore* password_store = GetPasswordStore();

  if (!password_store) {
    LOG(INFO) << "password_store is null ";
    return;
  }
  std::vector<std::unique_ptr<password_manager::PasswordForm>> passwordForms =
      GetPasswordFormsByUrl(password_store, url.ToString16());
  for (auto& form : passwordForms) {
    if (form->username_value == username.ToString16()) {
      CEF_POST_TASK(CEF_IOT,
                    base::BindOnce(&CefWebStorageImpl::GetPasswordCallbackImpl,
                                   weak_factory_.GetWeakPtr(), callback,
                                   form->password_value));
      return;
    }
  }
  CEF_POST_TASK(
      CEF_IOT,
      base::BindOnce(&CefWebStorageImpl::GetPasswordCallbackImpl,
                     weak_factory_.GetWeakPtr(), callback, std::u16string()));
  LOG(WARNING) << "get Password fail";
}

void CefWebStorageImpl::GetPasswordCallbackImpl(
    CefRefPtr<CefGetPasswordCallback> callback,
    const std::u16string& password) {
  if (!callback.get()) {
    return;
  }
  callback.get()->OnComplete(CefString(password));
}

void CefWebStorageImpl::GetSavedPasswordsCallbackImpl(
    CefRefPtr<CefGetSavedPasswordsCallback> callback,
    const std::vector<CefString>& url,
    const std::vector<CefString>& username) {
  if (!callback.get()) {
    return;
  }
  callback.get()->OnComplete(url, username);
}

password_manager::PasswordStore* CefWebStorageImpl::GetPasswordStore() {
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return nullptr;
  }

  return OhPasswordStoreFactory::GetPasswordStoreForContext(
             browser_context->AsBrowserContext(),
             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

std::vector<std::unique_ptr<password_manager::PasswordForm>>
CefWebStorageImpl::GetPasswordFormsByUrl(
    password_manager::PasswordStore* password_store,
    const std::u16string& url_string) {
  GURL origin = GURL(url_string);
  password_manager::PasswordFormDigest form = {
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(origin), origin};
  return password_store->GetMatchingLogins(form);
}

void CefWebStorageImpl::ClearPassword() {
#if defined(OHOS_NWEB_EX)
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::ClearPasswordInternal), this));
    return;
  }
  return ClearPasswordInternal();
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::ClearPasswordInternal() {
  password_manager::PasswordStore* password_store = GetPasswordStore();
  if (!password_store)
    return;

  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve);
  DCHECK(filter_builder->MatchesAllOriginsAndDomains());

  if (password_store) {
    password_store->RemoveLoginsByURLAndTime(filter_builder->BuildUrlFilter(),
                                             base::Time(), base::Time::Max(),
                                             base::DoNothing());
  }
}

void CefWebStorageImpl::RemovePassword(const CefString& url,
                                       const CefString& username) {
#if defined(OHOS_NWEB_EX)
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::RemovePasswordInternal), this,
        url, username));
    return;
  }
  return RemovePasswordInternal(url, username);
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::RemovePasswordInternal(const CefString& url_string,
                                               const CefString& username) {
  password_manager::PasswordStore* password_store = GetPasswordStore();
  if (!password_store)
    return;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> passwordForms =
      GetPasswordFormsByUrl(password_store, url_string.ToString16());

  for (auto& form : passwordForms) {
    if (form->username_value == username.ToString16()) {
      password_store->RemoveLogin(*form);
      return;
    }
  }
  LOG(INFO) << "No available forms to remove";
}

void CefWebStorageImpl::ModifyPassword(const CefString& url,
                                       const CefString& old_username,
                                       const CefString& new_username,
                                       const CefString& new_password) {
#if defined(OHOS_NWEB_EX)
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::ModifyPasswordInternal), this,
        url, old_username, new_username, new_password));
    return;
  }
  return ModifyPasswordInternal(url, old_username, new_username, new_password);
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::ModifyPasswordInternal(const CefString& url,
                                               const CefString& old_username,
                                               const CefString& new_username,
                                               const CefString& new_password) {
  password_manager::PasswordStore* password_store = GetPasswordStore();
  if (!password_store)
    return;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> passwordForms =
      GetPasswordFormsByUrl(password_store, url.ToString16());
  // if new_username has saved, not suppport modify password
  for (auto& form : passwordForms) {
    if (form->username_value == new_username.ToString16()) {
      LOG(WARNING) << "change password fail owing to new username has saved";
      return;
    }
  }

  for (auto& form : passwordForms) {
    if (form->username_value == old_username.ToString16()) {
      if (form->password_value == new_password.ToString16() &&
          old_username == new_username) {
        LOG(WARNING) << "username and password not changed";
        return;
      }
      password_manager::PasswordForm* new_password_form =
          new password_manager::PasswordForm(*form);
      new_password_form->username_value = new_username.ToString16();
      new_password_form->password_value = new_password.ToString16();
      password_store->UpdateLoginWithPrimaryKey(*new_password_form, *form);
      return;
    }
  }
  LOG(INFO) << "No available forms to modify";
}

void CefWebStorageImpl::RemovePasswordByUrl(const CefString& url) {
#if defined(OHOS_NWEB_EX)
  if (!ValidContext()) {
    StoreOrTriggerUIInitCallback(base::BindOnce(
        base::IgnoreResult(&CefWebStorageImpl::RemovePasswordByUrlInternal),
        this, url));
    return;
  }
  return RemovePasswordByUrlInternal(url);
#endif  // OHOS_NWEB_EX
}

void CefWebStorageImpl::RemovePasswordByUrlInternal(
    const CefString& url_string) {
  password_manager::PasswordStore* password_store = GetPasswordStore();
  if (!password_store)
    return;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> passwordForms =
      GetPasswordFormsByUrl(password_store, url_string.ToString16());
  if (passwordForms.size() == 0) {
    LOG(INFO) << "No available forms to remove.";
    return;
  }

  for (auto& form : passwordForms) {
    password_store->RemoveLogin(*form);
  }
}
#endif

// static
CefRefPtr<CefWebStorage> CefWebStorage::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  return context ? context->GetWebStorage(callback) : nullptr;
}
