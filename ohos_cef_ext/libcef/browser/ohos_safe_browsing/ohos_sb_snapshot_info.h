// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
#ifndef CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_
#define CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_
 
#include <mutex>
#include <string>
#include <utility>
 
#include "base/logging.h"
 
namespace ohos_safe_browsing {
 
struct sbNeedSnapshot{
  sbNeedSnapshot() = delete;
  sbNeedSnapshot(const sbNeedSnapshot&) = delete;
  sbNeedSnapshot& operator=(const sbNeedSnapshot&) = delete;
 
  static std::pair<std::string, bool> s_pending_safe_browsering_check_url;
  static std::pair<std::string, bool> s_pending_fmp_url;
  static std::string s_navigation_entry_committed_url;
 
  static void SetNewURL(std::string new_url) {
    s_pending_safe_browsering_check_url.first = new_url;
    s_pending_safe_browsering_check_url.second = true;
  }
 
  static void SetLCP(std::string lcp_url) {
    s_pending_fmp_url.first = lcp_url;
    s_pending_fmp_url.second = true;
 
  }
  
  static void SetNavigationEntryCommittedUrl(std::string new_url) {
    s_navigation_entry_committed_url = new_url;
  }
 
  static bool IsAllowSnapshot() {
    if(s_pending_safe_browsering_check_url.first == "" || s_pending_fmp_url.first == "" || 
       s_pending_safe_browsering_check_url.first != s_pending_fmp_url.first){
      return false;
    }
    return s_pending_safe_browsering_check_url.second && s_pending_fmp_url.second;
  
  }
 
  static void Reset() {
    s_pending_safe_browsering_check_url.first = "";
    s_pending_safe_browsering_check_url.second = false;
    s_pending_fmp_url.first = "";
    s_pending_fmp_url.second = false;
  }
};
 
}  // namespace ohos_safe_browsing
 
#endif  // CEF_LIBCEF_BROWSER_SB_SNAPSHOT_INFO_H_