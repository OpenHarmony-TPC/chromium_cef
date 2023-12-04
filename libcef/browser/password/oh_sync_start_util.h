/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 * Various utilities for kicking off sync initialization from data types or
 * other services.
 */

#ifndef CEF_LIBCEF_BROWSER_PASSWORD_OH_SYNC_START_UTIL_H_
#define CEF_LIBCEF_BROWSER_PASSWORD_OH_SYNC_START_UTIL_H_

#include "components/sync/model/syncable_service.h"

namespace base {
class FilePath;
}

namespace oh_sync_start_util {

// Creates a StartSyncFlare that a SyncableService can use to tell
// ProfileSyncService it needs sync to start ASAP.  Typically this would be
// given to the SyncableService on construction.
//
// The flare built by this function is designed to be Run()able from any thread
// so that non-UI types don't have to deal with posting tasks.
//
// |profile_path| is used to get a hold of the actual Profile* once the
// request to start sync is safely in UI Thread land.
syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& file_path);

}  // namespace oh_sync_start_util

#endif  // CEF_LIBCEF_BROWSER_PASSWORD_OH_SYNC_START_UTIL_H_
