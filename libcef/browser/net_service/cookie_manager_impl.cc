// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_manager_impl.h"

#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/time_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net_helpers.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OHOS)
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/cookie_config/cookie_store_util.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "ohos_adapter_helper.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/cookie_manager.h"
#endif // BUILDFLAG(IS_OHOS)

using network::mojom::CookieManager;

namespace {

// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
#if !BUILDFLAG(IS_OHOS)
  CEF_REQUIRE_UIT();
#endif // !BUILDFLAG(IS_OHOS)
  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}

// Do not keep a reference to the object returned by this method.
CookieManager* GetCookieManager(CefBrowserContext* browser_context) {
#if BUILDFLAG(IS_OHOS)
  return browser_context->AsBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForOhos();
#else
  CEF_REQUIRE_UIT();
  return browser_context->AsBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
#endif // BUILDFLAG(IS_OHOS)
}

// Always execute the callback asynchronously.
void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}

#if BUILDFLAG(IS_OHOS)
void DeleteCanonicalCookie(CookieManager* cookie_manager,
                           const net::CanonicalCookie& cookie,
                           base::OnceCallback<void(bool)> callback) {
  if (!cookie_manager) {
    if (callback)
      std::move(callback).Run(false);
    return;
  }
  cookie_manager->DeleteCanonicalCookie(cookie, std::move(callback));
}
#endif // BUILDFLAG(IS_OHOS)

void ExecuteVisitor(CefRefPtr<CefCookieVisitor> visitor,
                  #if BUILDFLAG(IS_OHOS)
                    CookieManager* cookie_manager,
                  #else
                    const CefBrowserContext::Getter& browser_context_getter,
                  #endif
                    const std::vector<net::CanonicalCookie>& cookies) {
#if !BUILDFLAG(IS_OHOS)
  CEF_REQUIRE_UIT();

  auto browser_context = GetBrowserContext(browser_context_getter);
  if (!browser_context)
    return;

  auto cookie_manager = GetCookieManager(browser_context);
#endif // !BUILDFLAG(IS_OHOS)

  int total = cookies.size(), count = 0;
  if (total == 0) {
    CefCookie cookie;
    bool deleteCookie = false;
    visitor->Visit(cookie, 0, 0, deleteCookie);
    return;
  }

  for (const auto& cc : cookies) {
    CefCookie cookie;
    net_service::MakeCefCookie(cc, cookie);

    bool deleteCookie = false;
    bool keepLooping = visitor->Visit(cookie, count, total, deleteCookie);
    if (deleteCookie) {
    #if BUILDFLAG(IS_OHOS)
      DeleteCanonicalCookie(cookie_manager, cc, CookieManager::DeleteCanonicalCookieCallback());
    #else
      cookie_manager->DeleteCanonicalCookie(
          cc, CookieManager::DeleteCanonicalCookieCallback());
    #endif // BUILDFLAG(IS_OHOS)
    }
    if (!keepLooping)
      break;
    count++;
  }
  std::string cookie_line = net::CanonicalCookie::BuildCookieLine(cookies);
  visitor->SetCookieLine(CefString(cookie_line));
}

bool FixInvalidGurl(const CefString& url, GURL& gurl) {
  if (!gurl.is_valid()) {
    GURL fixedGurl = GURL("https://" + url.ToString());
    if (fixedGurl.is_valid() && fixedGurl.host() == url.ToString()) {
      gurl = fixedGurl;
      return true;
    }
    return false;
  }
  return true;
}

#if BUILDFLAG(IS_OHOS)
base::OnceClosure SignalEventClosure(base::WaitableEvent* completion) {
  return base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(completion));
}

void DiscardBool(base::OnceClosure f, CefRefPtr<CefSetCookieCallback> callback, bool b) {
  std::move(f).Run();
  if (callback) {
    callback->OnComplete(b);
  }
}

base::OnceCallback<void(bool)> BoolCallbackAdapter(base::OnceClosure f,
                                                   const CefRefPtr<CefSetCookieCallback>& callback) {
  return base::BindOnce(&DiscardBool, std::move(f), callback);
}

void DiscardUint32(base::OnceClosure f, CefRefPtr<CefDeleteCookiesCallback> callback, uint32_t b) {
  std::move(f).Run();
  if (callback) {
    callback->OnComplete(b);
  }
}

