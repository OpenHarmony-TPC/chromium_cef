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

#include "fallback_proxy_service.h"

#include "arkweb/ohos_nweb_ex/overrides/net/proxy_resolution/fallback_proxy_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "arkweb/ohos_nweb/src/nweb_impl.h"

namespace fallback_proxy {
namespace {
constexpr char kFallbackProxyBlockList[] = "fallbackProxyBlockList";
constexpr int kFallbackProxyFailureLimit = 3;
}  // namespace

void FallbackProxyService_OnUpdateProxyToken(const std::string& old_token) {
  // Triggering Client Callback
  LOG(DEBUG) << "Fallback FallbackProxyService_OnUpdateProxyToken old_token:"
             << old_token;
  OHOS::NWeb::NWebImpl::OnUpdateProxyToken(old_token);
}

// static
FallbackProxyService* FallbackProxyService::GetInstance() {
  static base::NoDestructor<FallbackProxyService> instance;
  return instance.get();
}

FallbackProxyService::FallbackProxyService()
    : network_connection_tracker_(content::GetNetworkConnectionTracker()) {
  config_ = std::make_unique<FallbackProxyConfig>();
  config_->SetProxyServerUpdatedCallback(base::BindRepeating(
      &FallbackProxyService::OnProxyConfigUpdated, base::Unretained(this)));
  config_->SetProxyInfoUpdatedCallback(base::BindRepeating(
      &FallbackProxyService::OnProxyInfoUpdated, base::Unretained(this)));
  config_->SetProxyTokenUpdatedCallback(base::BindRepeating(
      &FallbackProxyService::OnProxyTokenUpdated, base::Unretained(this)));

  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&FallbackProxyService::OnConnectionChanged,
                     base::Unretained(this)));
  ArkwebGlobalListConfig::GetInstance()->AddConfigChangeObserver(
      kFallbackProxyBlockList, this);
  LOG(DEBUG) << "Fallback proxy service created success";
}

FallbackProxyService::~FallbackProxyService() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  ArkwebGlobalListConfig::GetInstance()->RemoveConfigChangeObservers(
      kFallbackProxyBlockList);
}

void FallbackProxyService::OnProxyConfigUpdated() {
  net::FallbackProxyConfigStatus::SetStatus(config_->ConfigEnabled(),
                                            !config_->GetProxyToken().empty());
  auto proxy_servers = config_->GetProxyServers();
  if (proxy_servers.empty()) {
    LOG(INFO) << "Fallback proxy will be disabled for the config updated and "
                 "the proxy server is empty";
    Disable(CONFIG_DISABLED, 0);
  } else {
    if (config_->GetProxyToken().empty()) {
      LOG(INFO)
          << "Fallback proxy won't be enabled for the config updated while "
             "the token is empty";
      disabled_reason_ = NEED_AUTH_TOKEN;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&FallbackProxyService_OnUpdateProxyToken,
                                    last_report_token_));
    } else {
      LOG(INFO) << "Fallback proxy will be enabled for the config updated";
      Enable();
    }
  }
}

void FallbackProxyService::OnProxyInfoUpdated() {
  network::mojom::FallbackProxyInfoConfigPtr config =
      CreateFallbackProxyInfoConfig();
  for (auto& client : proxy_config_clients_) {
    client->OnFallbackProxyInfoConfigUpdated(config->Clone());
  }
}

void FallbackProxyService::OnProxyTokenUpdated() {
  net::FallbackProxyConfigStatus::SetStatus(config_->ConfigEnabled(),
                                            !config_->GetProxyToken().empty());
  if (is_enabled_) {
    std::string auth_token(net::kFallbackProxyTunnelHeaderKey);
    auth_token.append(":");
    auth_token.append(config_->GetProxyToken());
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(auth_token);
    LOG(INFO) << "Fallback proxy notify clients to update new tunnel header";
    for (auto& client : proxy_config_clients_) {
      client->UpdateFallbackProxyAuthHeaders(headers);
    }
  } else if (disabled_reason_ == NEED_AUTH_TOKEN) {
    LOG(INFO)
        << "Fallback proxy will be enabled for new tunnel header received";
    Enable();
  }
}

