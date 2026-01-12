/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "fallback_proxy_config.h"

#include <string>

#include "arkweb/chromium_ext/url/ohos/log_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/base/proxy_string_util.h"

namespace fallback_proxy {
FallbackProxyConfig::FallbackProxyConfig() : enabled_(false) {}

FallbackProxyConfig::~FallbackProxyConfig() {}

void FallbackProxyConfig::WriteCurrentConfigToLog() {
  LOG(INFO) << "Fallback proxy config data : "
    << " using_proxy_error_codes_size = "
    << using_proxy_error_codes_.size()
    << "; hw_codes_size = " << safe_browsing_hw_codes_.size()
    << "; malicious_types_size = "
    << safe_browsing_malicious_types_.size()
    << "; proxy_tunnel_timeout = " << proxy_tunnel_timeout_
    << "; proxy_connect_timeout = " << proxy_connect_timeout_
    << "; malicious_url_check_wait_time = "
    << malicious_url_check_wait_time_;
}

#if BUILDFLAG(IS_ARKWEB_EXT)
void FallbackProxyConfig::SetFallbackProxyConfigData(
    const ohos_cloud_control::FallbackProxyConfigData& config_data) {
  config_enabled_ = config_data.is_fallback_proxy_enabled;
  std::vector<int> using_proxy_error_codes = config_data.error_code_list;
  std::vector<int> safe_browsing_hw_codes =
      config_data.safe_browsing_hw_code_list;
  std::vector<int> safe_browsing_malicious_types =
      config_data.safe_browsing_malicious_type_list;
  int restart_proxy_internal = config_data.restart_proxy_internal;
  int proxy_tunnel_timeout = config_data.proxy_tunnel_timeout;
  int proxy_connect_timeout = config_data.proxy_connect_timeout;
  int malicious_url_check_wait_time = config_data.malicious_url_check_wait_time;
  std::string proxy_server = config_data.fallback_server;

  UpdateFallbackProxyServerConfig(config_enabled_, proxy_server);
  if (restart_proxy_internal != restart_proxy_internal_) {
    LOG(INFO) << "Fallback proxy config data is changed"
              << ", restart_proxy_internal_before " << restart_proxy_internal_
              << ", restart_proxy_internal_after " << restart_proxy_internal;
    restart_proxy_internal_ = restart_proxy_internal;
  }
  if (using_proxy_error_codes != using_proxy_error_codes_ ||
      safe_browsing_hw_codes != safe_browsing_hw_codes_ ||
      safe_browsing_malicious_types != safe_browsing_malicious_types_ ||
      proxy_tunnel_timeout != proxy_tunnel_timeout_ ||
      proxy_connect_timeout != proxy_connect_timeout_ ||
      malicious_url_check_wait_time != malicious_url_check_wait_time_) {
    using_proxy_error_codes_ = using_proxy_error_codes;
    safe_browsing_hw_codes_ = safe_browsing_hw_codes;
    safe_browsing_malicious_types_ = safe_browsing_malicious_types;
    proxy_tunnel_timeout_ = proxy_tunnel_timeout;
    proxy_connect_timeout_ = proxy_connect_timeout;
    malicious_url_check_wait_time_ = malicious_url_check_wait_time;
    LOG(INFO) << "Fallback proxy config data is changed";
    WriteCurrentConfigToLog();
    if (proxy_info_updated_callback_) {
      proxy_info_updated_callback_.Run();
    }
  }
}
#endif

bool FallbackProxyConfig::HasProxyServer(const net::ProxyServer& proxy_server) {
  if (std::count(proxy_servers_.begin(), proxy_servers_.end(), proxy_server)) {
    return true;
  }
  return false;
}

std::vector<net::ProxyServer> FallbackProxyConfig::GetProxyServers() const {
  if (!enabled_) {
    return std::vector<net::ProxyServer>();
  }

  return proxy_servers_;
}

const net::ProxyBypassRules& FallbackProxyConfig::GetBypassRules() {
  return bypass_rules_;
}

const std::vector<int>& FallbackProxyConfig::GetUsingProxyErrorCodes() const {
  return using_proxy_error_codes_;
}

const std::vector<int>& FallbackProxyConfig::GetSafeBrowsingHwCode() const {
  return safe_browsing_hw_codes_;
}

const std::vector<int>& FallbackProxyConfig::GetSafeBrowsingMaliciousType()
    const {
  return safe_browsing_malicious_types_;
}

int FallbackProxyConfig::GetRestartProxyInternal() const {
  return restart_proxy_internal_;
}

int FallbackProxyConfig::GetProxyTunnelTimeout() const {
  return proxy_tunnel_timeout_;
}

int FallbackProxyConfig::GetProxyConnectTimeout() const {
  return proxy_connect_timeout_;
}

int FallbackProxyConfig::GetMaliciousUrlCheckWaitTime() const {
  return malicious_url_check_wait_time_;
}

const std::string& FallbackProxyConfig::GetProxyToken() const {
  return auth_token_;
}

void FallbackProxyConfig::UpdateProxyToken(const std::string& token,
                                           const std::string& token_info) {
  if (auth_token_ != token) {
    LOG(INFO) << "Fallback proxy config proxy token is changed. old_token="
              << auth_token_ << " token=" << token;
    auth_token_ = token;
    if (proxy_token_updated_callback_) {
      proxy_token_updated_callback_.Run();
    }
  }
}

void FallbackProxyConfig::InitializeEnableParamsForTesting(
    const std::string& proxy_url,
    const std::string& header) {
  UpdateFallbackProxyServerConfig(true, proxy_url);
  InitializeHeadersForTesting(header);
  LOG(DEBUG) << "Fallback proxy config::InitializeEnableParamsForTesting";
}

void FallbackProxyConfig::InitializeDisableParamsForTesting() {
  enabled_ = false;
  proxy_servers_.clear();

  LOG(DEBUG) << "Fallback proxy config::InitializeDisableParamsForTesting";
  UpdateProxyServer();
}

void FallbackProxyConfig::SetBypassRulesForTesting(const std::string& pattern) {
  bypass_rules_.ParseFromString(pattern);
}

void FallbackProxyConfig::UpdateProxyServer() {
  if (proxy_server_updated_callback_) {
    proxy_server_updated_callback_.Run();
  }
}

void FallbackProxyConfig::DisableFallbackProxy() {
  enabled_ = false;
  proxy_servers_.clear();
  UpdateProxyServer();
}

void FallbackProxyConfig::UpdateFallbackProxyServerConfig(
    bool config_enabled,
    const std::string& config_proxy_server) {
  GURL proxy_url(config_proxy_server);
  if (!config_enabled || !proxy_url.is_valid() ||
      proxy_url.scheme() != url::kHttpsScheme) {
    if (enabled_) {
      LOG(INFO)
          << "Fallback proxy config will disable proxy for config_enabled false"
          << " or proxy url invalid or proxy url scheme not https"
          << ", enabled_before " << enabled_ << ", config_enabled "
          << config_enabled << ", config_proxy_server "
          << url::LogUtils::ConvertUrlWithMask(config_proxy_server);
      DisableFallbackProxy();
    }
    return;
  }

  net::ProxyServer proxy_server(net::GetSchemeFromUriScheme(proxy_url.scheme()),
                                net::HostPortPair::FromURL(proxy_url));
  if (!proxy_server.is_valid()) {
    if (enabled_) {
      LOG(INFO)
          << "Fallback proxy config will disable proxy for proxy server invalid"
          << ", enabled_before " << enabled_ << ", config_enabled "
          << config_enabled << ", config_proxy_server "
          << url::LogUtils::ConvertUrlWithMask(config_proxy_server);
      DisableFallbackProxy();
    }
    return;
  }

  LOG(INFO) << "Fallback proxy config will enable proxy, for config_enabled "
                "true and proxy server valid"
             << ", enabled_before " << enabled_ << ", config_enabled "
             << config_enabled << ", config_proxy_server "
             << url::LogUtils::ConvertUrlWithMask(config_proxy_server);
  enabled_ = config_enabled;
  std::vector<net::ProxyServer> proxy_servers;
  proxy_servers.push_back(proxy_server);
  if (proxy_servers != proxy_servers_) {
    proxy_servers_ = proxy_servers;
    UpdateProxyServer();
  }
}

void FallbackProxyConfig::InitializeHeadersForTesting(
    const std::string& header) {
  std::string token_info;
  UpdateProxyToken(header, token_info);
}

}  // namespace fallback_proxy