base::OnceCallback<void(uint32_t)> Uint32CallbackAdapter(base::OnceClosure f,
                                                         const CefRefPtr<CefDeleteCookiesCallback>& callback) {
  return base::BindOnce(&DiscardUint32, std::move(f), callback);
}

bool GetCookieAccessResultInclude(net::CookieAccessResult cookie_access_result) {
  const bool is_include = cookie_access_result.status.IsInclude();
  if (!is_include) {
    LOG(WARNING) << "SetCookie failed with reason: "
                 << cookie_access_result.status.GetDebugString();
  }
  return is_include;
}
#endif // BUILDFLAG(IS_OHOS)
}  // namespace

#if BUILDFLAG(IS_OHOS)
void CefCookieManagerImpl::RunCookieTaskSync(base::OnceCallback<void(base::OnceClosure)> task) {
  base::WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                 base::WaitableEvent::InitialState::NOT_SIGNALED);
  RunCookieTaskAsync(base::BindOnce(std::move(task), SignalEventClosure(&completion)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

void CefCookieManagerImpl::RunCookieTaskSync(base::OnceCallback<void(base::OnceCallback<void(bool)>)> task,
                                             const CefRefPtr<CefSetCookieCallback>& callback) {
  base::WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                 base::WaitableEvent::InitialState::NOT_SIGNALED);
  RunCookieTaskAsync(base::BindOnce(
      std::move(task), BoolCallbackAdapter(SignalEventClosure(&completion), callback)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

void CefCookieManagerImpl::RunCookieTaskSync(base::OnceCallback<void(base::OnceCallback<void(uint32_t)>)> task,
                                             const CefRefPtr<CefDeleteCookiesCallback>& callback) {
  base::WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                 base::WaitableEvent::InitialState::NOT_SIGNALED);
  RunCookieTaskAsync(base::BindOnce(
      std::move(task), Uint32CallbackAdapter(SignalEventClosure(&completion), callback)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

void CefCookieManagerImpl::RunCookieTaskAsync(base::OnceClosure task) {
  base::AutoLock lock(task_queue_lock_);
  tasks_.push(std::move(task));
  cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CefCookieManagerImpl::RunCookieTasks,
                                base::Unretained(this)));
}

void CefCookieManagerImpl::RunCookieTasks() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (setting_network_cookie_manager_)
    return;

  std::queue<base::OnceClosure> temp_queue;
  {
    base::AutoLock lock(task_queue_lock_);
    temp_queue.swap(tasks_);
  }
  while (!temp_queue.empty()) {
    std::move(temp_queue.front()).Run();
    temp_queue.pop();
  }
}

net::CookieStore* CefCookieManagerImpl::GetCookieStore() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  if (!cookie_store_) {
    content::CookieStoreConfig cookie_config(
        cookie_store_path_, /* restore_old_session_cookies= */ true,
        /* persist_session_cookies= */ !is_pc_device_,
        /* first_party_sets_enabled= */ false);
    cookie_config.client_task_runner = cookie_store_task_runner_;
    cookie_config.background_task_runner =
        cookie_store_backend_thread_.task_runner();
    cookie_config.crypto_delegate = cookie_config::GetCookieCryptoDelegate();

    cookie_config.cookieable_schemes.insert(
        cookie_config.cookieable_schemes.begin(),
        net::CookieMonster::kDefaultCookieableSchemes,
        net::CookieMonster::kDefaultCookieableSchemes +
            net::CookieMonster::kDefaultCookieableSchemesCount);
    if (allow_file_scheme_cookies_)
      cookie_config.cookieable_schemes.push_back(url::kFileScheme);
    cookie_store_created_ = true;

    cookie_store_ = content::CreateCookieStore(cookie_config, nullptr);
    auto cookie_access_delegate_type = network::mojom::CookieAccessDelegateType::ALWAYS_LEGACY;
    cookie_store_->SetCookieAccessDelegate(
        std::make_unique<network::CookieAccessDelegateImpl>(
            cookie_access_delegate_type, nullptr /* first_party_sets */));
  }

  return cookie_store_.get();
}

void CefCookieManagerImpl::SetNetWorkCookieManagerComplete(base::OnceClosure complete) {
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManagerComplete";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  setting_network_cookie_manager_complete_ = true;
  setting_network_cookie_manager_ = false;
  if (complete)
    std::move(complete).Run();
  RunCookieTasks();
}

void CefCookieManagerImpl::SetNetWorkCookieManagerAsync(base::OnceClosure complete) {
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManagerAsync";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  setting_network_cookie_manager_ = true;
  if (!cookie_store_created_) {
    SetNetWorkCookieManagerComplete(std::move(complete));
    return;
  }
  GetCookieStore()->FlushStore(base::BindOnce(&CefCookieManagerImpl::SetNetWorkCookieManagerComplete,
      base::Unretained(this), std::move(complete)));
}

// static
CefRefPtr<CefCookieManagerImpl> CefCookieManagerImpl::GetInstance() {
  static CefRefPtr<CefCookieManagerImpl> instance = new CefCookieManagerImpl();
  return instance;
}

CefCookieManagerImpl::CefCookieManagerImpl()
    : cookie_store_created_(false),
      setting_network_cookie_manager_(false),
      cookie_store_task_thread_("CookieMonsterClient"),
      cookie_store_backend_thread_("CookieMonsterBackend") {
  cookie_store_task_thread_.Start();
  cookie_store_backend_thread_.Start();
  cookie_store_task_runner_ = cookie_store_task_thread_.task_runner();

  base::PathService::Get(base::DIR_CACHE, &cookie_store_path_);
  cookie_store_path_ = cookie_store_path_.Append("Cookies");
  LOG(DEBUG) << "cookie store database path: " << cookie_store_path_.MaybeAsASCII();

  auto& system_properties_adapter =
      OHOS::NWeb::OhosAdapterHelper::GetInstance().GetSystemPropertiesInstance();
  OHOS::NWeb::ProductDeviceType deviceType =
      system_properties_adapter.GetProductDeviceType();
  is_pc_device_ =
      deviceType == OHOS::NWeb::ProductDeviceType::DEVICE_TYPE_TABLET ||
      deviceType == OHOS::NWeb::ProductDeviceType::DEVICE_TYPE_2IN1;
}
#else
CefCookieManagerImpl::CefCookieManagerImpl() {}
#endif // BUILDFLAG(IS_OHOS)

void CefCookieManagerImpl::SetNetWorkCookieManager() {
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManager";
  RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::SetNetWorkCookieManagerAsync,
      base::Unretained(this)));
#endif // BUILDFLAG(IS_OHOS)
}

