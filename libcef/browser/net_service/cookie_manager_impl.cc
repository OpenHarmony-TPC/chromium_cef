// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_manager_impl.h"

#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/time_util.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OHOS)
#include "libcef/browser/net_service/net_helpers.h"
#endif

using network::mojom::CookieManager;

namespace {

// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  CEF_REQUIRE_UIT();

  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}

// Do not keep a reference to the object returned by this method.
CookieManager* GetCookieManager(CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();

  return browser_context->AsBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

// Always execute the callback asynchronously.
void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}
#if !BUILDFLAG(IS_OHOS)
// Always execute the callback asynchronously.
void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                           net::CookieAccessResult access_result) {
  if (!callback.get()) {
    return;
  }
  const bool is_include = access_result.status.IsInclude();
  if (!is_include) {
    LOG(WARNING) << "SetCookie failed with reason: "
                 << access_result.status.GetDebugString();
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefSetCookieCallback::OnComplete,
                                        callback.get(), is_include));
}

// Always execute the callback asynchronously.
void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                               uint32_t num_deleted) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefDeleteCookiesCallback::OnComplete,
                                        callback.get(), num_deleted));
}
#endif

void ExecuteVisitor(CefRefPtr<CefCookieVisitor> visitor,
                    const CefBrowserContext::Getter& browser_context_getter,
                    const std::vector<net::CanonicalCookie>& cookies) {
  CEF_REQUIRE_UIT();

  auto browser_context = GetBrowserContext(browser_context_getter);
  if (!browser_context) {
    return;
  }

  auto cookie_manager = GetCookieManager(browser_context);

  int total = cookies.size(), count = 0;

#ifdef OHOS_COOKIE
  if (total == 0) {
    CefCookie cookie;
    bool deleteCookie = false;
    visitor->Visit(cookie, 0, 0, deleteCookie);
    return;
  }
#endif

  for (const auto& cc : cookies) {
    CefCookie cookie;
    net_service::MakeCefCookie(cc, cookie);

    bool deleteCookie = false;
    bool keepLooping = visitor->Visit(cookie, count, total, deleteCookie);
    if (deleteCookie) {
      cookie_manager->DeleteCanonicalCookie(
          cc, CookieManager::DeleteCanonicalCookieCallback());
    }
    if (!keepLooping) {
      break;
    }
    count++;
  }
#ifdef OHOS_COOKIE
  std::string cookie_line = net::CanonicalCookie::BuildCookieLine(cookies);
  visitor->SetCookieLine(CefString(cookie_line));
#endif
}

#if !BUILDFLAG(IS_OHOS)
// Always execute the callback asynchronously.
void GetAllCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieList& cookies) {
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&ExecuteVisitor, visitor,
                                        browser_context_getter, cookies));
}

void GetCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieAccessResultList& include_cookies,
    const net::CookieAccessResultList&) {
  net::CookieList cookies;
  for (const auto& status : include_cookies) {
    cookies.push_back(status.cookie);
  }
  GetAllCookiesCallbackImpl(visitor, browser_context_getter, cookies);
}
#endif

#ifdef OHOS_COOKIE
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
#endif  // #ifdef OHOS_COOKIE
}  // namespace

#ifndef OHOS_COOKIE
CefCookieManagerImpl::CefCookieManagerImpl() : cookie_thread{"CookieThread"} {
  cookie_thread.Start();
  cookie_store_task_runner_ = cookie_thread.task_runner();
}
#else
CefCookieManagerImpl::CefCookieManagerImpl() {}
#endif

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
    for (auto& init_callback : init_callbacks_) {
      std::move(init_callback).Run();
    }
    init_callbacks_.clear();
  }

  RunAsyncCompletionOnUIThread(callback);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
  if (!visitor.get()) {
    return false;
  }

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
  if (!visitor.get()) {
    return false;
  }

  GURL gurl = GURL(url.ToString());
#ifdef OHOS_COOKIE
  if (!FixInvalidGurl(url, gurl)) {
    return false;
  }
#else
  if (!gurl.is_valid()) {
    return false;
  }
#endif  // #ifdef OHOS_COOKIE

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
                                     bool is_sync) {
  GURL gurl = GURL(url.ToString());
#ifdef OHOS_COOKIE
  if (!FixInvalidGurl(url, gurl)) {
    return false;
  }
#else
  if (!gurl.is_valid()) {
    return false;
  }
#endif

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::SetCookieInternal), this,
        gurl, cookie, callback, is_sync));
    return true;
  }

  return SetCookieInternal(gurl, cookie, callback, is_sync);
}