void FallbackProxyService::AddCustomProxyConfigClient(
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client) {
  proxy_config_clients_.Add(std::move(config_client));
}

net::ProxyConfig FallbackProxyService::CreateProxyConfig(
    const std::vector<net::ProxyServer>& proxy_servers) const {
  net::ProxyConfig config;
  DCHECK(config.proxy_rules().single_proxies.IsEmpty());
  config.proxy_rules().type = net::ProxyConfig::ProxyRules::Type::PROXY_LIST;

  for (const auto& proxy_server : proxy_servers) {
    config.proxy_rules().single_proxies.AddProxyServer(proxy_server);
  }

  if (config.proxy_rules().single_proxies.IsEmpty()) {
    // Return a DIRECT net config so that fallback proxy is not used.
    return net::ProxyConfig::CreateDirect();
  }

  config.proxy_rules().bypass_rules = config_->GetBypassRules();
  config.proxy_rules().reverse_bypass = true;
  return config;
}

network::mojom::CustomProxyConfigPtr
FallbackProxyService::CreateCustomProxyConfig(
    const std::vector<net::ProxyServer>& proxy_servers) const {
  auto config = network::mojom::CustomProxyConfig::New();

  config->rules = CreateProxyConfig(proxy_servers).proxy_rules();
  config->should_override_existing_config = false;
  config->allow_non_idempotent_methods = false;

  std::string header(net::kFallbackProxyTunnelHeaderKey);
  header.append(":");
  header.append(config_->GetProxyToken());
  net::HttpRequestHeaders tunnel_header;
  tunnel_header.AddHeadersFromString(header);
  config->connect_tunnel_headers = tunnel_header;

  return config;
}

void FallbackProxyService::UpdateCustomProxyConfig(
    const std::vector<net::ProxyServer>& proxy_servers) {
  network::mojom::CustomProxyConfigPtr config =
      CreateCustomProxyConfig(proxy_servers);
  for (auto& client : proxy_config_clients_) {
    client->OnCustomProxyConfigUpdated(config->Clone(), base::DoNothing());
  }
}

network::mojom::FallbackProxyInfoConfigPtr
FallbackProxyService::CreateFallbackProxyInfoConfig() const {
  auto config = network::mojom::FallbackProxyInfoConfig::New();
  config->using_proxy_error_codes = config_->GetUsingProxyErrorCodes();
  config->safe_browsing_hw_codes = config_->GetSafeBrowsingHwCode();
  config->safe_browsing_malicious_types =
      config_->GetSafeBrowsingMaliciousType();
  config->proxy_tunnel_timeout = config_->GetProxyTunnelTimeout();
  config->proxy_connect_timeout = config_->GetProxyConnectTimeout();
  config->malicious_url_check_wait_time =
      config_->GetMaliciousUrlCheckWaitTime();
  return config;
}

mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
FallbackProxyService::NewProxyConnectionObserverRemote() {
  mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
      observer_remote;
  observer_receivers_.Add(this,
                          observer_remote.InitWithNewPipeAndPassReceiver());
  // The disconnect handler is intentionally not set since ReceiverSet managers
  // connection clean up on disconnect.
  return observer_remote;
}

