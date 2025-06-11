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
#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_OHOS_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_OHOS_IMPL_H_

#include <atomic>

#include "include/cef_cookie.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

// Implementation of the CefCookieManager interface. May be created on any
// thread.
class CefCookieManagerImpl : public CefCookieManager {
 public:
  static CefRefPtr<CefCookieManagerImpl> GetInstance();

  CefCookieManagerImpl(const CefCookieManagerImpl&) = delete;
  CefCookieManagerImpl& operator=(const CefCookieManagerImpl&) = delete;

  // Called on the UI thread after object creation and before any other object
  // methods are executed on the UI thread.
  void Initialize(CefBrowserContext::Getter browser_context_getter,
                  CefRefPtr<CefCompletionCallback> callback);

  // CefCookieManager methods.
  bool IsAcceptCookieAllowed() override;
  void PutAcceptCookieEnabled(bool accept) override;
  bool IsThirdPartyCookieAllowed() override;
  void PutAcceptThirdPartyCookieEnabled(bool accept) override;
  bool IsFileURLSchemeCookiesAllowed() override;
  void PutAcceptFileURLSchemeCookiesEnabled(bool allow) override;
  bool VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor,
                       bool is_sync) override;
  bool VisitUrlCookies(const CefString& url,
                       bool includeHttpOnly,
                       CefRefPtr<CefCookieVisitor> visitor,
                       bool is_sync,
                       bool is_from_ndk) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie,
                 CefRefPtr<CefSetCookieCallback> callback,
                 bool is_sync,
                 const CefString& str_cookie,
                 bool includeHttpOnly) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name,
                     bool is_session,
                     CefRefPtr<CefDeleteCookiesCallback> callback,
                     bool is_sync) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

  void SetNetWorkCookieManager(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote);

 private:
  CefCookieManagerImpl();

  network::mojom::CookieManager* GetNetworkCookieManager();

  void SetNetWorkCookieManagerRemoteComplete(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote, base::OnceClosure complete);
  void SetNetWorkCookieManagerRemoteAsync(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote, base::OnceClosure complete);

  net::CookieStore* GetCookieStore();

  void RunPendingCookieTasks();
  void RunCookieTaskAsync(base::OnceClosure task);
  void RunCookieTaskSync(base::OnceCallback<void(base::OnceClosure)> task);
  void RunCookieTaskSync(base::OnceCallback<void(base::OnceCallback<void(bool)>)> task,
                         const CefRefPtr<CefSetCookieCallback>& callback);
  void RunCookieTaskSync(base::OnceCallback<void(base::OnceCallback<void(uint32_t)>)> task,
                         const CefRefPtr<CefDeleteCookiesCallback>& callback);

  void GetAllCookieListCompleted(CefRefPtr<CefCookieVisitor> visitor,
                                 network::mojom::CookieManager* cookie_manager,
                                 base::OnceClosure complete,
                                 const net::CookieList& cookies);
  void GetAllCookieListCookieTask(CefRefPtr<CefCookieVisitor> visitor, base::OnceClosure complete);

  void GetCookieListCookieTask(const GURL& url,
                               net::CookieOptions options,
                               CefRefPtr<CefCookieVisitor> visitor,
                               base::OnceClosure complete);

  void SetCookieInternalCookieTask(std::unique_ptr<net::CanonicalCookie> cookie,
                                   const GURL& url,
                                   const net::CookieOptions& options,
                                   base::OnceCallback<void(bool)> complete);

  void DeleteCookiesInternalCookieTask(network::mojom::CookieDeletionFilterPtr filter,
                                       base::OnceCallback<void(uint32_t)> complete);

  void FlushStoreInternalCookieTask(base::OnceClosure complete);

  void PutAcceptThirdPartyCookieEnabledInternal(bool accept);
  void PutAcceptFileURLSchemeCookiesEnabledCompleted(bool allow,
                                                     bool can_change_schemes);
  void PutAcceptFileURLSchemeCookiesEnabledInternal(bool allow);
  bool VisitAllCookiesInternal(CefRefPtr<CefCookieVisitor> visitor,
                               bool is_sync);
  bool VisitUrlCookiesInternal(const GURL& url,
                               bool includeHttpOnly,
                               CefRefPtr<CefCookieVisitor> visitor,
                               bool is_sync,
                               bool is_from_ndk);
  bool SetCookieInternal(const GURL& url,
                         const CefCookie& cookie,
                         CefRefPtr<CefSetCookieCallback> callback,
                         bool is_sync,
                         const CefString& str_cookie,
                         bool includeHttpOnly);
  bool DeleteCookiesInternal(const GURL& url,
                             const CefString& cookie_name,
                             bool is_session,
                             CefRefPtr<CefDeleteCookiesCallback> callback,
                             bool is_sync);
  bool FlushStoreInternal(CefRefPtr<CefCompletionCallback> callback);

  void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                             bool access_result);
  void GetCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
      base::OnceClosure complete,
      network::mojom::CookieManager* cookie_manager,
      const net::CookieAccessResultList& include_cookies,
      const net::CookieAccessResultList&);
  void GetAllCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
      base::OnceClosure complete,
      network::mojom::CookieManager* cookie_manager,
      const net::CookieList& cookies);
  void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                                 uint32_t num_deleted);
  void RunAsyncCompletionOnTaskRunner(
      CefRefPtr<CefCompletionCallback> callback);
  // If the context is fully initialized execute |callback|, otherwise
  // store it until the context is fully initialized.
  void StoreOrTriggerInitCallback(base::OnceClosure callback);

  std::atomic<bool> allow_file_scheme_cookies_ = false;

  bool cookie_store_created_;
  bool setting_network_cookie_manager_;

  base::Thread cookie_store_task_thread_;
  base::Thread cookie_store_backend_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;

  std::unique_ptr<net::CookieStore> cookie_store_;

  base::FilePath cookie_store_path_;

  base::Lock task_queue_lock_;
  base::circular_deque<base::OnceClosure> tasks_ GUARDED_BY(task_queue_lock_);

  mojo::Remote<network::mojom::CookieManager> network_cookie_manager_;

  CefBrowserContext::Getter browser_context_getter_;

  IMPLEMENT_REFCOUNTING(CefCookieManagerImpl);
};

#endif // CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_OHOS_IMPL_H_