void CefCookieManagerImpl::Initialize(
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

bool CefCookieManagerImpl::IsAcceptCookieAllowed() {
  return net_service::NetHelpers::IsAllowAcceptCookies();
}

void CefCookieManagerImpl::PutAcceptCookieEnabled(bool accept) {
  net_service::NetHelpers::accept_cookies = accept;
}

bool CefCookieManagerImpl::IsThirdPartyCookieAllowed() {
  return net_service::NetHelpers::IsThirdPartyCookieAllowed();
}

void CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabled(bool accept) {
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabled accept: " << accept;
  if (!setting_network_cookie_manager_complete_) {
    LOG(INFO) << "network service not started";
    net_service::NetHelpers::third_party_cookies = accept;
    return;
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(
            &CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal),
        this, accept));
    return;
  }

  PutAcceptThirdPartyCookieEnabledInternal(accept);
}

bool CefCookieManagerImpl::IsFileURLSchemeCookiesAllowed() {
  return allow_file_scheme_cookies_;
}

void CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabled(bool allow) {
#if BUILDFLAG(IS_OHOS)
  LOG(INFO) << "CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabled allow: " << allow;
  if (!setting_network_cookie_manager_complete_) {
    bool can_change_schemes = !cookie_store_created_;
    PutAcceptFileURLSchemeCookiesEnabledCompleted(allow, can_change_schemes);
    return;
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::
                               PutAcceptFileURLSchemeCookiesEnabledInternal),
        this, allow));
    return;
  }

  PutAcceptFileURLSchemeCookiesEnabledInternal(allow);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
  if (!visitor.get())
    return false;
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::VisitAllCookies is_sync: " << is_sync;
  if (!setting_network_cookie_manager_complete_) {
    return VisitAllCookiesInternal(visitor, is_sync);
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::VisitAllCookiesInternal),
        this, visitor, is_sync));
    return true;
  }

  return VisitAllCookiesInternal(visitor, is_sync);
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    bool is_sync) {
  if (!visitor.get())
    return false;

  GURL gurl = GURL(url.ToString());
#if BUILDFLAG(IS_OHOS)
  if (!FixInvalidGurl(url, gurl)) {
    return false;
  }
#else
  if (!gurl.is_valid()) {
    return false;
  }
#endif
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::VisitUrlCookies is_sync: " << is_sync;
  if (!setting_network_cookie_manager_complete_) {
    return VisitUrlCookiesInternal(gurl, includeHttpOnly, visitor, is_sync);
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::VisitUrlCookiesInternal),
        this, gurl, includeHttpOnly, visitor, is_sync));
    return true;
  }

  return VisitUrlCookiesInternal(gurl, includeHttpOnly, visitor, is_sync);
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie,
                                     CefRefPtr<CefSetCookieCallback> callback,
                                     bool is_sync, const CefString& str_cookie) {
  GURL gurl = GURL(url.ToString());
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::SetCookie is_sync: " << is_sync;
  if (!setting_network_cookie_manager_complete_) {
    return SetCookieInternal(gurl, cookie, callback, is_sync, str_cookie);
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::SetCookieInternal), this,
        gurl, cookie, callback, is_sync, str_cookie));
    return true;
  }

  return SetCookieInternal(gurl, cookie, callback, is_sync, str_cookie);
}

