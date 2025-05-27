// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/ohos_cef_ext/libcef/browser/ark_web_certificate_query.h"

#include "cef/include/cef_callback.h"
#include "cef/include/cef_request_handler.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/ssl_info_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "content/public/browser/web_contents.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace certificate_query {

namespace {

class CefAllowCertificateErrorCallbackImpl : public CefCallback {
 public:
  using CallbackType = CertificateErrorCallback;

  explicit CefAllowCertificateErrorCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  CefAllowCertificateErrorCallbackImpl(
      const CefAllowCertificateErrorCallbackImpl&) = delete;
  CefAllowCertificateErrorCallbackImpl& operator=(
      const CefAllowCertificateErrorCallbackImpl&) = delete;

  ~CefAllowCertificateErrorCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), false);
      } else {
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefAllowCertificateErrorCallbackImpl::RunNow,
                           std::move(callback_), false));
      }
    }
  }

  void Continue() override { ContinueNow(true); }

  void Cancel() override { ContinueNow(false); }

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  void ContinueNow(bool allow) {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), allow);
      }
    } else {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefAllowCertificateErrorCallbackImpl::ContinueNow,
                         this, allow));
    }
  }

  static void RunNow(CallbackType callback, bool allow) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(
        allow ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
              : content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
};

}  // namespace

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
bool OnCertificateError(
    content::WebContents* webContents,
    int certError,
    CefRefPtr<CefSSLInfo> sslInfo,
    const GURL& requestUrl,
    bool isMainFrameRequest,
    bool strictEnforcement,
    const GURL& originUrl,
    const std::string& referrer,
    CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl,
    bool onlyMainError) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(webContents);
  if (!browser) {
    return false;
  }
  auto client = browser->GetClient();
  if (!client) {
    return false;
  }
  auto handler = client->GetRequestHandler();
  if (!handler) {
    return false;
  }
  if (!handler->AsCefRequestHandlerExt()) {
    return false;
  }
  bool proceed;
  if (onlyMainError) {
    proceed = handler->OnCertificateError(
        browser.get(), static_cast<cef_errorcode_t>(certError),
        requestUrl.spec(), sslInfo, callbackImpl.get());
  } else {
    proceed = handler->AsCefRequestHandlerExt()->OnAllCertificateError(
        browser.get(), static_cast<cef_errorcode_t>(certError),
        requestUrl.spec(), originUrl.spec(), referrer, isMainFrameRequest,
        strictEnforcement, sslInfo, callbackImpl.get());
  }
  if (proceed) {
    return true;
  }

  return false;
}

CertificateErrorCallback AllowAllCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    const GURL& origin_url,
    const std::string& referrer,
    CertificateErrorCallback callback,
    bool default_disallow) {
  CEF_REQUIRE_UIT();

  bool result;
  CefRefPtr<CefSSLInfo> sslInfo(new CefSSLInfoImpl(ssl_info));
  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
      new CefAllowCertificateErrorCallbackImpl(std::move(callback)));

  result = OnCertificateError(web_contents, cert_error, sslInfo, request_url,
                              is_main_frame_request, strict_enforcement,
                              origin_url, referrer, callbackImpl, false);
  if (!result) {
    callback = callbackImpl->Disconnect();
    LOG_IF(ERROR, callback.is_null())
        << "Should return true from OnCertificateError when executing the "
           "callback";
  }

  if (!is_main_frame_request) {
    if (!callback.is_null() && default_disallow) {
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
      return base::NullCallback();
    } else {
      return callback;
    }
  }

  CefRefPtr<CefAllowCertificateErrorCallbackImpl> mainCallbackImpl(
      new CefAllowCertificateErrorCallbackImpl(std::move(callback)));
  result = OnCertificateError(web_contents, cert_error, sslInfo, request_url,
                              is_main_frame_request, strict_enforcement,
                              origin_url, referrer, mainCallbackImpl, true);
  if (!result) {
    callback = mainCallbackImpl->Disconnect();
    LOG_IF(ERROR, callback.is_null())
        << "Should return true from OnCertificateError when executing the "
           "callback";
  }
  if (!callback.is_null() && default_disallow) {
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  return callback;
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_LOAD)

}  // namespace certificate_query
