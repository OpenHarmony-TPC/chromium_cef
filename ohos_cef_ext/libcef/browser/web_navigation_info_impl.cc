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

#include "ohos_cef_ext/libcef/browser/web_navigation_info_impl.h"

#include "base/strings/string_util.h"
#include "ohos_cef_ext/include/cef_web_navigation_info.h"

namespace {

using CefWebNavigationInfoKeys::kAttemptType;
using CefWebNavigationInfoKeys::kDnsAddressList;
using CefWebNavigationInfoKeys::kDnsHost;
using CefWebNavigationInfoKeys::kDnsInfo;
using CefWebNavigationInfoKeys::kDnsResult;
using CefWebNavigationInfoKeys::kDnsTransition;
using CefWebNavigationInfoKeys::kRequestAttemptResult;
using CefWebNavigationInfoKeys::kRequestTraceId;
using CefWebNavigationInfoKeys::kSocketAddressList;
using CefWebNavigationInfoKeys::kSocketHost;
using CefWebNavigationInfoKeys::kSocketInfo;
using CefWebNavigationInfoKeys::kSocketResult;
using CefWebNavigationInfoKeys::kSslExpiredDate;
using CefWebNavigationInfoKeys::kSslHost;
using CefWebNavigationInfoKeys::kSslInfo;
using CefWebNavigationInfoKeys::kSslIsFatalCertError;
using CefWebNavigationInfoKeys::kSslIssuer;
using CefWebNavigationInfoKeys::kSslResult;
using CefWebNavigationInfoKeys::kWasFetchedViaProxy;

// Helper: Join string list with comma separator
std::string JoinWithComma(const std::vector<std::string>& strings) {
  return "[" + base::JoinString(strings, ",") + "]";
}

// Helper: Create DNS dictionary
CefRefPtr<CefDictionaryValue> CreateDnsInfoDict(const net::DnsInfo& dns_info) {
  auto dns_dict = CefDictionaryValue::Create();
  if (!dns_info.host.empty()) {
    dns_dict->SetString(kDnsHost, dns_info.host);
  }
  dns_dict->SetInt(kDnsResult, dns_info.result);
  dns_dict->SetInt(kDnsTransition, dns_info.dns_transition);
  return dns_dict;
}

// Helper: Create SSL dictionary
CefRefPtr<CefDictionaryValue> CreateSslInfoDict(
    const net::ArkWebSSLInfo& ssl_info) {
  auto ssl_dict = CefDictionaryValue::Create();
  if (!ssl_info.host.empty()) {
    ssl_dict->SetString(kSslHost, ssl_info.host);
  }
  if (!ssl_info.issuer.empty()) {
    ssl_dict->SetString(kSslIssuer, ssl_info.issuer);
  }
  if (!ssl_info.expired_date.empty()) {
    ssl_dict->SetString(kSslExpiredDate, ssl_info.expired_date);
  }
  ssl_dict->SetInt(kSslResult, ssl_info.result);
  ssl_dict->SetBool(kSslIsFatalCertError, ssl_info.is_fatal_cert_error);
  return ssl_dict;
}

// Helper: Create Socket dictionary
CefRefPtr<CefDictionaryValue> CreateSocketInfoDict(
    const net::SocketInfo& socket_info) {
  auto socket_dict = CefDictionaryValue::Create();
  if (!socket_info.host.empty()) {
    socket_dict->SetString(kSocketHost, socket_info.host);
  }
  socket_dict->SetInt(kSocketResult, socket_info.result);
  return socket_dict;
}

// Helper: Create RequestAttempt dictionary
CefRefPtr<CefDictionaryValue> CreateRequestAttemptDict(
    const net::RequestAttempt& attempt) {
  auto dict = CefDictionaryValue::Create();
  if (!attempt.request_trace_id.empty()) {
    dict->SetString(kRequestTraceId, attempt.request_trace_id);
  }
  dict->SetInt(kAttemptType, attempt.attempt_type);
  dict->SetInt(kRequestAttemptResult, attempt.request_result);
  dict->SetBool(kWasFetchedViaProxy, attempt.was_fetched_via_proxy);
  return dict;
}

// Helper: Set address list to dictionary
void SetAddressList(CefRefPtr<CefDictionaryValue> dict,
                    const std::string& key,
                    const std::vector<std::string>& address_list) {
  if (address_list.empty()) {
    return;
  }
  auto list = CefListValue::Create();
  list->SetSize(address_list.size());
  for (size_t i = 0; i < address_list.size(); ++i) {
    list->SetString(i, address_list[i]);
  }
  dict->SetList(key, list);
}

}  // namespace

