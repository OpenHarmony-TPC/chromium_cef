// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/certificate_query.h"

#include "include/cef_request_handler.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

#ifdef OHOS_LOGGER_REPORT
#include "base/base_switches.h"
#include "base/command_line.h"
#include "url/ohos/log_utils.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#endif

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

  ~CefAllowCertificateErrorCallbackImpl() {
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

CertificateErrorCallback AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    CertificateErrorCallback callback,
    bool default_disallow) {
  CEF_REQUIRE_UIT();

  if (!is_main_frame_request) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  if (auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetRequestHandler()) {
        CefRefPtr<CefSSLInfo> sslInfo(new CefSSLInfoImpl(ssl_info));
        CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
            new CefAllowCertificateErrorCallbackImpl(std::move(callback)));

        bool proceed = handler->OnCertificateError(
            browser.get(), static_cast<cef_errorcode_t>(cert_error),
            request_url.spec(), sslInfo, callbackImpl.get());
        if (!proceed) {
          callback = callbackImpl->Disconnect();
          LOG_IF(ERROR, callback.is_null())
              << "Should return true from OnCertificateError when executing "
                 "the callback";
        }
      }
    }
  }

  if (!callback.is_null() && default_disallow) {
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  return callback;
}

#ifdef OHOS_NETWORK_LOAD
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

  bool proceed;
  if (onlyMainError) {
    proceed = handler->OnCertificateError(browser.get(), static_cast<cef_errorcode_t>(certError), requestUrl.spec(),
                                          sslInfo, callbackImpl.get());
  } else {
    proceed = handler->OnAllCertificateError(
      browser.get(), static_cast<cef_errorcode_t>(certError),
      requestUrl.spec(), originUrl.spec(), referrer, isMainFrameRequest,
      strictEnforcement, sslInfo, callbackImpl.get());
  }
  if (proceed) {
    return true;
  }

  return false;
}
#endif

#ifdef OHOS_LOGGER_REPORT
int IsSslCertErrorFatal(int cert_error) {
  switch (cert_error) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_CERT_VALIDITY_TOO_LONG:
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
    case net::ERR_CERT_SYMANTEC_LEGACY:
    case net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
    case net::ERR_SSL_OBSOLETE_VERSION_OR_CIPHER:
      return false;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return true;
    default:
      NOTREACHED();
      return true;
  }
}
#endif

#ifdef OHOS_NETWORK_LOAD
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

#ifdef OHOS_LOGGER_REPORT
  if ( base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableLoggerReport )) {
    bool is_fatal_error = false;
    is_fatal_error = IsSslCertErrorFatal(cert_error) || is_fatal_error;
    std::string err_msg = 
        "a ssl error occurred, err_code: " + std::to_string(cert_error) +
        ", is_fatal_error: " + std::to_string(is_fatal_error) +
        ", origin: " + origin_url.spec() + ", refer: " + referrer;
    int32_t usage_scenario = web_contents->GetOrCreateWebPreferences().usage_scenario;
    LOG(URL) <<  "event_message: "  <<  err_msg  <<  ", url: "
            << url::LogUtils::ConvertUrl(origin_url.spec(), usage_scenario);
  }
#endif
  bool result;
  CefRefPtr<CefSSLInfo> sslInfo(new CefSSLInfoImpl(ssl_info));
  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
            new CefAllowCertificateErrorCallbackImpl(std::move(callback)));

  result = OnCertificateError(web_contents, cert_error, sslInfo, request_url, is_main_frame_request,
                     strict_enforcement, origin_url, referrer, callbackImpl, false);
  if (!result) {
    callback = callbackImpl->Disconnect();
    LOG_IF(ERROR, callback.is_null()) << "Should return true from OnCertificateError when executing the callback";
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
  result = OnCertificateError(web_contents, cert_error, sslInfo, request_url, is_main_frame_request,
                     strict_enforcement, origin_url, referrer, mainCallbackImpl, true);
  if (!result) {
    callback = mainCallbackImpl->Disconnect();
    LOG_IF(ERROR, callback.is_null()) << "Should return true from OnCertificateError when executing the callback";
  }
  if (!callback.is_null() && default_disallow) {
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  return callback;
}
#endif

}  // namespace certificate_query