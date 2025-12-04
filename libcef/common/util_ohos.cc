// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/util_ohos.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"

namespace util_ohos {

namespace {

void OverrideChildProcessPath() {
  base::FilePath child_process_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kBrowserSubprocessPath);
  if (child_process_path.empty()) {
    return;
  }

  // Used by ChildProcessHost::GetChildPath and PlatformCrashpadInitialization.
  base::PathService::Override(content::CHILD_PROCESS_EXE, child_process_path);
}

}  // namespace

void PreSandboxStartup() {
  OverrideChildProcessPath();
}

}  // namespace util_ohos
