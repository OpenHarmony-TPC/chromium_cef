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
#include "libcef/browser/ohos_safe_browsing/ohos_url_trust_list_manager.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"

namespace ohos_safe_browsing {
static constexpr int MAX_PATH_SIZE = 0x10000;

char OhosUrlTrustListInterface::interfaceKey;

OhosUrlTrustListManager::OhosUrlTrustListManager() {}

UrlListSetResult OhosUrlTrustListManager::SetUrlTrustList(
  const std::string& urlTrustList)
{
  if (urlTrustList.empty()) {
    ruleMap_.clear();
    return UrlListSetResult::SET_OK;
  }
  absl::optional<base::Value> jsonParsed =
    base::JSONReader::Read(urlTrustList);
  if (!jsonParsed || !jsonParsed->is_dict()) {
    LOG(ERROR) << "parse: json format failed.";
    return UrlListSetResult::PARAM_ERROR;
  }
  base::Value::List* list = jsonParsed->GetDict().FindList("UrlPermissionList");
  if (!list) {
    LOG(ERROR) << "parse: can not find UrlPermissionList.";
    return UrlListSetResult::PARAM_ERROR;
  }

  if (list->size() == 0) {
    LOG(INFO) << "parse: list is empty, disable url trust list.";
    ruleMap_.clear();
    return UrlListSetResult::SET_OK;
  }

  std::multimap<std::string, UrlTrustRule> map;
  for (const auto& ruleJson : *list) {
    UrlTrustRule rule;
    base::JSONValueConverter<UrlTrustRule> converter;
    converter.Convert(ruleJson, &rule);
    if (rule.host.empty()) {
      LOG(ERROR) << "parse: empty host.";
      return UrlListSetResult::PARAM_ERROR;
    }
    if (rule.port == -1) {
      LOG(ERROR) << "parse: host " << rule.host << " port invalid.";
      return UrlListSetResult::PARAM_ERROR;
    }
    if (rule.path.size() > MAX_PATH_SIZE) {
      LOG(ERROR) << "parse: host " << rule.host << " path len too long.";
      return UrlListSetResult::PARAM_ERROR;
    }
    map.insert(std::make_pair(rule.host, rule));
  }
  ruleMap_ = map;
  return UrlListSetResult::SET_OK;
}

static std::string GetUrlRealPath(std::string path)
{
  if (path.empty()) {
    return path;
  }
  if (path[0] == '/') {
    path.erase(0, 1);
  }
  return path;
}

UrlTrustCheckResult OhosUrlTrustListManager::CheckUrlTrustList(
  const GURL& url)
{
  if (ruleMap_.size() == 0) {
    return UrlTrustCheckResult::RESULT_ALLOW;
  }

  auto range = ruleMap_.equal_range(url.host());
  std::string realPath = GetUrlRealPath(url.path());
  for (auto itr = range.first; itr != range.second; ++itr) {
    auto& rule = itr->second;
    if (!rule.scheme.empty() && (rule.scheme != url.scheme())) {
      continue;
    }
    if (rule.port > 0 && (rule.port != url.EffectiveIntPort())) {
      continue;
    }
    if (!rule.path.empty()) {
      if (realPath.find(rule.path) != 0) {
        continue;
      }
      size_t next = rule.path.size();
      if (next < realPath.size() && realPath[next] != '/') {
        continue;
      }
    }
    return UrlTrustCheckResult::RESULT_ALLOW;
  }
  LOG(INFO) << "Deny url: scheme:" << url.scheme() <<
    ",host:" << url.host() <<
    ",port:" << url.EffectiveIntPort() <<
    ",path:" << url.path();
  return UrlTrustCheckResult::RESULT_DENY;
}
} // namespace ohos_safe_browsing
