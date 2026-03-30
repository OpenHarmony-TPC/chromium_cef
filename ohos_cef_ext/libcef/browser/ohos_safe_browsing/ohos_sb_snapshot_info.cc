// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "libcef/browser/ohos_safe_browsing/ohos_sb_snapshot_info.h"

namespace ohos_safe_browsing {

void SafeBrowsingSnapshotManager::SetNewUrl(std::string newUrl,    
               const SafeBrowsingDetectionResult& result, 
               uint32_t nwebId) 
{
    if (newUrl.empty()) return;
    
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    auto it = pending_safe_browsing_check_map_.find(nwebId);
    
    if (it != pending_safe_browsing_check_map_.end()) {
        it->second.Put(std::move(newUrl), result);
    } else {
        base::LRUCache<std::string, SafeBrowsingDetectionResult> newCache(100);
        newCache.Put(std::move(newUrl), result);
        pending_safe_browsing_check_map_.emplace(nwebId, std::move(newCache));
    }
}

void SafeBrowsingSnapshotManager::SetFMP(std::string fmpUrl, uint32_t nwebId) {
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    pending_fmp_.url = std::move(fmpUrl);
    pending_fmp_.nwebId = nwebId;
}

bool SafeBrowsingSnapshotManager::IsAllowSnapshot() {
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    
    if (pending_safe_browsing_check_map_.empty()) {
        return false;
    }
    
    if (pending_fmp_.empty()) {
        return false;
    }
    
    auto it = pending_safe_browsing_check_map_.find(pending_fmp_.nwebId);
    if (it == pending_safe_browsing_check_map_.end()) {
        return false;
    }
    
    auto cacheIt = it->second.Peek(pending_fmp_.url);
    if (cacheIt == it->second.end()) {
        return false;
    }
    
    antifraud_detecting_url_.first = pending_fmp_.url;
    antifraud_detecting_url_.second = cacheIt->second;
    
    return true;
}

std::optional<std::pair<std::string, SafeBrowsingDetectionResult>> 
    SafeBrowsingSnapshotManager::GetAntiFraudDetectingUrlInfo() const 
{
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    if (antifraud_detecting_url_.first.empty()) {
        return std::nullopt;
    }
    return antifraud_detecting_url_;
}

SafeBrowsingDetectionResult SafeBrowsingSnapshotManager::
    GetAntiFraudDetectingResult() const 
{
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    if (antifraud_detecting_url_.first.empty()) {
        return SafeBrowsingDetectionResult();
    }
    return antifraud_detecting_url_.second;
}

ohos_safe_browsing::SafeBrowsingSnapshotManager::PendingFmp 
SafeBrowsingSnapshotManager::GetPendingFmp() const 
{
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    return pending_fmp_;
}

std::string SafeBrowsingSnapshotManager::GetPendingFmpUrl() const 
{
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    return pending_fmp_.url;
}


void SafeBrowsingSnapshotManager::ResetFmp() {
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    pending_fmp_.url.clear();
    pending_fmp_.nwebId = 0;
}

void SafeBrowsingSnapshotManager::ClearCache(uint32_t nwebId) {
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    pending_safe_browsing_check_map_.erase(nwebId);
}

}  // namespace ohos_safe_browsing
