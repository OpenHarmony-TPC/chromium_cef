/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "libcef/browser/net_service/cookie_manager_ohos_impl.h"

#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/time_util.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ohos/sys_info_utils.h"
#include "base/synchronization/lock.h"
#include "content/public/common/content_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/cookie_config/cookie_store_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/storage_partition.h"
#include "net_helpers.h"
#include "url/gurl.h"

#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/cookie_manager.h"

#if defined(OHOS_EX_EXCEPTION_LIST)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#endif  // defined(OHOS_EX_EXCEPTION_LIST)

using network::mojom::CookieManager;

namespace {

// Always execute the callback asynchronously.
void RunAsyncCompletionOnUIThread(const CefRefPtr<CefCompletionCallback>& callback) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}

void DeleteCanonicalCookie(CookieManager* cookie_manager,
                           const net::CanonicalCookie& cookie,
                           base::OnceCallback<void(bool)> callback) {
  if (!cookie_manager) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }
  cookie_manager->DeleteCanonicalCookie(cookie, std::move(callback));
}

void ExecuteVisitor(CefRefPtr<CefCookieVisitor> visitor,
                    CookieManager* cookie_manager,
                    const std::vector<net::CanonicalCookie>& cookies) {
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
      DeleteCanonicalCookie(cookie_manager, cc, CookieManager::DeleteCanonicalCookieCallback());
    }
    if (!keepLooping) {
      break;
    }
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

base::OnceClosure SignalEventClosure(base::WaitableEvent* completion) {
  return base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(completion));
}

void DiscardBool(base::OnceClosure f, CefRefPtr<CefSetCookieCallback> callback, bool b) {
  std::move(f).Run();
  if (callback) {
    callback->OnComplete(b);
  }
}

base::OnceCallback<void(bool)> OnceClosureToBoolCallback(base::OnceClosure f,
                                                         const CefRefPtr<CefSetCookieCallback>& callback) {
  return base::BindOnce(&DiscardBool, std::move(f), callback);
}

void DiscardUint32(base::OnceClosure f, CefRefPtr<CefDeleteCookiesCallback> callback, uint32_t b) {
  std::move(f).Run();
  if (callback) {
    callback->OnComplete(b);
  }
}

base::OnceCallback<void(uint32_t)> OnceClosureToUint32Callback(base::OnceClosure f,
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
}  // namespace

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
      std::move(task), OnceClosureToBoolCallback(SignalEventClosure(&completion), callback)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

void CefCookieManagerImpl::RunCookieTaskSync(base::OnceCallback<void(base::OnceCallback<void(uint32_t)>)> task,
                                             const CefRefPtr<CefDeleteCookiesCallback>& callback) {
  base::WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                 base::WaitableEvent::InitialState::NOT_SIGNALED);
  RunCookieTaskAsync(base::BindOnce(
      std::move(task), OnceClosureToUint32Callback(SignalEventClosure(&completion), callback)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

void CefCookieManagerImpl::RunCookieTaskAsync(base::OnceClosure task) {
  base::AutoLock lock(task_queue_lock_);
  tasks_.push_back(std::move(task));

  cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CefCookieManagerImpl::RunPendingCookieTasks,
                                base::Unretained(this)));
}

void CefCookieManagerImpl::RunPendingCookieTasks() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (setting_network_cookie_manager_) {
    LOG(DEBUG) << "network service starting";
    return;
  }

  base::circular_deque<base::OnceClosure> temp_queue;
  {
    base::AutoLock lock(task_queue_lock_);
    temp_queue.swap(tasks_);
  }
  while (!temp_queue.empty()) {
    std::move(temp_queue.front()).Run();
    temp_queue.pop_front();
  }
}

net::CookieStore* CefCookieManagerImpl::GetCookieStore() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  if (!cookie_store_) {
    content::CookieStoreConfig cookie_config(
        cookie_store_path_, /* restore_old_session_cookies= */ true,
        /* persist_session_cookies= */ !(base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kCookieConfigPersistSession)));
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

void CefCookieManagerImpl::SetNetWorkCookieManagerRemoteComplete(
    mojo::PendingRemote<CookieManager> cookie_manager_remote, base::OnceClosure complete) {
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManagerRemoteComplete";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  setting_network_cookie_manager_ = false;
  network_cookie_manager_.Bind(std::move(cookie_manager_remote));
  if (!complete.is_null()) {
    std::move(complete).Run();
  }
  RunPendingCookieTasks();
}

