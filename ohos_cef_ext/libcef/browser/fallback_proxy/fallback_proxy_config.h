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

#ifndef CEF_LIBCEF_BROWSER_FALLBACK_PROXY_CONFIG_H
#define CEF_LIBCEF_BROWSER_FALLBACK_PROXY_CONFIG_H

#include <cstdint>

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_config.h"
#include "ohos_nweb_ex/overrides/cef/libcef/browser/browser_config/browser_cloud_control_global_config.h"

namespace fallback_proxy {

class FallbackProxyConfig {
 public:
  FallbackProxyConfig();
  ~FallbackProxyConfig();

  // 存放通知FallbackProxyService云控关于代理开关或代理服务器变更的回调函数
  void SetProxyServerUpdatedCallback(
      base::RepeatingClosure proxy_server_updated_callback) {
    proxy_server_updated_callback_ = proxy_server_updated_callback;
  }

  // 存放通知FallbackProxyService云控关于代理信息变更的回调函数
  void SetProxyInfoUpdatedCallback(
      base::RepeatingClosure proxy_info_updated_callback) {
    proxy_info_updated_callback_ = proxy_info_updated_callback;
  }

  // 存放通知FallbackProxyService头部信息变更的回调函数
  void SetProxyTokenUpdatedCallback(
      base::RepeatingClosure proxy_token_updated_callback) {
    proxy_token_updated_callback_ = proxy_token_updated_callback;
  }

  // 云控更新整个fallbackProxyConfigData信息
  void SetFallbackProxyConfigData(
      const ohos_cloud_control::FallbackProxyConfigData& config_data);

  bool ConfigEnabled() { return config_enabled_; }
  // 判断proxy_server是否是配置的server
  bool HasProxyServer(const net::ProxyServer& proxy_server);
  // 获取云控配置的proxy_servers
  std::vector<net::ProxyServer> GetProxyServers() const;
  // 获取bypass_rules，默认为空，测试用例执行时，可能非空
  const net::ProxyBypassRules& GetBypassRules();
  // 获取云控配置的错误码列表
  const std::vector<int>& GetUsingProxyErrorCodes() const;
  // 获取云控配置的HwCode列表
  const std::vector<int>& GetSafeBrowsingHwCode() const;
  // 获取云控配置的MaliciousType列表
  const std::vector<int>& GetSafeBrowsingMaliciousType() const;
  // 获取云控配置的restart_proxy_internal，单位为秒
  int GetRestartProxyInternal() const;
  // 获取云控配置的proxy_tunnel_timeout，单位为秒
  int GetProxyTunnelTimeout() const;
  // 获取云控配置的proxy_connect_timeout，单位为秒
  int GetProxyConnectTimeout() const;
  // 获取云控配置的等待恶意网址拦截查询时间，单位为毫秒
  int GetMaliciousUrlCheckWaitTime() const;
  // 获取UI设置的token信息
  const std::string& GetProxyToken() const;
  // 更新token信息
  void UpdateProxyToken(const std::string& token,
                        const std::string& token_info);

  // For test，用于使能fallback proxy
  void InitializeEnableParamsForTesting(const std::string& proxy_url,
                                        const std::string& header);
  // For test，用于去使能fallback proxy
  void InitializeDisableParamsForTesting();
  // For test,将patterns存放于bypass_rules_
  void SetBypassRulesForTesting(const std::string& patterns);

 private:
  void DisableFallbackProxy();
  // 通知fallback_proxy_service更新proxy server
  void UpdateProxyServer();
  // 根据云控传来的enabled和config_proxy_server值来确定是否需要更新proxy server
  void UpdateFallbackProxyServerConfig(bool enabled,
                                       const std::string& config_proxy_server);
  // For test,用于设置tunnel header
  void InitializeHeadersForTesting(const std::string& header);

  bool enabled_;

  bool config_enabled_ = false;

  std::vector<net::ProxyServer> proxy_servers_;

  std::vector<int> using_proxy_error_codes_;

  std::vector<int> safe_browsing_hw_codes_;

  std::vector<int> safe_browsing_malicious_types_;

  // 等待多久重新启用代理服务时间，单位为秒.
  int restart_proxy_internal_ = 300;

  // 和代理服务器建立隧道的超时时间，单位为毫秒.
  int proxy_tunnel_timeout_ = 0;

  // 和代理服务器建立socket的超时时间，单位为毫秒.
  int proxy_connect_timeout_ = 0;

  // 恶意网址拦截信息查询等待时间，单位为毫秒.
  int malicious_url_check_wait_time_ = 0;

  // Rules for using the Fallback Proxy.
  net::ProxyBypassRules bypass_rules_;

  // The token used to for tunner headers which used to setup the connect
  // tunnel.
  std::string auth_token_;

  base::RepeatingClosure proxy_server_updated_callback_;

  base::RepeatingClosure proxy_info_updated_callback_;

  base::RepeatingClosure proxy_token_updated_callback_;

protected:
  void WriteCurrentConfigToLog();
};

}  // namespace fallback_proxy

#endif  // CEF_LIBCEF_BROWSER_FALLBACK_PROXY_CONFIG_H
