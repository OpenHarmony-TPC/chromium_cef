/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_ADBLOCK_SAFE_BROWSING_DATABASE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_ADBLOCK_SAFE_BROWSING_DATABASE_MANAGER_IMPL_H_

#include <set>
#include <string>
#include <vector>
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace safe_browsing {

class SafeBrowsingDatabaseManagerImpl : public SafeBrowsingDatabaseManager {
 public:
  SafeBrowsingDatabaseManagerImpl(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // SafeBrowsingDatabaseManager implementation:
  void CancelCheck(Client* client) override;
  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection) override;
  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      const std::string& metric_variation) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool MatchDownloadAllowlistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool IsDownloadProtectionEnabled() const override;

 protected:
  ~SafeBrowsingDatabaseManagerImpl() override = default;
};

}  // namespace safe_browsing

#endif  // CEF_LIBCEF_BROWSER_ADBLOCK_SAFE_BROWSING_DATABASE_MANAGER_IMPL_H_
