// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net_service/restrict_cookie_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net_helpers.h"
#include "url/gurl.h"

class ProxyingRestrictedCookieManagerListener
    : public network::mojom::CookieChangeListener {
 public:
  ProxyingRestrictedCookieManagerListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      base::WeakPtr<ProxyingRestrictedCookieManager> restricted_cookie_manager,
      mojo::PendingRemote<network::mojom::CookieChangeListener> client_listener)
      : url_(url),
        site_for_cookies_(site_for_cookies),
        restricted_cookie_manager_(restricted_cookie_manager),
        client_listener_(std::move(client_listener)) {}

  void OnCookieChange(const net::CookieChangeInfo& change) override {
    if (restricted_cookie_manager_ &&
        restricted_cookie_manager_->AllowCookies(url_, site_for_cookies_))
      client_listener_->OnCookieChange(change);
  }

 private:
  const GURL url_;
  const net::SiteForCookies site_for_cookies_;
  base::WeakPtr<ProxyingRestrictedCookieManager> restricted_cookie_manager_;
  mojo::Remote<network::mojom::CookieChangeListener> client_listener_;
};

// static
void ProxyingRestrictedCookieManager::CreateAndBind(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying_rcm,
    bool is_service_worker,
    int process_id,
    int frame_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyingRestrictedCookieManager::CreateAndBindOnIoThread,
                     std::move(underlying_rcm), is_service_worker, process_id,
                     frame_id, std::move(receiver)));
}

ProxyingRestrictedCookieManager::~ProxyingRestrictedCookieManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ProxyingRestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    network::mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->GetAllForUrl(
        url, site_for_cookies, top_frame_origin, std::move(options),
        std::move(callback));
  } else {
    std::move(callback).Run(std::vector<net::CookieWithAccessResult>());
  }
}

void ProxyingRestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    SetCanonicalCookieCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->SetCanonicalCookie(
        cookie, url, site_for_cookies, top_frame_origin, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void ProxyingRestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
    AddChangeListenerCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  mojo::PendingRemote<network::mojom::CookieChangeListener>
      proxy_listener_remote;
  auto proxy_listener =
      std::make_unique<ProxyingRestrictedCookieManagerListener>(
          url, site_for_cookies, weak_factory_.GetWeakPtr(),
          std::move(listener));

  mojo::MakeSelfOwnedReceiver(
      std::move(proxy_listener),
      proxy_listener_remote.InitWithNewPipeAndPassReceiver());

  underlying_restricted_cookie_manager_->AddChangeListener(
      url, site_for_cookies, top_frame_origin, std::move(proxy_listener_remote),
      std::move(callback));
}

void ProxyingRestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->SetCookieFromString(
        url, site_for_cookies, top_frame_origin, cookie, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void ProxyingRestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    GetCookiesStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AllowCookies(url, site_for_cookies)) {
    underlying_restricted_cookie_manager_->GetCookiesString(
        url, site_for_cookies, top_frame_origin, std::move(callback));
  } else {
    std::move(callback).Run("");
  }
}

void ProxyingRestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    CookiesEnabledForCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(AllowCookies(url, site_for_cookies));
}

ProxyingRestrictedCookieManager::ProxyingRestrictedCookieManager(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager>
        underlying_restricted_cookie_manager,
    bool is_service_worker,
    int process_id,
    int frame_id)
    : underlying_restricted_cookie_manager_(
          std::move(underlying_restricted_cookie_manager)) {
  is_service_worker_ = is_service_worker;
  process_id_ = process_id;
  frame_id_ = frame_id;
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void ProxyingRestrictedCookieManager::CreateAndBindOnIoThread(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying_rcm,
    bool is_service_worker,
    int process_id,
    int frame_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto wrapper = base::WrapUnique(new ProxyingRestrictedCookieManager(
      std::move(underlying_rcm), is_service_worker, process_id, frame_id));
  mojo::MakeSelfOwnedReceiver(std::move(wrapper), std::move(receiver));
}

bool ProxyingRestrictedCookieManager::AllowCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  return net_service::NetHelpers::IsAllowAcceptCookies();
}