CefWebNavigationInfoImpl::CefWebNavigationInfoImpl(
    const net::WebNavigationInfo& info) {
  // Copy basic fields from net::WebNavigationInfo to CEF member variables
  url_ = info.navigation_info.request_url;
  page_trace_id_ = info.page_trace_id;
  original_url_ = info.original_url;
  connection_type_ = info.connection_type;
  did_use_http_dns_ = info.did_use_http_dns;
  did_use_fallback_proxy_ = info.did_use_fallback_proxy;
  is_captive_portal_ = info.is_captive_portal;
  is_auto_reload_ = info.is_auto_reload;
  has_ignore_certificate_error_ = info.has_ignore_certificate_error;
  auto_reload_reason_ = static_cast<int>(info.auto_reload_reason);
  hw_code_ = info.hw_code;
  website_policy_ = info.threat_type;

  original_error_code_ = info.navigation_info.original_error_code;
  error_code_ = info.navigation_info.error_code;
  is_https_dns_enabled_ = info.navigation_info.can_use_secure_dns;
  is_fallback_proxy_enabled_ = info.navigation_info.is_fallback_proxy_enabled;

  // Copy DNS records as comma-separated strings
  insecure_dns_records_ = JoinWithComma(info.insecure_dns_records);
  secure_dns_records_ = JoinWithComma(info.secure_dns_records);

  request_trace_id_ = info.navigation_info.request_uuid;
  time_stamp_ = info.navigation_info.time_stamp;
  name_servers_ = info.navigation_info.dns_name_servers;
  local_ip_ = info.navigation_info.local_address;

  // Build request_attempts list from net::RequestAttempt
  request_attempts_ = CefListValue::Create();
  request_attempts_->SetSize(info.navigation_info.request_attempts.size());

  size_t index = 0;
  for (const auto& attempt : info.navigation_info.request_attempts) {
    auto attempt_dict = CreateRequestAttemptDict(attempt);

    // DNS info
    auto dns_dict = CreateDnsInfoDict(attempt.dns_info);
    SetAddressList(dns_dict, kDnsAddressList, attempt.dns_info.address_list);
    attempt_dict->SetDictionary(kDnsInfo, dns_dict);

    // SSL info
    auto ssl_dict = CreateSslInfoDict(attempt.ssl_info);
    attempt_dict->SetDictionary(kSslInfo, ssl_dict);

    // Socket info
    auto socket_dict = CreateSocketInfoDict(attempt.socket_info);
    SetAddressList(socket_dict, kSocketAddressList,
                   attempt.socket_info.address_list);
    attempt_dict->SetDictionary(kSocketInfo, socket_dict);

    request_attempts_->SetDictionary(index, attempt_dict);
    ++index;
  }
}

CefString CefWebNavigationInfoImpl::GetUrl() {
  return url_;
}

CefString CefWebNavigationInfoImpl::GetPageTraceId() {
  return page_trace_id_;
}

CefString CefWebNavigationInfoImpl::GetOriginalUrl() {
  return original_url_;
}

int CefWebNavigationInfoImpl::GetConnectionType() {
  return connection_type_;
}

bool CefWebNavigationInfoImpl::DidUseHttpDns() {
  return did_use_http_dns_;
}

bool CefWebNavigationInfoImpl::DidUseFallbackProxy() {
  return did_use_fallback_proxy_;
}

bool CefWebNavigationInfoImpl::IsCaptivePortal() {
  return is_captive_portal_;
}

bool CefWebNavigationInfoImpl::IsAutoReload() {
  return is_auto_reload_;
}

bool CefWebNavigationInfoImpl::HasIgnoreCertificateError() {
  return has_ignore_certificate_error_;
}

int CefWebNavigationInfoImpl::GetAutoReloadReason() {
  return auto_reload_reason_;
}

int CefWebNavigationInfoImpl::GetOriginalErrorCode() {
  return original_error_code_;
}

int CefWebNavigationInfoImpl::GetErrorCode() {
  return error_code_;
}

bool CefWebNavigationInfoImpl::IsHttpsDnsEnabled() {
  return is_https_dns_enabled_;
}

bool CefWebNavigationInfoImpl::IsFallbackProxyEnabled() {
  return is_fallback_proxy_enabled_;
}

int CefWebNavigationInfoImpl::GetHwCode() {
  return hw_code_;
}

CefString CefWebNavigationInfoImpl::GetInsecureDnsRecords() {
  return insecure_dns_records_;
}

CefString CefWebNavigationInfoImpl::GetSecureDnsRecords() {
  return secure_dns_records_;
}

CefString CefWebNavigationInfoImpl::GetRequestTraceId() {
  return request_trace_id_;
}

CefString CefWebNavigationInfoImpl::GetTimeStamp() {
  return time_stamp_;
}

CefString CefWebNavigationInfoImpl::GetNameServers() {
  return name_servers_;
}

CefString CefWebNavigationInfoImpl::GetLocalIp() {
  return local_ip_;
}

int CefWebNavigationInfoImpl::GetWebsitePolicy() {
  return website_policy_;
}

CefRefPtr<CefListValue> CefWebNavigationInfoImpl::GetRequestAttempts() {
  return request_attempts_;
}
