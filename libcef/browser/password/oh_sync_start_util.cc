// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "libcef/browser/password/oh_sync_start_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"

namespace {

void StartSyncOnUIThread(const base::FilePath& file_path,
                         syncer::ModelType type) {
  LOG(INFO) << "sync_start_util StartSyncOnUIThread";
  // TODO
}

void StartSyncProxy(const base::FilePath& file_path, syncer::ModelType type) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&StartSyncOnUIThread, file_path, type));
}

}  // namespace

namespace oh_sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& file_path) {
  return base::BindRepeating(&StartSyncProxy, file_path);
}

}  // namespace oh_sync_start_util
