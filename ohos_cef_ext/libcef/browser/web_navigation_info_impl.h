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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_WEB_NAVIGATION_INFO_IMPL_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_WEB_NAVIGATION_INFO_IMPL_H_
#pragma once

#include "arkweb/chromium_ext/net/base/request_attempt.h"
#include "arkweb/chromium_ext/net/base/web_navigation_info.h"
#include "include/cef_base.h"
#include "include/cef_values.h"
#include "include/cef_web_navigation_info.h"
#include "libcef/common/value_base.h"

// CefWebNavigationInfo implementation
class CefWebNavigationInfoImpl : public CefWebNavigationInfo {
 public:
  explicit CefWebNavigationInfoImpl(const net::WebNavigationInfo& info);

  CefWebNavigationInfoImpl(const CefWebNavigationInfoImpl&) = delete;
  CefWebNavigationInfoImpl& operator=(const CefWebNavigationInfoImpl&) = delete;

  // CefWebNavigationInfo methods.
  CefString GetUrl() override;
  CefString GetPageTraceId() override;
  CefString GetOriginalUrl() override;
  int GetConnectionType() override;
  bool DidUseHttpDns() override;
  bool DidUseFallbackProxy() override;
  bool IsCaptivePortal() override;
  bool IsAutoReload() override;
  bool HasIgnoreCertificateError() override;
  int GetAutoReloadReason() override;
  int GetOriginalErrorCode() override;
  int GetErrorCode() override;
  bool IsHttpsDnsEnabled() override;
  bool IsFallbackProxyEnabled() override;
  int GetHwCode() override;
  CefString GetInsecureDnsRecords() override;
  CefString GetSecureDnsRecords() override;
  CefString GetRequestTraceId() override;
  CefString GetTimeStamp() override;
  CefString GetNameServers() override;
  CefString GetLocalIp() override;
  int GetWebsitePolicy() override;
  CefRefPtr<CefListValue> GetRequestAttempts() override;

 private:
  std::string url_;
  std::string page_trace_id_;
  std::string original_url_;
  int connection_type_ = 0;
  bool did_use_http_dns_ = false;
  bool did_use_fallback_proxy_ = false;
  bool is_captive_portal_ = false;
  bool is_auto_reload_ = false;
  bool has_ignore_certificate_error_ = false;
  int auto_reload_reason_ = 0;
  int original_error_code_ = 0;
  int error_code_ = 0;
  bool is_https_dns_enabled_ = false;
  bool is_fallback_proxy_enabled_ = false;
  int hw_code_ = 0;
  int website_policy_ = 0;
  std::string insecure_dns_records_;
  std::string secure_dns_records_;
  std::string request_trace_id_;
  std::string time_stamp_;
  std::string name_servers_;
  std::string local_ip_;
  CefRefPtr<CefListValue> request_attempts_;

  IMPLEMENT_REFCOUNTING(CefWebNavigationInfoImpl);
};

#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_WEB_NAVIGATION_INFO_IMPL_H_