bool CefCookieManagerImpl::DeleteCookies(
    const CefString& url,
    const CefString& cookie_name,
    bool is_session,
    CefRefPtr<CefDeleteCookiesCallback> callback,
    bool is_sync) {
  // Empty URLs are allowed but not invalid URLs.
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_empty() && !gurl.is_valid()) {
#if BUILDFLAG(IS_OHOS)
    if (callback)
      callback->OnComplete(0);
#endif // BUILDFLAG(IS_OHOS)
    return false;
  }
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::DeleteCookies is_sync: " << is_sync;
  if (!setting_network_cookie_manager_complete_) {
    return DeleteCookiesInternal(gurl, cookie_name, is_session, callback, is_sync);
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::DeleteCookiesInternal), this,
        gurl, cookie_name, is_session, callback, is_sync));
    return true;
  }

  return DeleteCookiesInternal(gurl, cookie_name, is_session, callback, is_sync);
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::FlushStore network_cookie: " << setting_network_cookie_manager_complete_;
  if (!setting_network_cookie_manager_complete_) {
    return FlushStoreInternal(callback);
  }
#endif // BUILDFLAG(IS_OHOS)
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::FlushStoreInternal), this,
        callback));
    return true;
  }

  return FlushStoreInternal(callback);
}

bool CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal(
    bool accept) {
#if BUILDFLAG(IS_OHOS)
  if (!cookie_store_task_runner_->RunsTasksInCurrentSequence()) {
    cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
          base::IgnoreResult(&CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal),
          base::Unretained(this), accept));
    return true;
  }
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
#endif // BUILDFLAG(IS_OHOS)
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;
  net_service::NetHelpers::third_party_cookies = accept;
  GetCookieManager(browser_context)->BlockThirdPartyCookies(!accept);
  return true;
}

void CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledCompleted(
    bool allow,
    bool can_change_schemes) {
  if (can_change_schemes) {
    allow_file_scheme_cookies_ = allow;
  }
}

bool CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledInternal(
    bool allow) {
#if BUILDFLAG(IS_OHOS)
  if (!cookie_store_task_runner_->RunsTasksInCurrentSequence()) {
    cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
          base::IgnoreResult(&CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledInternal),
          base::Unretained(this), allow));
    return true;
  }
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
#endif // BUILDFLAG(IS_OHOS)
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  GetCookieManager(browser_context)
      ->AllowFileSchemeCookies(
          allow,
          base::BindOnce(&CefCookieManagerImpl::
                             PutAcceptFileURLSchemeCookiesEnabledCompleted,
                         base::Unretained(this), allow));
  return true;
}

