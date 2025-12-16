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

#ifndef CEF_LIBCEF_BROWSER_FALLBACK_PROXY_SERVICE_H
#define CEF_LIBCEF_BROWSER_FALLBACK_PROXY_SERVICE_H

#include <memory>
#include <vector>

#include "arkweb/chromium_ext/net/base/fallback_proxy_constants.h"
#include "arkweb/chromium_ext/servieces/network/public/mojom/network_config_ohos.mojom.h"
#include "base/no_destructor.h"
#include "cef/ohos_cef_ext/libcef/browser/arkweb_global_list_config.h"
#include "fallback_proxy_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/proxy_server.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "ohos_nweb_ex/overrides/cef/libcef/browser/browser_config/browser_cloud_control_global_config.h"
#endif
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace fallback_proxy {

class FallbackProxyService
    : public network::mojom::CustomProxyConnectionObserver,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public ArkwebGlobalListConfig::GlobalListConfigObserver {
 public:
  static FallbackProxyService* GetInstance();

  // Adds a config client that can be used to update Fallback Proxy settings
  void AddCustomProxyConfigClient(
      mojo::Remote<network::mojom::CustomProxyConfigClient> config_client);

  // Passes a new pending remote for the custom proxy connection observer
  // implemented by this class.
  mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
  NewProxyConnectionObserverRemote();

  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override {}
  void OnTunnelHeadersReceived(const net::ProxyChain& proxy_chain,
                               uint64_t chain_index,
                               const scoped_refptr<net::HttpResponseHeaders>&
                                   response_headers) override {}

  // http_proxy_client_socket收到Http headers应答时的会调函数
  // 如果响应码为600，说明和代理服务器鉴权失败，此时需要将参数token和last_report_token_
  // 比较，如果两者不相等的话，则Disable代理且通知UI更新头部信息；如果两者相等且参数token
  // 和配置的token值相等的话，也Disable代理.
  // 如果响应码为601/602/605的话，则Disable代理，且起一个重启代理的定时器.
  void OnTunnelHeadersReceivedWithToken(
      const net::ProxyChain& proxy_chain,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      const std::string& token) override;
  // url_request对于使用了fallback proxy的请求会通过proxy_delegate回调此函数
  // 当error_code为-130或-7时，且失败次数连续达到3次，则Disable代理，待网络切换后重启代理
  // 当error_code为-136时，则Disable代理，且本次浏览器生命周期内都不再重启代理
  void OnProxyConnectResult(const net::ProxyChain& proxy_chain,
                            int error_code) override;

  // override ArkwebGlobalListConfig::GlobalListConfigObserver
  void OnInterestedConfigChanged() override;

  // 通知proxy_delegate存放url恶意网址拦截相关信息
  void SaveURLMaliciousTypeAndHwCode(const std::string& url,
                                     int type,
                                     int hw_code);
  // 通知proxy_delegate存放fallback proxy info，如proxyTunnelTimeout等.
  void SetFallbackProxyConfigData(
      const ohos_cloud_control::FallbackProxyConfigData& config_data);
  void UpdateProxyToken(const std::string& token,
                        const std::string& token_info);

  // Should be called whenever there is a possible change to the custom proxy
  // config.
  void UpdateCustomProxyConfig(
      const std::vector<net::ProxyServer>& proxy_servers);

  // 以下函数提供给测试用例所用:
  // Called after custom proxy config client added
  void EnableProxyForTesting(const std::string& proxy_url,
                             const std::string& header);
  void DisableProxyForTesting();
  void RemoveNetworkConnectionObserver();
  FallbackProxyConfig* config() { return config_.get(); }
  void OnHostBlockListUpdatedForTesting(
      const std::vector<std::string>& block_list);

 private:
  enum DisabledReason {
    INIT_VALUE,
    SERVER_CONNECT_FAILED,
    SERVER_SSL_ERROR,
    SERVER_INTERNAL_ERROR,
    NEED_AUTH_TOKEN,
    CONFIG_DISABLED
  };

  friend class base::NoDestructor<FallbackProxyService>;
  FallbackProxyService();
  ~FallbackProxyService() override;

  // Callback when the network type chagned.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Callback for fallback_proxy_config when the config proxies server or
  // enabled switch updated.
  void OnProxyConfigUpdated();
  // Callback for fallback_proxy_config when the fallback proxy info updated,
  // for example, ProxyTunnelTimeout or fallback proxy retried error codes.
  void OnProxyInfoUpdated();
  // Callback for fallback_proxy_config when the tunnel token updated.
  void OnProxyTokenUpdated();

  // Constructs a proxy configuration.
  net::ProxyConfig CreateProxyConfig(
      const std::vector<net::ProxyServer>& proxy_servers) const;

  // notify NetworkServiceProxyDelegate to update the status of the fallback
  // proxy.
  void UpdateFallbackProxyStatus(net::FallbackProxyStatus status);

  // Creates a config of proxy server that can be sent to
  // NetworkServiceProxyDelegate.
  network::mojom::CustomProxyConfigPtr CreateCustomProxyConfig(
      const std::vector<net::ProxyServer>& proxy_servers) const;

  // Creates a config of fallback proxy info (eg. proxyConnectTimeout) that
  // can be sent to NetworkServiceProxyDelegate.
  network::mojom::FallbackProxyInfoConfigPtr CreateFallbackProxyInfoConfig()
      const;

  // Enabled fallback proxy service with valid proxy servers and tunnel headers.
  void Enable();
  // Disabled fallback proxy service.
  void Disable(DisabledReason reason, int reason_code);

  net::FallbackProxyStatus GetFallbackProxyStatusByDisabledReason(
      DisabledReason reason);

  // Must be accessed on UI thread. Guaranteed to be non-null during the
  // lifetime of |this|.
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;

  // Parameters including DNS names and allowable configurations.
  std::unique_ptr<FallbackProxyConfig> config_;

  // The set of clients that will get updates about changes to the proxy config.
  mojo::RemoteSet<network::mojom::CustomProxyConfigClient>
      proxy_config_clients_;

  mojo::ReceiverSet<network::mojom::CustomProxyConnectionObserver>
      observer_receivers_;

  // Records the number of consecutive failures of the fallback proxy.
  int last_failure_count_ = 0;

  bool is_enabled_ = false;

  std::vector<net::ProxyServer> proxy_servers_;

  DisabledReason disabled_reason_ = INIT_VALUE;

  std::string last_report_token_;

  base::OneShotTimer restart_timer_;
};

}  // namespace fallback_proxy

#endif  // CEF_LIBCEF_BROWSER_FALLBACK_PROXY_SERVICE_H
