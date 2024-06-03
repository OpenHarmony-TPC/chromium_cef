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

#include "libcef/browser/subresource_filter/adblock_safe_browsing_database_manager_impl.h"

#include <set>
#include <string>
#include <vector>
#include "base/notreached.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

SafeBrowsingDatabaseManagerImpl::SafeBrowsingDatabaseManagerImpl(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : SafeBrowsingDatabaseManager(std::move(ui_task_runner),
                                  std::move(io_task_runner)) {}

void SafeBrowsingDatabaseManagerImpl::CancelCheck(Client* client) {
  NOTIMPLEMENTED();
}

bool SafeBrowsingDatabaseManagerImpl::CanCheckRequestDestination(
    network::mojom::RequestDestination request_destination) const {
  NOTIMPLEMENTED();
  return false;
}

bool SafeBrowsingDatabaseManagerImpl::CanCheckUrl(const GURL& url) const {
  NOTIMPLEMENTED();
  return (url != GURL("about:blank"));
}

bool SafeBrowsingDatabaseManagerImpl::ChecksAreAlwaysAsync() const {
  NOTIMPLEMENTED();
  return false;
}

bool SafeBrowsingDatabaseManagerImpl::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    MechanismExperimentHashDatabaseCache experiment_cache_selection) {
  NOTIMPLEMENTED();
  return true;
}

bool SafeBrowsingDatabaseManagerImpl::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool SafeBrowsingDatabaseManagerImpl::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool SafeBrowsingDatabaseManagerImpl::CheckResourceUrl(const GURL& url,
                                                       Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool SafeBrowsingDatabaseManagerImpl::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    const std::string& metric_variation) {
  NOTIMPLEMENTED();
  return false;
}

bool SafeBrowsingDatabaseManagerImpl::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

AsyncMatch SafeBrowsingDatabaseManagerImpl::CheckCsdAllowlistUrl(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return AsyncMatch::MATCH;
}

bool SafeBrowsingDatabaseManagerImpl::MatchDownloadAllowlistUrl(
    const GURL& url) {
  NOTIMPLEMENTED();
  return true;
}

bool SafeBrowsingDatabaseManagerImpl::MatchMalwareIP(
    const std::string& ip_address) {
  NOTIMPLEMENTED();
  return true;
}

safe_browsing::ThreatSource SafeBrowsingDatabaseManagerImpl::GetThreatSource()
    const {
  NOTIMPLEMENTED();
  return safe_browsing::ThreatSource::UNKNOWN;
}

bool SafeBrowsingDatabaseManagerImpl::IsDownloadProtectionEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace safe_browsing