bool CefCookieManagerImpl::DeleteCookies(
    const CefString& url,
    const CefString& cookie_name,
#if BUILDFLAG(IS_OHOS)
    bool is_session,
#endif
    CefRefPtr<CefDeleteCookiesCallback> callback,
    bool is_sync) {
  // Empty URLs are allowed but not invalid URLs.
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_empty() && !gurl.is_valid()) {
    return false;
  }

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::DeleteCookiesInternal), this,
        gurl, cookie_name,
#if BUILDFLAG(IS_OHOS)
        is_session,
#endif
        callback, is_sync));
    return true;
  }

  return DeleteCookiesInternal(gurl, cookie_name,
#if BUILDFLAG(IS_OHOS)
                               is_session,
#endif
                               callback, is_sync);
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::FlushStoreInternal), this,
        callback));
    return true;
  }

  return FlushStoreInternal(callback);
}

bool CefCookieManagerImpl::VisitAllCookiesInternal(
    CefRefPtr<CefCookieVisitor> visitor, bool is_sync) {
  DCHECK(ValidContext());
  DCHECK(visitor);

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  if (!is_sync) {
    GetCookieManager(browser_context)
        ->GetAllCookies(base::BindOnce(
#if BUILDFLAG(IS_OHOS)
            &CefCookieManagerImpl::GetAllCookiesCallbackImpl,
            base::Unretained(this),
#else
            &GetAllCookiesCallbackImpl,
#endif
            visitor, browser_context_getter_));
    return true;
  }

#ifdef OHOS_COOKIE
  net::CookieList cookie_list;
  GetCookieManager(browser_context)->GetAllCookiesSync(&cookie_list);
  ExecuteVisitor(visitor, browser_context_getter_, cookie_list);
  return true;
#endif
}

bool CefCookieManagerImpl::VisitUrlCookiesInternal(
    const GURL& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    bool is_sync) {
  DCHECK(ValidContext());
  DCHECK(visitor);
  DCHECK(url.is_valid());

#ifdef OHOS_COOKIE
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
#else
  net::CookieOptions options;
  if (includeHttpOnly) {
    options.set_include_httponly();
  }
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
#endif  // #ifdef OHOS_COOKIE

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  if (!is_sync) {
    GetCookieManager(browser_context)
        ->GetCookieList(url, options, net::CookiePartitionKeyCollection(),
                        base::BindOnce(
#if BUILDFLAG(IS_OHOS)
                            &CefCookieManagerImpl::GetCookiesCallbackImpl,
                            base::Unretained(this),
#else
                            &GetCookiesCallbackImpl,
#endif
                            visitor, browser_context_getter_));
    return true;
  }

#ifdef OHOS_COOKIE
  net::CookieAccessResultList include_cookies;
  net::CookieAccessResultList exclude_cookies;
  GetCookieManager(browser_context)
      ->GetCookieListSync(url, options, net::CookiePartitionKeyCollection(),
                          &include_cookies, &exclude_cookies);
  net::CookieList cookies_list;
  for (const auto& status : include_cookies) {
    cookies_list.push_back(status.cookie);
  }
  ExecuteVisitor(visitor, browser_context_getter_, cookies_list);
  return true;
#endif
}

bool CefCookieManagerImpl::SetCookieInternal(
    const GURL& url,
    const CefCookie& cookie,
    CefRefPtr<CefSetCookieCallback> callback,
    bool is_sync) {
  DCHECK(ValidContext());
  DCHECK(url.is_valid());

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires) {
    expiration_time = CefBaseTime(cookie.expires);
  }

  net::CookieSameSite same_site =
      net_service::MakeCookieSameSite(cookie.same_site);
  net::CookiePriority priority =
      net_service::MakeCookiePriority(cookie.priority);

  auto canonical_cookie = net::CanonicalCookie::CreateSanitizedCookie(
      url, name, value, domain, path,
      base::Time(),  // Creation time.
      expiration_time,
      base::Time(),  // Last access time.
      cookie.secure ? true : false, cookie.httponly ? true : false, same_site,
      priority, /*same_party=*/false, /*partition_key=*/absl::nullopt);

  if (!canonical_cookie) {
    SetCookieCallbackImpl(
        callback, net::CookieAccessResult(net::CookieInclusionStatus(
                      net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)));
    return true;
  }

  net::CookieOptions options;
  if (cookie.httponly) {
    options.set_include_httponly();
  }
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  if (!is_sync) {
    GetCookieManager(browser_context)
        ->SetCanonicalCookie(*canonical_cookie, url, options,
                             base::BindOnce(
#if BUILDFLAG(IS_OHOS)
                                 &CefCookieManagerImpl::SetCookieCallbackImpl,
                                 base::Unretained(this),
#else
                                 SetCookieCallbackImpl,
#endif
                                 callback));
    return true;
  }

