// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_

#include <atomic>

#include "include/cef_cookie.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_path.h"
#include "base/threading/thread.h"

#if BUILDFLAG(IS_OHOS)
#include <queue>

#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#endif // BUILDFLAG(IS_OHOS)

// Implementation of the CefCookieManager interface. May be created on any
// thread.
class CefCookieManagerImpl : public CefCookieManager {
 public:
#if BUILDFLAG(IS_OHOS)
  static CefRefPtr<CefCookieManagerImpl> GetInstance();
#else
  CefCookieManagerImpl();
#endif // BUILDFLAG(IS_OHOS)

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
                       bool is_sync) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie,
                 CefRefPtr<CefSetCookieCallback> callback,
                 bool is_sync, const CefString& str_cookie) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name,
                     bool is_session,
                     CefRefPtr<CefDeleteCookiesCallback> callback,
                     bool is_sync) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

  void SetNetWorkCookieManager() override;

 private:
#if BUILDFLAG(IS_OHOS)
  CefCookieManagerImpl();

  void SetNetWorkCookieManagerComplete(base::OnceClosure complete);
  void SetNetWorkCookieManagerAsync(base::OnceClosure complete);

  net::CookieStore* GetCookieStore();

  void RunCookieTasks(base::OnceClosure task);
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
#endif // BUILDFLAG(IS_OHOS)

  bool PutAcceptThirdPartyCookieEnabledInternal(bool accept);
  void PutAcceptFileURLSchemeCookiesEnabledCompleted(bool allow,
                                                     bool can_change_schemes);
  bool PutAcceptFileURLSchemeCookiesEnabledInternal(bool allow);
  bool VisitAllCookiesInternal(CefRefPtr<CefCookieVisitor> visitor,
                               bool is_sync);
  bool VisitUrlCookiesInternal(const GURL& url,
                               bool includeHttpOnly,
                               CefRefPtr<CefCookieVisitor> visitor,
                               bool is_sync);
  bool SetCookieInternal(const GURL& url,
                         const CefCookie& cookie,
                         CefRefPtr<CefSetCookieCallback> callback,
                         bool is_sync,
                         const CefString& str_cookie);
  bool DeleteCookiesInternal(const GURL& url,
                             const CefString& cookie_name,
                             bool is_session,
                             CefRefPtr<CefDeleteCookiesCallback> callback,
                             bool is_sync);
  bool FlushStoreInternal(CefRefPtr<CefCompletionCallback> callback);

  void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                          #if BUILDFLAG(IS_OHOS)
                             bool access_result);
                          #else
                             net::CookieAccessResult access_result);
                          #endif // BUILDFLAG(IS_OHOS)
  void GetCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
    #if BUILDFLAG(IS_OHOS)
      base::OnceClosure complete,
      network::mojom::CookieManager* cookie_manager,
    #else
      const CefBrowserContext::Getter& browser_context_getter,
    #endif // BUILDFLAG(IS_OHOS)
      const net::CookieAccessResultList& include_cookies,
      const net::CookieAccessResultList&);
  void GetAllCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
    #if BUILDFLAG(IS_OHOS)
      base::OnceClosure complete,
      network::mojom::CookieManager* cookie_manager,
    #else
      const CefBrowserContext::Getter& browser_context_getter,
    #endif // BUILDFLAG(IS_OHOS)
      const net::CookieList& cookies);
  void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                                 uint32_t num_deleted);
  void RunAsyncCompletionOnTaskRunner(
      CefRefPtr<CefCompletionCallback> callback);
  // If the context is fully initialized execute |callback|, otherwise
  // store it until the context is fully initialized.
  void StoreOrTriggerInitCallback(base::OnceClosure callback);

  bool ValidContext() const;

  // Only accessed on the UI thread. Will be non-null after Initialize().
  CefBrowserContext::Getter browser_context_getter_;

  bool initialized_ = false;
  std::atomic<bool> allow_file_scheme_cookies_ = false;
  std::vector<base::OnceClosure> init_callbacks_;

#if BUILDFLAG(IS_OHOS)
  bool cookie_store_created_;
  bool setting_network_cookie_manager_;

  base::Thread cookie_store_task_thread_;
  base::Thread cookie_store_backend_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;

  std::unique_ptr<net::CookieStore> cookie_store_;

  base::FilePath cookie_store_path_;

  std::queue<base::OnceClosure> tasks_;

  bool setting_network_cookie_manager_complete_ = false;
  bool is_pc_device_ = false;
#endif // BUILDFLAG(IS_OHOS)

  IMPLEMENT_REFCOUNTING(CefCookieManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_