#if BUILDFLAG(IS_OHOS)
void CefCookieManagerImpl::GetAllCookieListCompleted(CefRefPtr<CefCookieVisitor> visitor,
                                                     CookieManager* cookie_manager,
                                                     base::OnceClosure complete,
                                                     const net::CookieList& cookies) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  ExecuteVisitor(visitor, cookie_manager, cookies);
  if (complete)
    std::move(complete).Run();
}

void CefCookieManagerImpl::GetAllCookieListCookieTask(CefRefPtr<CefCookieVisitor> visitor,
                                                      base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::GetAllCookieListCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!setting_network_cookie_manager_complete_) {
    GetCookieStore()->GetAllCookiesAsync(
        base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCompleted,
                       base::Unretained(this), visitor, nullptr, std::move(complete)));
    return;
  }
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    if (complete)
      std::move(complete).Run();
    return;
  }
  CookieManager* cookie_manager = GetCookieManager(browser_context);
  if (!cookie_manager) {
    if (complete)
      std::move(complete).Run();
    return;
  }
  cookie_manager->GetAllCookies(
      base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCompleted,
                     base::Unretained(this), visitor, cookie_manager, std::move(complete)));
}

void CefCookieManagerImpl::GetCookieListCookieTask(const GURL& url,
                                                   net::CookieOptions options,
                                                   CefRefPtr<CefCookieVisitor> visitor,
                                                   base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::GetCookieListCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!setting_network_cookie_manager_complete_) {
    GetCookieStore()->GetCookieListWithOptionsAsync(
        url, options, net::CookiePartitionKeyCollection(),
        base::BindOnce(&CefCookieManagerImpl::GetCookiesCallbackImpl,
                       base::Unretained(this), visitor, std::move(complete), nullptr));
    return;
  }
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    if (complete)
      std::move(complete).Run();
    return;
  }
  CookieManager* cookie_manager = GetCookieManager(browser_context);
  if (!cookie_manager) {
    if (complete)
      std::move(complete).Run();
    return;
  }

  cookie_manager->GetCookieList(
      url, options, net::CookiePartitionKeyCollection(),
      base::BindOnce(&CefCookieManagerImpl::GetCookiesCallbackImpl,
                     base::Unretained(this), visitor, std::move(complete), cookie_manager));
}

void CefCookieManagerImpl::SetCookieInternalCookieTask(std::unique_ptr<net::CanonicalCookie> cookie,
                                                       const GURL& url,
                                                       const net::CookieOptions& options,
                                                       base::OnceCallback<void(bool)> complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::SetCookieInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!cookie) {
    if (complete)
      std::move(complete).Run(false);
    return;
  }
  if (!setting_network_cookie_manager_complete_) {
    GetCookieStore()->SetCanonicalCookieAsync(
        std::move(cookie), url, options,
        base::BindOnce(GetCookieAccessResultInclude).Then(std::move(complete)));
    return;
  }
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    if (complete)
      std::move(complete).Run(false);
    return;
  }
  CookieManager* cookie_manager = GetCookieManager(browser_context);
  if (!cookie_manager) {
    if (complete)
      std::move(complete).Run(false);
    return;
  }
  cookie_manager->SetCanonicalCookie(
      *cookie, url, options,
      base::BindOnce(GetCookieAccessResultInclude).Then(std::move(complete)));
}

void CefCookieManagerImpl::DeleteCookiesInternalCookieTask(network::mojom::CookieDeletionFilterPtr filter,
                                                           base::OnceCallback<void(uint32_t)> complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::DeleteCookiesInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!filter) {
    if (complete)
      std::move(complete).Run(0);
    return;
  }
  if (!setting_network_cookie_manager_complete_) {
    GetCookieStore()->DeleteAllMatchingInfoAsync(network::DeletionFilterToInfo(std::move(filter)),
                                                 std::move(complete));
    return;
  }
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    if (complete)
      std::move(complete).Run(0);
    return;
  }
  CookieManager* cookie_manager = GetCookieManager(browser_context);
  if (!cookie_manager) {
    if (complete)
      std::move(complete).Run(0);
    return;
  }
  cookie_manager->DeleteCookies(std::move(filter), std::move(complete));
}

