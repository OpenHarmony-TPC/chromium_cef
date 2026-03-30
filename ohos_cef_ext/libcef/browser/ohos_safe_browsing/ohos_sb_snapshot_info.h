// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_
#define CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_

#include <mutex>
#include <string>
#include <utility>
#include <shared_mutex>

#include "base/logging.h"
#include "base/containers/lru_cache.h"
#include "arkweb/ohos_nweb/src/capi/nweb_safe_browsing_detection_result_item.h"

namespace ohos_safe_browsing {

class SafeBrowsingSnapshotManager {

private:
SafeBrowsingSnapshotManager() = default;
~SafeBrowsingSnapshotManager() = default;

SafeBrowsingSnapshotManager(const SafeBrowsingSnapshotManager&) = delete;
SafeBrowsingSnapshotManager& operator=(const SafeBrowsingSnapshotManager&) = delete;

mutable std::shared_mutex data_mutex_;

std::map<uint32_t, base::LRUCache<std::string, SafeBrowsingDetectionResult>> pending_safe_browsing_check_map_;

struct PendingFmp {
    std::string url;
    uint32_t nwebId{0};
    
    bool empty() const { return url.empty(); }
} pending_fmp_;

std::pair<std::string, SafeBrowsingDetectionResult> antifraud_detecting_url_;

public:
static SafeBrowsingSnapshotManager& GetInstance() {
  static SafeBrowsingSnapshotManager instance;
  return instance;
}

void SetNewUrl(std::string newUrl,    
               const SafeBrowsingDetectionResult& result, 
               uint32_t nwebId);

void SetFMP(std::string fmpUrl, uint32_t nwebId);

bool IsAllowSnapshot();

std::optional<std::pair<std::string, SafeBrowsingDetectionResult>> 
    GetAntiFraudDetectingUrlInfo() const;

SafeBrowsingDetectionResult GetAntiFraudDetectingResult() const;

PendingFmp GetPendingFmp() const;

std::string GetPendingFmpUrl() const;

void ResetFmp();
void ClearCache(uint32_t nwebId);

};

} // namespace ohos_safe_browsing

#endif  // CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_