void CefCookieManagerImpl::SetNetWorkCookieManagerRemoteAsync(
    mojo::PendingRemote<CookieManager> cookie_manager_remote, base::OnceClosure complete) {
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManagerRemoteAsync";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  setting_network_cookie_manager_ = true;
  if (!cookie_store_created_) {
    SetNetWorkCookieManagerRemoteComplete(std::move(cookie_manager_remote), std::move(complete));
    return;
  }
  GetCookieStore()->FlushStore(base::BindOnce(&CefCookieManagerImpl::SetNetWorkCookieManagerRemoteComplete,
      base::Unretained(this), std::move(cookie_manager_remote), std::move(complete)));
}

void CefCookieManagerImpl::SetNetWorkCookieManager(
    mojo::PendingRemote<CookieManager> cookie_manager_remote) {
  LOG(INFO) << "CefCookieManagerImpl::SetNetWorkCookieManager cookie_manager_remote";
  RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::SetNetWorkCookieManagerRemoteAsync,
      base::Unretained(this), std::move(cookie_manager_remote)));
}

// static
CefRefPtr<CefCookieManagerImpl> CefCookieManagerImpl::GetInstance() {
  static CefRefPtr<CefCookieManagerImpl> instance = new CefCookieManagerImpl();
  return instance;
}

network::mojom::CookieManager* CefCookieManagerImpl::GetNetworkCookieManager() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!network_cookie_manager_.is_bound())
    return nullptr;
  return network_cookie_manager_.get();
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
}

void CefCookieManagerImpl::Initialize(
    CefBrowserContext::Getter browser_context_getter,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();
  browser_context_getter_ = browser_context_getter;
  RunAsyncCompletionOnUIThread(callback);
}

bool CefCookieManagerImpl::IsAcceptCookieAllowed() {
  return net_service::NetHelpers::IsAllowAcceptCookies();
}

void CefCookieManagerImpl::PutAcceptCookieEnabled(bool accept) {
  net_service::NetHelpers::accept_cookies = accept;

#if defined(OHOS_EX_EXCEPTION_LIST)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kForBrowser)) {
    if (browser_context_getter_) {
      auto cef_browser_context = browser_context_getter_.Run();
      if (cef_browser_context) {
        HostContentSettingsMap* host_content_settings_map =
            HostContentSettingsMapFactory::GetForProfile(
                cef_browser_context->AsBrowserContext());
        if (host_content_settings_map) {
          host_content_settings_map->SetDefaultContentSetting(
              ContentSettingsType::COOKIES,
              net_service::NetHelpers::IsAllowAcceptCookies() ? ContentSetting::CONTENT_SETTING_ALLOW
                                                              : ContentSetting::CONTENT_SETTING_BLOCK);
        }
      }
    }
  }
#endif  // defined(OHOS_EX_EXCEPTION_LIST)
}

bool CefCookieManagerImpl::IsThirdPartyCookieAllowed() {
  return net_service::NetHelpers::IsThirdPartyCookieAllowed();
}

void CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabled(bool accept) {
  LOG(INFO) << "CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabled accept: " << accept;
  net_service::NetHelpers::third_party_cookies = accept;
  PutAcceptThirdPartyCookieEnabledInternal(accept);
}

bool CefCookieManagerImpl::IsFileURLSchemeCookiesAllowed() {
  return allow_file_scheme_cookies_;
}

void CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabled(bool allow) {
  LOG(INFO) << "CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabled allow: " << allow;
  PutAcceptFileURLSchemeCookiesEnabledInternal(allow);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
  if (!visitor.get()) {
    return false;
  }
  LOG(DEBUG) << "CefCookieManagerImpl::VisitAllCookies is_sync: " << is_sync;
  return VisitAllCookiesInternal(visitor, is_sync);
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    bool is_sync,
    bool is_from_ndk) {
  if (!visitor.get()) {
    return false;
  }

  GURL gurl = GURL(url.ToString());
  if (!FixInvalidGurl(url, gurl)) {
    return false;
  }
  LOG(DEBUG) << "CefCookieManagerImpl::VisitUrlCookies is_sync: " << is_sync;
  return VisitUrlCookiesInternal(gurl, includeHttpOnly, visitor, is_sync, is_from_ndk);
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie,
                                     CefRefPtr<CefSetCookieCallback> callback,
                                     bool is_sync, const CefString& str_cookie,
                                     bool includeHttpOnly) {
  GURL gurl = GURL(url.ToString());
  LOG(DEBUG) << "CefCookieManagerImpl::SetCookie is_sync: " << is_sync;
  return SetCookieInternal(gurl, cookie, callback, is_sync, str_cookie, includeHttpOnly);
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
    return false;
  }
  LOG(DEBUG) << "CefCookieManagerImpl::DeleteCookies is_sync: " << is_sync;
  return DeleteCookiesInternal(gurl, cookie_name, is_session, callback, is_sync);
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
  LOG(DEBUG) << "CefCookieManagerImpl::FlushStore";
  return FlushStoreInternal(callback);
}

void CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal(
    bool accept) {
  if (!cookie_store_task_runner_->RunsTasksInCurrentSequence()) {
    cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
          &CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal,
          base::Unretained(this), accept));
    return;
  }
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  auto cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    LOG(INFO) << "network service not started";
    return;
  }
  cookie_manager->BlockThirdPartyCookies(!accept);
}

void CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledCompleted(
    bool allow,
    bool can_change_schemes) {
  if (can_change_schemes) {
    allow_file_scheme_cookies_ = allow;
  }
}

void CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledInternal(
    bool allow) {
  if (!cookie_store_task_runner_->RunsTasksInCurrentSequence()) {
    cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
          &CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledInternal,
          base::Unretained(this), allow));
    return;
  }
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  auto cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    LOG(INFO) << "network service not started";
    bool can_change_schemes = !cookie_store_created_;
    PutAcceptFileURLSchemeCookiesEnabledCompleted(allow, can_change_schemes);
    return;
  }
  cookie_manager->AllowFileSchemeCookies(
          allow,
          base::BindOnce(&CefCookieManagerImpl::PutAcceptFileURLSchemeCookiesEnabledCompleted,
                         base::Unretained(this), allow));
}

void CefCookieManagerImpl::GetAllCookieListCompleted(CefRefPtr<CefCookieVisitor> visitor,
                                                     CookieManager* cookie_manager,
                                                     base::OnceClosure complete,
                                                     const net::CookieList& cookies) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  ExecuteVisitor(visitor, cookie_manager, cookies);
  if (!complete.is_null()) {
    std::move(complete).Run();
  }
}

void CefCookieManagerImpl::GetAllCookieListCookieTask(CefRefPtr<CefCookieVisitor> visitor,
                                                      base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::GetAllCookieListCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  CookieManager* cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    GetCookieStore()->GetAllCookiesAsync(
        base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCompleted,
                       base::Unretained(this), visitor, nullptr, std::move(complete)));
    return;
  }
  cookie_manager->GetAllCookies(
      base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCompleted,
                     base::Unretained(this), visitor, cookie_manager, std::move(complete)));
}

bool CefCookieManagerImpl::VisitAllCookiesInternal(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
  DCHECK(visitor);
  LOG(DEBUG) << "CefCookieManagerImpl::VisitAllCookiesInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCookieTask,
                                     base::Unretained(this), visitor));
  } else {
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::GetAllCookieListCookieTask,
                                      base::Unretained(this), visitor, base::NullCallback()));
  }
  return true;
}

void CefCookieManagerImpl::GetCookieListCookieTask(const GURL& url,
                                                   net::CookieOptions options,
                                                   CefRefPtr<CefCookieVisitor> visitor,
                                                   base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::GetCookieListCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  CookieManager* cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    GetCookieStore()->GetCookieListWithOptionsAsync(
        url, options, net::CookiePartitionKeyCollection(),
        base::BindOnce(&CefCookieManagerImpl::GetCookiesCallbackImpl,
                       base::Unretained(this), visitor, std::move(complete), nullptr));
    return;
  }

  cookie_manager->GetCookieList(
      url, options, net::CookiePartitionKeyCollection(),
      base::BindOnce(&CefCookieManagerImpl::GetCookiesCallbackImpl,
                     base::Unretained(this), visitor, std::move(complete), cookie_manager));
}

bool CefCookieManagerImpl::VisitUrlCookiesInternal(
    const GURL& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    bool is_sync,
    bool is_from_ndk) {
  DCHECK(visitor);
  DCHECK(url.is_valid());

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  if (is_from_ndk && !includeHttpOnly) {
    options.set_exclude_httponly();
  }
  LOG(DEBUG) << "CefCookieManagerImpl::VisitUrlCookiesInternal is_sync : " << is_sync;
  if (is_sync) {
    RunCookieTaskSync(base::BindOnce(&CefCookieManagerImpl::GetCookieListCookieTask,
                                     base::Unretained(this), std::move(url), options, visitor));
  } else {
    RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::GetCookieListCookieTask,
                                      base::Unretained(this), std::move(url), options, visitor,
                                      base::NullCallback()));
  }
  return true;
}

