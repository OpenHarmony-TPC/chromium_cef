// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_dns_data_base.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "nweb_pre_dns_adapter.h"
#include "ohos_adapter_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

void CacheHostName(const std::string& hostname) {
  OHOS::NWeb::OhosWebDnsDataBaseAdapter& dnsDatabaseAdapter =
    OHOS::NWeb::OhosAdapterHelper::GetInstance().GetWebDnsDataBaseInstance();
  if (dnsDatabaseAdapter.ExistHostname(hostname)) {
    return;
  }

  dnsDatabaseAdapter.InsertHostname(hostname);
}

net::AddressList GetAddrList(const std::string& hostname) {
  using OHOS::NWeb::g_AddrInfoMap;
  net::AddressList addr_list;
  if (g_AddrInfoMap.empty()) {
    return addr_list;
  }

  addrinfo* addrInfo;
  auto it = g_AddrInfoMap.find(hostname);
  if (it == g_AddrInfoMap.end()) {
    return addr_list;
  }
  addrInfo = it->second;
  auto canonical_name = (addrInfo->ai_canonname != nullptr)
             ? absl::optional<std::string>(std::string(addrInfo->ai_canonname))
             : absl::optional<std::string>();
  if (canonical_name) {
    std::vector<std::string> aliases({*canonical_name});
    addr_list.SetDnsAliases(std::move(aliases));
  }
  for (auto ai = addrInfo; ai != NULL; ai = ai->ai_next) {
    net::IPEndPoint ipe;
    // NOTE: Ignoring non-INET* families.
    if (ipe.FromSockAddr(ai->ai_addr, ai->ai_addrlen))
      addr_list.push_back(ipe);
    else
      LOG(INFO) << "Unknown family found in addrinfo: " << ai->ai_family;
  }
  return addr_list;
}