void CefCookieManagerImpl::FlushStoreInternalCookieTask(base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::FlushStoreInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!setting_network_cookie_manager_complete_) {
    GetCookieStore()->FlushStore(std::move(complete));
    return;
  }
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    if (complete)
      std::move(complete).Run();
    return;
  }
  CookieManager* cookie_manager = GetCookieManager(browser_context);
  if (!cookie_manager) {
    if (complete)
      std::move(complete).Run();
    return;
  }
  cookie_manager->FlushCookieStore(std::move(complete));
}
#endif // BUILDFLAG(IS_OHOS)

bool CefCookieManagerImpl::VisitAllCookiesInternal(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
#if BUILDFLAG(IS_OHOS)
  DCHECK(visitor);
  LOG(DEBUG) << "CefCookieManagerImpl::VisitAllCookiesInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCookieTask,
                                     base::Unretained(this), visitor));
  } else {
    base::OnceClosure complete;
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCookieTask,
                                      base::Unretained(this), visitor, std::move(complete)));
  }
#else
  DCHECK(ValidContext());
  DCHECK(visitor);

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  if (!is_sync) {
    GetCookieManager(browser_context)
      ->GetAllCookies(base::BindOnce(
          &CefCookieManagerImpl::GetAllCookiesCallbackImpl,
          base::Unretained(this), visitor, browser_context_getter_));
    return true;
  }
  net::CookieList cookie_list;
  GetCookieManager(browser_context)
      ->GetAllCookiesSync(&cookie_list);
  ExecuteVisitor(visitor, browser_context_getter_, cookie_list);
#endif // BUILDFLAG(IS_OHOS)
  return true;
}

bool CefCookieManagerImpl::VisitUrlCookiesInternal(
    const GURL& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    bool is_sync) {
#if !BUILDFLAG(IS_OHOS)
  DCHECK(ValidContext());
#endif // !BUILDFLAG(IS_OHOS)
  DCHECK(visitor);
  DCHECK(url.is_valid());

#if BUILDFLAG(IS_OHOS)
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
#else
  net::CookieOptions options;
  if (includeHttpOnly)
    options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
#endif

#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::VisitUrlCookiesInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::GetCookieListCookieTask,
                                     base::Unretained(this), std::move(url), options, visitor));
  } else {
    base::OnceClosure complete;
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::GetCookieListCookieTask,
                                      base::Unretained(this), std::move(url), options, visitor,
                                      std::move(complete)));
  }
#else
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  if (!is_sync) {
    GetCookieManager(browser_context)
      ->GetCookieList(
          url, options, net::CookiePartitionKeyCollection(),
          base::BindOnce(&CefCookieManagerImpl::GetCookiesCallbackImpl,
                         base::Unretained(this), visitor,
                         browser_context_getter_));
    return true;
  }
  net::CookieAccessResultList include_cookies;
  net::CookieAccessResultList exclude_cookies;
  GetCookieManager(browser_context)
      ->GetCookieListSync(
          url, options, net::CookiePartitionKeyCollection(), &include_cookies,
          &exclude_cookies);
  net::CookieList cookies_list;
  for (const auto& status : include_cookies) {
    cookies_list.push_back(status.cookie);
  }
  ExecuteVisitor(visitor, browser_context_getter_, cookies_list);
#endif // BUILDFLAG(IS_OHOS)
  return true;
}

