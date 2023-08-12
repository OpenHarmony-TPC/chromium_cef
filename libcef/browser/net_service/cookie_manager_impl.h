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

// Implementation of the CefCookieManager interface. May be created on any
// thread.
class CefCookieManagerImpl : public CefCookieManager {
 public:
  CefCookieManagerImpl();

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
  bool VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) override;
  bool VisitUrlCookies(const CefString& url,
                       bool includeHttpOnly,
                       CefRefPtr<CefCookieVisitor> visitor) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie,
                 CefRefPtr<CefSetCookieCallback> callback) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name,
                     bool is_session,
                     CefRefPtr<CefDeleteCookiesCallback> callback) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

 private:
  bool PutAcceptThirdPartyCookieEnabledInternal(bool accept);
  void PutAcceptFileURLSchemeCookiesEnabledCompleted(bool allow,
                                                     bool can_change_schemes);
  bool PutAcceptFileURLSchemeCookiesEnabledInternal(bool allow);
  bool VisitAllCookiesInternal(CefRefPtr<CefCookieVisitor> visitor);
  bool VisitUrlCookiesInternal(const GURL& url,
                               bool includeHttpOnly,
                               CefRefPtr<CefCookieVisitor> visitor);
  bool SetCookieInternal(const GURL& url,
                         const CefCookie& cookie,
                         CefRefPtr<CefSetCookieCallback> callback);
  bool DeleteCookiesInternal(const GURL& url,
                             const CefString& cookie_name,
                             bool is_session,
                             CefRefPtr<CefDeleteCookiesCallback> callback);
  bool FlushStoreInternal(CefRefPtr<CefCompletionCallback> callback);

  void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                             net::CookieAccessResult access_result);
  void GetCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
      const CefBrowserContext::Getter& browser_context_getter,
      const net::CookieAccessResultList& include_cookies,
      const net::CookieAccessResultList&);
  void GetAllCookiesCallbackImpl(
      CefRefPtr<CefCookieVisitor> visitor,
      const CefBrowserContext::Getter& browser_context_getter,
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
  base::Thread cookie_thread;
  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;

  IMPLEMENT_REFCOUNTING(CefCookieManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_
