// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
 
#include "libcef/browser/ohos_safe_browsing/ohos_sb_snapshot_info.h"
 
namespace ohos_safe_browsing {
 
std::pair<std::string, bool> sbNeedSnapshot::s_pending_safe_browsering_check_url = 
        std::make_pair("", false);
 
std::pair<std::string, bool> sbNeedSnapshot::s_pending_fmp_url = std::make_pair("", false);
 
std::string sbNeedSnapshot::s_navigation_entry_committed_url = "";
 
}  // namespace ohos_safe_browsing