bool CefCookieManagerImpl::SetCookieInternal(
    const GURL& url,
    const CefCookie& cookie,
    CefRefPtr<CefSetCookieCallback> callback,
    bool is_sync,
    const CefString& str_cookie) {
#if !BUILDFLAG(IS_OHOS)
  DCHECK(ValidContext());
#endif // !BUILDFLAG(IS_OHOS)
  DCHECK(url.is_valid());

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

#if !BUILDFLAG(IS_OHOS)
  net::CookieSameSite same_site =
      net_service::MakeCookieSameSite(cookie.same_site);
  net::CookiePriority priority =
      net_service::MakeCookiePriority(cookie.priority);
#endif

#if !BUILDFLAG(IS_OHOS)
  auto canonical_cookie = net::CanonicalCookie::CreateSanitizedCookie(
      url, name, value, domain, path,
      base::Time(),  // Creation time.
      expiration_time,
      base::Time(),  // Last access time.
      cookie.secure ? true : false, cookie.httponly ? true : false, same_site,
      priority, /*same_party=*/false, net::CookiePartitionKey::Todo());
#else
  std::unique_ptr<net::CanonicalCookie> canonical_cookie(net::CanonicalCookie::Create(
      url, str_cookie.ToString(), base::Time::Now(), absl::nullopt /* server_time */,
      net::CookiePartitionKey::FromWire(net::SchemefulSite(url), absl::nullopt)));
#endif

  if (!canonical_cookie) {
#if BUILDFLAG(IS_OHOS)
    LOG(WARNING) << "SetCookie failed with reason: create canonical cookie failed";
    SetCookieCallbackImpl(callback, false);
#else
    SetCookieCallbackImpl(
        callback, net::CookieAccessResult(net::CookieInclusionStatus(
                      net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)));
#endif // BUILDFLAG(IS_OHOS)
    return true;
  }

  net::CookieOptions options;
  if (cookie.httponly)
    options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::SetCookieInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::SetCookieInternalCookieTask,
                                     base::Unretained(this), std::move(canonical_cookie),
                                     std::move(url), std::move(options)), callback);
  } else {
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::SetCookieInternalCookieTask, base::Unretained(this),
                                      std::move(canonical_cookie), std::move(url), std::move(options),
                                      base::BindOnce(&CefCookieManagerImpl::SetCookieCallbackImpl,
                                                     base::Unretained(this), callback)));
  }
#else
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  if (!is_sync) {
    GetCookieManager(browser_context)
      ->SetCanonicalCookie(
          *canonical_cookie, url, options,
          base::BindOnce(&CefCookieManagerImpl::SetCookieCallbackImpl,
                         base::Unretained(this), callback));
    return true;
  }
  net::CookieAccessResult access_result;
  GetCookieManager(browser_context)
      ->SetCanonicalCookieSync(*canonical_cookie, url, options, &access_result);
  callback->OnComplete(access_result.status.IsInclude());
#endif // BUILDFLAG(IS_OHOS)
  return true;
}

bool CefCookieManagerImpl::DeleteCookiesInternal(
    const GURL& url,
    const CefString& cookie_name,
    bool is_session,
    CefRefPtr<CefDeleteCookiesCallback> callback,
    bool is_sync) {
#if !BUILDFLAG(IS_OHOS)
  DCHECK(ValidContext());
#endif // !BUILDFLAG(IS_OHOS)
  DCHECK(url.is_empty() || url.is_valid());

  network::mojom::CookieDeletionFilterPtr deletion_filter =
      network::mojom::CookieDeletionFilter::New();

  if (url.is_empty()) {
    // Delete all cookies.
  } else if (cookie_name.empty()) {
    // Delete all matching host cookies.
    deletion_filter->host_name = url.host();
  } else {
    // Delete all matching host and domain cookies.
    deletion_filter->url = url;
    deletion_filter->cookie_name = cookie_name;
  }

  if (is_session) {
    deletion_filter->session_control =
        network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
  }
#if BUILDFLAG(IS_OHOS)
  LOG(DEBUG) << "CefCookieManagerImpl::DeleteCookiesInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::DeleteCookiesInternalCookieTask,
                                     base::Unretained(this), std::move(deletion_filter)), callback);
  } else {
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::DeleteCookiesInternalCookieTask,
                                      base::Unretained(this), std::move(deletion_filter),
                                      base::BindOnce(&CefCookieManagerImpl::DeleteCookiesCallbackImpl,
                                                     base::Unretained(this), callback)));
  }
#else
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  if (!is_sync) {
    GetCookieManager(browser_context)
      ->DeleteCookies(
          std::move(deletion_filter),
          base::BindOnce(&CefCookieManagerImpl::DeleteCookiesCallbackImpl,
                         base::Unretained(this), callback));
    return true;
  }
  uint32_t num_deleted = 0;
  GetCookieManager(browser_context)
      ->DeleteCookiesSync(
          std::move(deletion_filter), &num_deleted);
  callback->OnComplete(num_deleted);
#endif // BUILDFLAG(IS_OHOS)
  return true;
}

bool CefCookieManagerImpl::FlushStoreInternal(
    CefRefPtr<CefCompletionCallback> callback) {
#if BUILDFLAG(IS_OHOS)
  RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::FlushStoreInternalCookieTask, base::Unretained(this),
                                    base::BindOnce(&CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner,
                                                   base::Unretained(this), callback)));