void FallbackProxyService::OnTunnelHeadersReceivedWithToken(
    const net::ProxyChain& proxy_chain,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    const std::string& token) {
  if (!proxy_chain.is_single_proxy()) {
    LOG(ERROR)
        << "FallbackProxy allows only one proxy server to be configured.";
    return;
  }
  const net::ProxyServer& proxy_server = proxy_chain.First();

  if (!config_->HasProxyServer(proxy_server)) {
    return;
  }
  int response_code = response_headers->response_code();
  if (response_code == net::FallbackProxyResponseCode::TOKEN_AUTH_FAILED) {
    // notify to get new tunnel headers
    if (last_report_token_.empty() || last_report_token_ != token) {
      LOG(INFO) << "Fallback proxy will be disabled for tunnel auth failed, "
                   "reason code is "
                << response_code << ", the token is changed";
      Disable(NEED_AUTH_TOKEN, response_code);
      last_report_token_ = token;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&FallbackProxyService_OnUpdateProxyToken,
                                    last_report_token_));
    } else if (token == config_->GetProxyToken()) {
      LOG(INFO) << "Fallback proxy will be disabled for tunnel auth failed, "
                   "reason code is "
                << response_code
                << ", the token is same as the config's token";
      Disable(NEED_AUTH_TOKEN, response_code);
    }
  } else if (response_code ==
                 net::FallbackProxyResponseCode::EXCEEDS_THRESHOLD ||
             response_code == net::FallbackProxyResponseCode::INTERNAL_ERROR ||
             response_code ==
                 net::FallbackProxyResponseCode::INSUFFICIENT_RESOURCE) {
    LOG(INFO)
        << "Fallback proxy will be disabled for tunnel failed, reason code is "
        << response_code;
    Disable(SERVER_INTERNAL_ERROR, response_code);
  }
  last_failure_count_ = 0;
}

void FallbackProxyService::OnProxyConnectResult(
    const net::ProxyChain& proxy_chain,
    int net_error) {
  if (!proxy_chain.is_single_proxy()) {
    LOG(ERROR)
        << "FallbackProxy allows only one proxy server to be configured.";
    return;
  }
  const net::ProxyServer& proxy_server = proxy_chain.First();

  if (!config_->HasProxyServer(proxy_server)) {
    return;
  }

  if (net_error == net::ERR_PROXY_CERTIFICATE_INVALID) {
    LOG(INFO)
        << "Fallback proxy will be disabled for proxy certificate is invalid";
    Disable(SERVER_SSL_ERROR, net_error);
  } else if (net_error == net::ERR_PROXY_CONNECTION_FAILED ||
             net_error == net::ERR_TIMED_OUT) {
    ++last_failure_count_;
    if (last_failure_count_ >= kFallbackProxyFailureLimit) {
      LOG(INFO) << "Fallback proxy will be disabled for proxy connect failed "
                   "exceeds the threshold";
      Disable(SERVER_CONNECT_FAILED, net_error);
    }
  }
}

void FallbackProxyService::OnInterestedConfigChanged() {
  std::vector<std::string> block_list =
      ArkwebGlobalListConfig::GetInstance()->GetFallbackProxyBlockList();
  LOG(DEBUG) << "Fallback proxy block list has changed, size "
             << block_list.size();
  for (auto& client : proxy_config_clients_) {
    client->OnHostBlockListUpdated(block_list);
  }
}

void FallbackProxyService::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (connection_type_ == type) {
    return;
  }

  connection_type_ = type;
  if (connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE) {
    return;
  }

  if (!is_enabled_ && disabled_reason_ == SERVER_CONNECT_FAILED) {
    LOG(INFO) << "Fallback proxy will be enabled for the network type is "
                 "changed, connectionType="
              << static_cast<int>(connection_type_);
    Enable();
  }
}

void FallbackProxyService::Enable() {
  disabled_reason_ = INIT_VALUE;
  auto proxy_servers = config_->GetProxyServers();
  if (proxy_servers.empty()) {
    LOG(INFO)
        << "Fallback proxy is enabled failed, for the proxy server is empty";
    return;
  }

  if (is_enabled_ && proxy_servers == proxy_servers_) {
    return;
  }

  LOG(INFO) << "Fallback proxy enabled.";
  is_enabled_ = true;
  proxy_servers_ = proxy_servers;
  last_failure_count_ = 0;
  UpdateCustomProxyConfig(proxy_servers);
  UpdateFallbackProxyStatus(net::FallbackProxyStatus::NORMAL);
}

