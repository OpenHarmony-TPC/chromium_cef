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
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"

namespace resource_util {
extern void OverrideCacheDirExt(const base::FilePath& cache_path);
}  // namespace resource_util

class OverrideCacheDirExtTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base::PathService::Override(base::DIR_CACHE, base::FilePath("/"));
  }
  void TearDown() override {
    if (base::PathExists(base::FilePath("/data/storage/el2"))) {
      base::DeletePathRecursively(base::FilePath("/data/storage/el2"));
    }
  }
};

TEST_F(OverrideCacheDirExtTest, TestCase001) {
  base::FilePath cache_path = base::FilePath();
  resource_util::OverrideCacheDirExt(cache_path);

  base::FilePath actual_cache_path;
  base::PathService::Get(base::DIR_CACHE, &actual_cache_path);
  EXPECT_EQ(actual_cache_path, base::FilePath("/"));
}

TEST_F(OverrideCacheDirExtTest, TestCase002) {
  base::FilePath cache_path = base::FilePath("/data/storage/el2/base/cache/web");
  resource_util::OverrideCacheDirExt(cache_path);

  base::FilePath actual_cache_path;
  base::PathService::Get(base::DIR_CACHE, &actual_cache_path);
  EXPECT_EQ(actual_cache_path, base::FilePath("/data/storage/el2/base/cache/web"));
}

TEST_F(OverrideCacheDirExtTest, TestCase003) {
  base::FilePath test_dir = base::FilePath("/data/storage/el2/base");
  base::FilePath test_file = test_dir.Append("tmp_test");
  base::CreateDirectory(test_dir);
  base::WriteFile(test_file, "test");

  base::FilePath cache_path = base::FilePath("/data/storage/el2/base/tmp_test");
  resource_util::OverrideCacheDirExt(cache_path);

  base::FilePath actual_cache_path;
  base::PathService::Get(base::DIR_CACHE, &actual_cache_path);
  EXPECT_EQ(actual_cache_path, base::FilePath("/"));
}