#else
  DCHECK(ValidContext());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context)
    return false;

  GetCookieManager(browser_context)
      ->FlushCookieStore(
          base::BindOnce(&CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner,
                         base::Unretained(this), callback));
#endif // BUILDFLAG(IS_OHOS)
  return true;
}

void CefCookieManagerImpl::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefCookieManagerImpl::StoreOrTriggerInitCallback, this,
                       std::move(callback)));
    return;
  }

  if (initialized_) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

bool CefCookieManagerImpl::ValidContext() const {
  return CEF_CURRENTLY_ON_UIT() && initialized_;
}

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
#if BUILDFLAG(IS_OHOS)
  if (context) {
    return context->GetCookieManager(callback);
  }
  if (callback) {
    callback->OnComplete();
  }
  return CefCookieManagerImpl::GetInstance().get();
#else
  return context ? context->GetCookieManager(callback) : nullptr;
#endif // BUILDFLAG(IS_OHOS)
}

bool CefCookieManager::CreateCefCookie(const CefString& url,
                                       const CefString& value,
                                       CefCookie& cef_cookie) {
  return net_service::MakeCefCookie(GURL(url.ToString()), value.ToString(),
                                    cef_cookie);
}

// Always execute the set callback on cookie_store_task_runner_.
void CefCookieManagerImpl::SetCookieCallbackImpl(
    CefRefPtr<CefSetCookieCallback> callback,
#if BUILDFLAG(IS_OHOS)
    bool access_result) {
#else
    net::CookieAccessResult access_result) {
#endif // BUILDFLAG(IS_OHOS)
  if (!callback.get()) {
    return;
  }
#if BUILDFLAG(IS_OHOS)
  callback->OnComplete(access_result);
#else
  const bool is_include = access_result.status.IsInclude();
  if (!is_include) {
    LOG(WARNING) << "SetCookie failed with reason: "
                 << access_result.status.GetDebugString();
  }

  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefSetCookieCallback::OnComplete,
                                        callback.get(), is_include));
#endif // BUILDFLAG(IS_OHOS)
}

void CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get())
    return;
#if BUILDFLAG(IS_OHOS)
  callback->OnComplete();
#else
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
#endif // BUILDFLAG(IS_OHOS)
}

void CefCookieManagerImpl::GetAllCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
#if BUILDFLAG(IS_OHOS)
    base::OnceClosure complete,
    CookieManager* cookie_manager,
#else
    const CefBrowserContext::Getter& browser_context_getter,
#endif // BUILDFLAG(IS_OHOS)
    const net::CookieList& cookies) {
#if BUILDFLAG(IS_OHOS)
  ExecuteVisitor(visitor, cookie_manager, cookies);
  if (complete)
    std::move(complete).Run();
#else
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&ExecuteVisitor, visitor,
                                        browser_context_getter, cookies));
#endif // BUILDFLAG(IS_OHOS)
}

void CefCookieManagerImpl::GetCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
#if BUILDFLAG(IS_OHOS)
    base::OnceClosure complete,
    CookieManager* cookie_manager,
#else
    const CefBrowserContext::Getter& browser_context_getter,
#endif // BUILDFLAG(IS_OHOS)
    const net::CookieAccessResultList& include_cookies,
    const net::CookieAccessResultList&) {
  net::CookieList cookies;
  for (const auto& status : include_cookies) {
    cookies.push_back(status.cookie);
  }
#if BUILDFLAG(IS_OHOS)
  GetAllCookiesCallbackImpl(visitor, std::move(complete), cookie_manager, cookies);
#else
  GetAllCookiesCallbackImpl(visitor, browser_context_getter, cookies);
#endif // BUILDFLAG(IS_OHOS)
}

void CefCookieManagerImpl::DeleteCookiesCallbackImpl(
    CefRefPtr<CefDeleteCookiesCallback> callback,
    uint32_t num_deleted) {
  if (!callback.get())
    return;
#if BUILDFLAG(IS_OHOS)
  callback->OnComplete(num_deleted);
#else
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefDeleteCookiesCallback::OnComplete,
                                        callback.get(), num_deleted));
#endif // BUILDFLAG(IS_OHOS)
}
