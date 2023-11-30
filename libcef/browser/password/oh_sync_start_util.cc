/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 */

#include "libcef/browser/password/oh_sync_start_util.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

void StartSyncOnUIThread(const base::FilePath& file_path,
                         syncer::ModelType type) {
  LOG(INFO) << "sync_start_util StartSyncOnUIThread";
  // TODO
}

void StartSyncProxy(const base::FilePath& file_path, syncer::ModelType type) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&StartSyncOnUIThread, file_path, type));
}

}  // namespace

namespace oh_sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& file_path) {
  return base::BindRepeating(&StartSyncProxy, file_path);
}

}  // namespace oh_sync_start_util
