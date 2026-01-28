/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "gtest/gtest.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"

namespace resource_util {
extern void OverrideUserDataDirExt(const base::FilePath& user_data_path,
                                   const base::CommandLine* command_line);
}  // namespace resource_util

class OverrideUserDataDirExt : public ::testing::Test {
 protected:
  void SetUp() override {
    base::PathService::Override(base::DIR_OHOS_CRASHPAD, base::FilePath("/"));
    base::PathService::Override(base::DIR_OHOS_APP_INSTALLATION, base::FilePath("/"));
    // Initialize command line arguments
    base::CommandLine::Init(0, nullptr);
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    // Turn off the switch
    cmd_line->RemoveSwitch(switches::kBundleInstallationDir);
  }
  void TearDown() override {
    if (base::PathExists(base::FilePath("/data/storage/el2"))) {
      base::DeletePathRecursively(base::FilePath("/data/storage/el2"));
    }
  }
};

TEST_F(OverrideUserDataDirExt, OverrideUserDataDirExt001) {
  base::FilePath crash_path = base::FilePath("/data/storage/el2/base/crash");
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  resource_util::OverrideUserDataDirExt(crash_path, cmd_line);

  base::FilePath actual_crash_path;
  base::PathService::Get(base::DIR_OHOS_CRASHPAD, &actual_crash_path);
  EXPECT_EQ(actual_crash_path, base::FilePath("/data/storage/el2/base/log/crashpad"));

  base::FilePath actual_Installation_path;
  base::PathService::Get(base::DIR_OHOS_APP_INSTALLATION, &actual_Installation_path);
  EXPECT_EQ(actual_Installation_path, base::FilePath("/"));
}

TEST_F(OverrideUserDataDirExt, OverrideUserDataDirExt002) {
  base::FilePath crash_path = base::FilePath("/data/storage/el2/base/crash");
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  base::FilePath build_path = base::FilePath("/data/storage/el2/base/build");
  cmd_line->AppendSwitchPath(switches::kBundleInstallationDir, build_path);
  resource_util::OverrideUserDataDirExt(crash_path, cmd_line);

  base::FilePath actual_crash_path;
  base::PathService::Get(base::DIR_OHOS_CRASHPAD, &actual_crash_path);
  EXPECT_EQ(actual_crash_path, base::FilePath("/data/storage/el2/base/log/crashpad"));

  base::FilePath actual_Installation_path;
  base::PathService::Get(base::DIR_OHOS_APP_INSTALLATION, &actual_Installation_path);
  EXPECT_EQ(actual_Installation_path, base::FilePath("/data/storage/el2/base/build"));
}