void CefCookieManagerImpl::SetCookieInternalCookieTask(std::unique_ptr<net::CanonicalCookie> cookie,
                                                       const GURL& url,
                                                       const net::CookieOptions& options,
                                                       base::OnceCallback<void(bool)> complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::SetCookieInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!cookie) {
    if (!complete.is_null()) {
      std::move(complete).Run(false);
    }
    return;
  }

  CookieManager* cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    GetCookieStore()->SetCanonicalCookieAsync(
        std::move(cookie), url, options,
        base::BindOnce(GetCookieAccessResultInclude).Then(std::move(complete)));
    return;
  }
  cookie_manager->SetCanonicalCookie(
      *cookie, url, options,
      base::BindOnce(GetCookieAccessResultInclude).Then(std::move(complete)));
}

bool CefCookieManagerImpl::SetCookieInternal(
    const GURL& url,
    const CefCookie& cookie,
    CefRefPtr<CefSetCookieCallback> callback,
    bool is_sync,
    const CefString& str_cookie,
    bool includeHttpOnly) {
  DCHECK(url.is_valid());

  std::unique_ptr<net::CanonicalCookie> canonical_cookie(net::CanonicalCookie::Create(
      url, str_cookie.ToString(), base::Time::Now(), absl::nullopt /* server_time */,
      net::CookiePartitionKey::FromWire(net::SchemefulSite(url), absl::nullopt)));
  if (!canonical_cookie) {
    LOG(WARNING) << "SetCookie failed with reason: create canonical cookie failed";
    SetCookieCallbackImpl(callback, false);
    return true;
  }

  net::CookieOptions options;
  if (includeHttpOnly || cookie.httponly) {
    options.set_include_httponly();
  }
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

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
  return true;
}

void CefCookieManagerImpl::DeleteCookiesInternalCookieTask(network::mojom::CookieDeletionFilterPtr filter,
                                                           base::OnceCallback<void(uint32_t)> complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::DeleteCookiesInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!filter) {
    if (!complete.is_null()) {
      std::move(complete).Run(0);
    }
    return;
  }

  CookieManager* cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    GetCookieStore()->DeleteAllMatchingInfoAsync(network::DeletionFilterToInfo(std::move(filter)),
                                                 std::move(complete));
    return;
  }
  cookie_manager->DeleteCookies(std::move(filter), std::move(complete));
}

bool CefCookieManagerImpl::DeleteCookiesInternal(
    const GURL& url,
    const CefString& cookie_name,
    bool is_session,
    CefRefPtr<CefDeleteCookiesCallback> callback,
    bool is_sync) {
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
  return true;
}

void CefCookieManagerImpl::FlushStoreInternalCookieTask(base::OnceClosure complete) {
  LOG(DEBUG) << "CefCookieManagerImpl::FlushStoreInternalCookieTask";
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  CookieManager* cookie_manager = GetNetworkCookieManager();
  if (!cookie_manager) {
    GetCookieStore()->FlushStore(std::move(complete));
    return;
  }
  cookie_manager->FlushCookieStore(std::move(complete));
}

bool CefCookieManagerImpl::FlushStoreInternal(
    CefRefPtr<CefCompletionCallback> callback) {
  RunCookieTaskAsync(base::BindOnce(&CefCookieManagerImpl::FlushStoreInternalCookieTask, base::Unretained(this),
                                    base::BindOnce(&CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner,
                                                   base::Unretained(this), callback)));
  return true;
}

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  if (context) {
    return context->GetCookieManager(callback);
  }
  if (callback) {
    callback->OnComplete();
  }
  return CefCookieManagerImpl::GetInstance().get();
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
    bool access_result) {
  if (!callback.get()) {
    return;
  }
  callback->OnComplete(access_result);
}

void CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get()) {
    return;
  }
  callback->OnComplete();
}

void CefCookieManagerImpl::GetAllCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    base::OnceClosure complete,
    CookieManager* cookie_manager,
    const net::CookieList& cookies) {
  ExecuteVisitor(visitor, cookie_manager, cookies);
  if (!complete.is_null()) {
    std::move(complete).Run();
  }
}

void CefCookieManagerImpl::GetCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    base::OnceClosure complete,
    CookieManager* cookie_manager,
    const net::CookieAccessResultList& include_cookies,
    const net::CookieAccessResultList&) {
  net::CookieList cookies;
  for (const auto& status : include_cookies) {
    cookies.push_back(status.cookie);
  }
  GetAllCookiesCallbackImpl(visitor, std::move(complete), cookie_manager, cookies);
}

void CefCookieManagerImpl::DeleteCookiesCallbackImpl(
    CefRefPtr<CefDeleteCookiesCallback> callback,
    uint32_t num_deleted) {
  if (!callback.get()) {
    return;
  }
  callback->OnComplete(num_deleted);
}
