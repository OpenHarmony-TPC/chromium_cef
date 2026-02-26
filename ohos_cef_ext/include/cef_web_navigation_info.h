/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef CEF_OHOS_CEF_EXT_INCLUDE_CEF_WEB_NAVIGATION_INFO_H_
#define CEF_OHOS_CEF_EXT_INCLUDE_CEF_WEB_NAVIGATION_INFO_H_

#include <string>

#include "include/cef_base.h"
#include "include/cef_values.h"

///
/// Structure representing web DNS information.
///
class CefWebDnsInfo {
 public:
  CefWebDnsInfo() = default;

  std::string host;
  int result = 0;
  std::string address_list;
  int dns_transition = 0;
  std::string dns_record_truncation;
};

///
/// Structure representing web socket information.
///
class CefWebSocketInfo {
 public:
  CefWebSocketInfo() = default;

  std::string host;
  int result = 0;
  std::string address_list;
};

///
/// Structure representing web SSL information.
///
class CefWebSSLInfo {
 public:
  CefWebSSLInfo() = default;

  std::string host;
  int result = 0;
  std::string issuer;
  bool is_fatal_error = false;
  std::string expired_date;
};

///
/// Structure representing web request attempt information.
///
class CefWebRequestAttempt {
 public:
  CefWebRequestAttempt() = default;

  std::string request_trace_id;
  int attempt_type = 0;
  int request_attempt_result = 0;
  bool was_fetched_via_proxy = false;
  CefWebDnsInfo dns_info;
  CefWebSocketInfo socket_info;
  CefWebSSLInfo ssl_info;
};

///
/// Class representing web navigation information.
///
/*--cef(source=library)--*/
class CefWebNavigationInfo : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the request URL.
  ///
  /*--cef()--*/
  virtual CefString GetUrl() = 0;

  ///
  /// Returns the page trace ID.
  ///
  /*--cef()--*/
  virtual CefString GetPageTraceId() = 0;

  ///
  /// Returns the original URL.
  ///
  /*--cef()--*/
  virtual CefString GetOriginalUrl() = 0;

  ///
  /// Returns the connection type.
  ///
  /*--cef()--*/
  virtual int GetConnectionType() = 0;

  ///
  /// Returns true if HTTP DNS was used.
  ///
  /*--cef()--*/
  virtual bool DidUseHttpDns() = 0;

  ///
  /// Returns true if fallback proxy was used.
  ///
  /*--cef()--*/
  virtual bool DidUseFallbackProxy() = 0;

  ///
  /// Returns true if this is a captive portal.
  ///
  /*--cef()--*/
  virtual bool IsCaptivePortal() = 0;

  ///
  /// Returns true if this is an auto reload.
  ///
  /*--cef()--*/
  virtual bool IsAutoReload() = 0;

  ///
  /// Returns true if has ignore certificate error.
  ///
  /*--cef()--*/
  virtual bool HasIgnoreCertificateError() = 0;

  ///
  /// Returns the auto reload reason.
  ///
  /*--cef()--*/
  virtual int GetAutoReloadReason() = 0;

  ///
  /// Returns the original error code.
  ///
  /*--cef()--*/
  virtual int GetOriginalErrorCode() = 0;

  ///
  /// Returns the error code.
  ///
  /*--cef()--*/
  virtual int GetErrorCode() = 0;

  ///
  /// Returns true if HTTPS DNS is enabled.
  ///
  /*--cef()--*/
  virtual bool IsHttpsDnsEnabled() = 0;

  ///
  /// Returns true if fallback proxy is enabled.
  ///
  /*--cef()--*/
  virtual bool IsFallbackProxyEnabled() = 0;

  ///
  /// Returns the HW code.
  ///
  /*--cef()--*/
  virtual int GetHwCode() = 0;

  ///
  /// Returns the insecure DNS records.
  ///
  /*--cef()--*/
  virtual CefString GetInsecureDnsRecords() = 0;

  ///
  /// Returns the secure DNS records.
  ///
  /*--cef()--*/
  virtual CefString GetSecureDnsRecords() = 0;

  ///
  /// Returns the request trace ID.
  ///
  /*--cef()--*/
  virtual CefString GetRequestTraceId() = 0;

  ///
  /// Returns the timestamp.
  ///
  /*--cef()--*/
  virtual CefString GetTimeStamp() = 0;

  ///
  /// Returns the name servers.
  ///
  /*--cef()--*/
  virtual CefString GetNameServers() = 0;

  ///
  /// Returns the local IP address.
  ///
  /*--cef()--*/
  virtual CefString GetLocalIp() = 0;

  ///
  /// Returns the website policy.
  ///
  /*--cef()--*/
  virtual int GetWebsitePolicy() = 0;

  ///
  /// Returns the request attempts as a list of dictionaries.
  /// Each dictionary contains: request_trace_id, attempt_type,
  /// request_attempt_result, was_fetched_via_proxy, dns_info,
  /// socket_info, ssl_info.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefListValue> GetRequestAttempts() = 0;
};

///
/// Constants for dictionary keys in WebNavigationInfo serialization.
/// These keys are used when converting between structures and CefValue objects.
///
namespace CefWebNavigationInfoKeys {

// RequestAttempt dictionary keys
inline constexpr const char* kRequestTraceId = "request_trace_id";
inline constexpr const char* kAttemptType = "attempt_type";
inline constexpr const char* kRequestAttemptResult = "request_attempt_result";
inline constexpr const char* kWasFetchedViaProxy = "was_fetched_via_proxy";

// Nested info structure keys
inline constexpr const char* kDnsInfo = "dns_info";
inline constexpr const char* kSocketInfo = "socket_info";
inline constexpr const char* kSslInfo = "ssl_info";

// WebDnsInfo field keys
inline constexpr const char* kDnsHost = "host";
inline constexpr const char* kDnsResult = "result";
inline constexpr const char* kDnsAddressList = "address_list";
inline constexpr const char* kDnsTransition = "dns_transition";
inline constexpr const char* kDnsRecordTruncation = "dns_record_truncation";

// WebSocketInfo field keys
inline constexpr const char* kSocketHost = "host";
inline constexpr const char* kSocketResult = "result";
inline constexpr const char* kSocketAddressList = "address_list";

// WebSSLInfo field keys
inline constexpr const char* kSslHost = "host";
inline constexpr const char* kSslResult = "result";
inline constexpr const char* kSslIssuer = "issuer";
inline constexpr const char* kSslIsFatalCertError = "is_fatal_cert_error";
inline constexpr const char* kSslExpiredDate = "expired_date";

}  // namespace CefWebNavigationInfoKeys

#endif  // CEF_OHOS_CEF_EXT_INCLUDE_CEF_WEB_NAVIGATION_INFO_H_