void FallbackProxyService::Disable(DisabledReason reason, int reason_code) {
  if (!is_enabled_ && disabled_reason_ != INIT_VALUE) {
    if (reason == CONFIG_DISABLED) {
      disabled_reason_ = reason;
      if (restart_timer_.IsRunning()) {
        restart_timer_.Stop();
      }
      LOG(INFO)
          << "Fallback proxy is already disabled, the disabled reason is "
          << reason;
      UpdateFallbackProxyStatus(
          net::FallbackProxyStatus::DISABLE_BY_CLOUD_CONTROL);
    }
    return;
  }

  if (reason == SERVER_INTERNAL_ERROR && !restart_timer_.IsRunning()) {
    restart_timer_.Start(FROM_HERE,
                         base::Seconds(config_->GetRestartProxyInternal()),
                         this, &FallbackProxyService::Enable);
  }

  LOG(INFO) << "Fallback proxy disabled, the reason is " << reason;
  is_enabled_ = false;
  disabled_reason_ = reason;
  UpdateCustomProxyConfig(std::vector<net::ProxyServer>());
  net::FallbackProxyStatus status =
      GetFallbackProxyStatusByDisabledReason(reason);
  UpdateFallbackProxyStatus(status);
  if (reason != CONFIG_DISABLED) {
    net::ReportProxyExceptionReason(reason_code);
  }
}

void FallbackProxyService::SaveURLMaliciousTypeAndHwCode(const std::string& url,
                                                         int type,
                                                         int hw_code) {
  LOG(DEBUG)
      << "FallbackProxyService::SaveURLMaliciousTypeAndHwCode is_enabled_"
      << is_enabled_;
  if (!is_enabled_) {
    return;
  }

  for (auto& client : proxy_config_clients_) {
    LOG(DEBUG) << "FallbackProxyService::SaveURLMaliciousTypeAndHwCode";
    client->SaveURLMaliciousTypeAndHwCode(url, type, hw_code);
  }
}

void FallbackProxyService::UpdateFallbackProxyStatus(
    net::FallbackProxyStatus status) {
  for (auto& client : proxy_config_clients_) {
    client->UpdateFallbackProxyStatus(static_cast<int>(status));
  }
}

void FallbackProxyService::SetFallbackProxyConfigData(
    const ohos_cloud_control::FallbackProxyConfigData& config_data) {
  LOG(DEBUG) << "Fallback proxy service set cloud control data";
  config_->SetFallbackProxyConfigData(config_data);
}

void FallbackProxyService::UpdateProxyToken(const std::string& token,
                                            const std::string& token_info) {
  LOG(DEBUG) << "Fallback proxy service updates proxy token, token " << token;
  config_->UpdateProxyToken(token, token_info);
}

void FallbackProxyService::EnableProxyForTesting(const std::string& proxy_url,
                                                 const std::string& header) {
  if (config_) {
    config_->InitializeEnableParamsForTesting(proxy_url, header);
  }
}

void FallbackProxyService::DisableProxyForTesting() {
  if (config_) {
    config_->InitializeDisableParamsForTesting();
  }
}

void FallbackProxyService::OnHostBlockListUpdatedForTesting(
    const std::vector<std::string>& block_list) {
  for (auto& client : proxy_config_clients_) {
    client->OnHostBlockListUpdated(block_list);
  }
}

net::FallbackProxyStatus
FallbackProxyService::GetFallbackProxyStatusByDisabledReason(
    DisabledReason reason) {
  switch (reason) {
    case CONFIG_DISABLED:
      return net::FallbackProxyStatus::DISABLE_BY_CLOUD_CONTROL;
    case NEED_AUTH_TOKEN:
      return net::FallbackProxyStatus::AUTH_FAILED;
    default:
      return net::FallbackProxyStatus::UNAVAILABLE;
  }
}

// for testing
void FallbackProxyService::RemoveNetworkConnectionObserver() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

}  // namespace fallback_proxy
