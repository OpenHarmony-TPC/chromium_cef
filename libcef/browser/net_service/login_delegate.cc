// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/net_service/login_delegate.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "cef/libcef/browser/browser_host_base.h"
// #include "libcef/browser/net_database/cef_data_base_impl.h"
#include "cef/libcef/browser/net_service/browser_urlrequest_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ARKWEB_CA)
#include "third_party/bounds_checking_function/include/securec.h"
#endif

namespace net_service {

namespace {
const int USERNAME_PASSWORD_VECTOR_NUM = 2;

class AuthCallbackImpl : public CefAuthCallback {
 public:
  explicit AuthCallbackImpl(base::WeakPtr<LoginDelegate> delegate
#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
                            ,
                            const CefString& host,
                            const CefString& realm
#endif
                            )
      : delegate_(delegate),
#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
        host_(host),
        realm_(realm),
#endif
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  }

  AuthCallbackImpl(const AuthCallbackImpl&) = delete;
  AuthCallbackImpl& operator=(const AuthCallbackImpl&) = delete;

  ~AuthCallbackImpl() override {
    if (delegate_.MaybeValid()) {
      // If |delegate_| isn't valid this will be a no-op.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&LoginDelegate::Cancel, delegate_));
    }
  }

  void Continue(const CefString& username, const CefString& password) override {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&AuthCallbackImpl::Continue, this, username,
                                    password));
      return;
    }

    if (delegate_) {
      delegate_->Continue(username, password);
      delegate_ = nullptr;
    }
  }

  void Cancel() override {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&AuthCallbackImpl::Cancel, this));
      return;
    }

    if (delegate_) {
      delegate_->Cancel();
      delegate_ = nullptr;
    }
  }

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  bool IsHttpAuthInfoSaved() override {
    constexpr int32_t MAX_PWD_LENGTH = 256;
    auto dataBase = CefDataBase::GetGlobalDataBase();
    if (dataBase == nullptr) {
      LOG(ERROR) << "IsHttpAuthInfoSaved dataBase is empty";
      return false;
    }
    if (!dataBase->ExistHttpAuthCredentials()) {
      LOG(WARNING) << "IsHttpAuthInfoSaved dataBase is existHttpAuth";
      return false;
    }
    CefString username;
    char password[MAX_PWD_LENGTH + 1] = {0};
    dataBase->GetHttpAuthCredentials(host_, realm_, username, password,
                                     MAX_PWD_LENGTH + 1);
    if (username.empty() || strlen(password) == 0) {
      if (memset_s(password, MAX_PWD_LENGTH + 1, 0, MAX_PWD_LENGTH + 1) !=
          EOK) {
        return false;
      }
      LOG(WARNING) << "IsHttpAuthInfoSaved name or password is empty";
      return false;
    }
    CefString passwordCef(password, strlen(password));
    if (memset_s(password, MAX_PWD_LENGTH + 1, 0, MAX_PWD_LENGTH + 1) != EOK) {
      return false;
    }
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&AuthCallbackImpl::Continue, this, username,
                                    passwordCef));
      passwordCef.MemsetToZero();
      LOG(INFO) << "IsHttpAuthInfoSaved continue";
      return true;
    }
    if (delegate_) {
      delegate_->Continue(username, passwordCef);
      delegate_ = nullptr;
      passwordCef.MemsetToZero();
      LOG(INFO) << "IsHttpAuthInfoSaved login byself";
      return true;
    }
    passwordCef.MemsetToZero();
    LOG(WARNING) << "IsHttpAuthInfoSaved is not found";
    return false;
  }
#endif

 private:
  base::WeakPtr<LoginDelegate> delegate_;
#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
  CefString host_;
  CefString realm_;
#endif
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  IMPLEMENT_REFCOUNTING(AuthCallbackImpl);
};

void RunCallbackOnIOThread(
    CefRefPtr<CefBrowserHostBase> browser,
    std::optional<CefBrowserURLRequest::RequestInfo> url_request_info,
    const net::AuthChallengeInfo& auth_info,
    const GURL& origin_url,
    CefRefPtr<AuthCallbackImpl> callback_impl) {
  CEF_REQUIRE_IOT();

  // TODO(network): After the old network code path is deleted move this
  // callback to the BrowserURLRequest's context thread.
  if (url_request_info) {
    bool handled = url_request_info->second->GetAuthCredentials(
        auth_info.is_proxy, auth_info.challenger.host(),
        auth_info.challenger.port(), auth_info.realm, auth_info.scheme,
        callback_impl.get());
    if (handled) {
      // The user will execute the callback, or the request will be canceled on
      // AuthCallbackImpl destruction.
      return;
    }
  }

  if (browser) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler) {
        bool handled = handler->GetAuthCredentials(
            browser.get(), origin_url.spec(), auth_info.is_proxy,
            auth_info.challenger.host(), auth_info.challenger.port(),
            auth_info.realm, auth_info.scheme, callback_impl.get());
        if (handled) {
          // The user will execute the callback, or the request will be canceled
          // on AuthCallbackImpl destruction.
          return;
        }
      }
    }
  }

  callback_impl->Cancel();
}
}  // namespace

LoginDelegate::LoginDelegate(const net::AuthChallengeInfo& auth_info,
                             content::WebContents* web_contents,
                             const content::GlobalRequestID& request_id,
                             const GURL& origin_url,
                             LoginAuthRequiredCallback callback)
    : callback_(std::move(callback)), weak_ptr_factory_(this) {
  CEF_REQUIRE_UIT();

  // May be nullptr for requests originating from CefURLRequest.
  CefRefPtr<CefBrowserHostBase> browser;
  if (web_contents) {
    browser = CefBrowserHostBase::GetBrowserForContents(web_contents);
  }

  // |callback| needs to be executed asynchronously.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&LoginDelegate::Start,
                                        weak_ptr_factory_.GetWeakPtr(), browser,
                                        auth_info, request_id, origin_url));
}

void LoginDelegate::Continue(const CefString& username,
                             const CefString& password) {
  CEF_REQUIRE_UIT();
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        net::AuthCredentials(username.ToString16(), password.ToString16()));
  }
}

void LoginDelegate::Cancel() {
  CEF_REQUIRE_UIT();
  if (!callback_.is_null()) {
    std::move(callback_).Run(std::nullopt);
  }
}

void LoginDelegate::Start(CefRefPtr<CefBrowserHostBase> browser,
                          const net::AuthChallengeInfo& auth_info,
                          const content::GlobalRequestID& request_id,
                          const GURL& origin_url) {
  CEF_REQUIRE_UIT();

  auto url_request_info = CefBrowserURLRequest::FromRequestID(request_id);

  if (browser || url_request_info) {
    // AuthCallbackImpl is bound to the current thread.
    CefRefPtr<AuthCallbackImpl> callbackImpl =
        new AuthCallbackImpl(weak_ptr_factory_.GetWeakPtr(),
#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
                             auth_info.challenger.host(), auth_info.realm
#endif
        );

    // Execute callbacks on the IO thread to maintain the "old"
    // network_delegate callback behaviour.
    CEF_POST_TASK(CEF_IOT, base::BindOnce(&RunCallbackOnIOThread, browser,
                                          url_request_info, auth_info,
                                          origin_url, callbackImpl));
  } else {
    Cancel();
  }
}

}  // namespace net_service
                           