#ifdef OHOS_COOKIE
  net::CookieAccessResult access_result;
  GetCookieManager(browser_context)
      ->SetCanonicalCookieSync(*canonical_cookie, url, options, &access_result);
  callback->OnComplete(access_result.status.IsInclude());
  return true;
#endif
}

bool CefCookieManagerImpl::DeleteCookiesInternal(
    const GURL& url,
    const CefString& cookie_name,
#if BUILDFLAG(IS_OHOS)
    bool is_session,
#endif
    CefRefPtr<CefDeleteCookiesCallback> callback,
    bool is_sync) {
  DCHECK(ValidContext());
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

#if BUILDFLAG(IS_OHOS)
  if (is_session) {
    deletion_filter->session_control =
        network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
  }
#endif

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  if (!is_sync) {
    GetCookieManager(browser_context)
        ->DeleteCookies(std::move(deletion_filter),
                        base::BindOnce(
#if BUILDFLAG(IS_OHOS)
                            &CefCookieManagerImpl::DeleteCookiesCallbackImpl,
                            base::Unretained(this),
#else
                            DeleteCookiesCallbackImpl,
#endif
                            callback));
    return true;
  }

#ifdef OHOS_COOKIE
  uint32_t num_deleted = 0;
  GetCookieManager(browser_context)
      ->DeleteCookiesSync(std::move(deletion_filter), &num_deleted);
  callback->OnComplete(num_deleted);
  return true;
#endif
}

bool CefCookieManagerImpl::FlushStoreInternal(
    CefRefPtr<CefCompletionCallback> callback) {
  DCHECK(ValidContext());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->FlushCookieStore(base::BindOnce(
#if BUILDFLAG(IS_OHOS)
          &CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner,
          base::Unretained(this),
#else
          RunAsyncCompletionOnUIThread,
#endif
          callback));
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

#if BUILDFLAG(IS_OHOS)
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
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::
                               PutAcceptFileURLSchemeCookiesEnabledInternal),
        this, allow));
    return;
  }
  PutAcceptFileURLSchemeCookiesEnabledInternal(allow);
}

bool CefCookieManagerImpl::PutAcceptThirdPartyCookieEnabledInternal(
    bool accept) {
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }
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
  DCHECK(ValidContext());
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->AllowFileSchemeCookies(
          allow,
          base::BindOnce(&CefCookieManagerImpl::
                             PutAcceptFileURLSchemeCookiesEnabledCompleted,
                         base::Unretained(this), allow));
  return true;
}
#endif  // IS_OHOS

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  return context ? context->GetCookieManager(callback) : nullptr;
}

#if BUILDFLAG(IS_OHOS)
bool CefCookieManager::CreateCefCookie(const CefString& url,
                                       const CefString& value,
                                       CefCookie& cef_cookie) {
  return net_service::MakeCefCookie(GURL(url.ToString()), value.ToString(),
                                    cef_cookie);
}

// Always execute the set callback on cookie_store_task_runner_.
void CefCookieManagerImpl::SetCookieCallbackImpl(
    CefRefPtr<CefSetCookieCallback> callback,
    net::CookieAccessResult access_result) {
  if (!callback.get()) {
    return;
  }
  const bool is_include = access_result.status.IsInclude();
  if (!is_include) {
    LOG(WARNING) << "SetCookie failed with reason: "
                 << access_result.status.GetDebugString();
  }

  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefSetCookieCallback::OnComplete,
                                        callback.get(), is_include));
}

void CefCookieManagerImpl::RunAsyncCompletionOnTaskRunner(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}

void CefCookieManagerImpl::GetAllCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieList& cookies) {
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&ExecuteVisitor, visitor,
                                        browser_context_getter, cookies));
}

void CefCookieManagerImpl::GetCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieAccessResultList& include_cookies,
    const net::CookieAccessResultList&) {
  net::CookieList cookies;
  for (const auto& status : include_cookies) {
    cookies.push_back(status.cookie);
  }
  GetAllCookiesCallbackImpl(visitor, browser_context_getter, cookies);
}

void CefCookieManagerImpl::DeleteCookiesCallbackImpl(
    CefRefPtr<CefDeleteCookiesCallback> callback,
    uint32_t num_deleted) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefDeleteCookiesCallback::OnComplete,
                                        callback.get(), num_deleted));
}
#endif  // IS_OHOS
