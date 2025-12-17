/*
 * Copyright (c) 2025-2025 Huawei Device Co., Ltd.
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

#include "ohos_cef_ext/libcef/browser/arkweb_global_list_config.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace fallback_proxy;

// A mock provider that allows us to capture pref observer changes.
class MockPrefService : public TestingPrefServiceSimple {
 public:
  MockPrefService() {}
  ~MockPrefService() override {}
  PrefRegistrySimple* GetRegistry() { return registry(); }
};

class MockListChangeObserver
    : public HwGlobalListConfig::GlobalListConfigObserver {
 public:
  MockListChangeObserver() {}
  int GetCallCount() { return call_count; }
  void OnInterestedConfigChanged() override;
  ~MockListChangeObserver() override {}

 private:
  int call_count = 0;
};

void MockListChangeObserver::OnInterestedConfigChanged() {
  call_count++;
  LOG(INFO) << __func__ << call_count;
}

class HwGlobalListConfigTest : public testing::Test {
 public:
  HwGlobalListConfigTest() {}
  ~HwGlobalListConfigTest() override {}
  MockListChangeObserver block_list_observer;
  MockListChangeObserver test_observer;
  base::FilePath version1_path;
  base::FilePath version2_path;
  base::FilePath version3_path;
  base::FilePath version4_path;
  base::FilePath version5_path;
  base::ScopedTempDir test_file_dir;

 protected:
  void SetUp() override;
  MockPrefService* service() const { return service_.get(); }
  PrefRegistrySimple* registry() const { return service_.get()->GetRegistry(); }
  base::test::TaskEnvironment task_environment_;
  void ExectuteConfigUntilIdle(HwGlobalListConfig* config,
                               const base::FilePath& config_path,
                               std::string& version);

 private:
  std::unique_ptr<MockPrefService> service_;
};

void HwGlobalListConfigTest::ExectuteConfigUntilIdle(
    HwGlobalListConfig* config,
    const base::FilePath& config_path,
    std::string& version) {
  config->SetGlobalListConfigPath(config_path, version);
  task_environment_.RunUntilIdle();
}

void HwGlobalListConfigTest::SetUp() {
  service_ = std::make_unique<MockPrefService>();
  base::FilePath root("/sdcard/Download");
  base::FilePath test_dir = root.Append("hw_global_list_config");

  if (test_file_dir.CreateUniqueTempDirUnderPath(test_dir)) {
    version1_path = test_file_dir.GetPath().AppendASCII("config1.json");
    version2_path = test_file_dir.GetPath().AppendASCII("config2.json");
    version3_path = test_file_dir.GetPath().AppendASCII("config3.json");
    version4_path = test_file_dir.GetPath().AppendASCII("config4.json");
    version5_path = test_file_dir.GetPath().AppendASCII("config5.json");
    std::string file_content_1 =
        "{\"fallbackProxyBlockList\":[\"a.com\",\"b.com\"],"
        "\"test\":[\"c.com\",\"d.com\"]}";
    std::string file_content_2 =
        "{\"fallbackProxyBlockList\":[\"a.com\",\"e.com\"],"
        "\"test\":[\"c.com\",\"d.com\"]}";
    std::string file_content_3 =
        "{\"fallbackProxyBlockList\":[],"
        "\"test\":[\"a.com\",\"e.com\"]}";
    std::string file_content_4 = "{\"test\":[\"a.com\",\"e.com\"]}";
    std::string file_content_5 =
        "{\"fallbackProxyBlockList\":\"tyeperror\","
        "\"test\":[\"a.com\",\"e.com\"]}";
    WriteFile(version1_path, file_content_1);
    WriteFile(version2_path, file_content_2);
    WriteFile(version3_path, file_content_3);
    WriteFile(version4_path, file_content_4);
    WriteFile(version5_path, file_content_5);
  }
}

TEST_F(HwGlobalListConfigTest, TestBlockListGet) {
  HwGlobalListConfig* config = new HwGlobalListConfig();
  config->Init(service());
  HwGlobalListConfig::RegisterPrefs(registry());
  registry()->RegisterListPref("test");

  std::string version = "1.0";
  ExectuteConfigUntilIdle(config, version1_path, version);
  std::vector<std::string> expected_value;
  expected_value.push_back("a.com");
  expected_value.push_back("b.com");
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());

  version = "2.0";
  expected_value.clear();
  expected_value.push_back("a.com");
  expected_value.push_back("e.com");
  ExectuteConfigUntilIdle(config, version2_path, version);
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());

  // test same version, keep the content same with last set
  ExectuteConfigUntilIdle(config, version3_path, version);
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());

  // test empty the list
  version = "3.0";
  ExectuteConfigUntilIdle(config, version3_path, version);
  expected_value.clear();
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());

  // test set value emtpy if the required key-value item missing in config
  version = "2.0";
  ExectuteConfigUntilIdle(config, version2_path, version);
  EXPECT_EQ(2, config->GetFallbackProxyBlockList().size());
  expected_value.clear();
  version = "4.0";
  ExectuteConfigUntilIdle(config, version4_path, version);
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());

  // test set value emtpy when type of value abnormal for a key
  version = "2.0";
  ExectuteConfigUntilIdle(config, version2_path, version);
  EXPECT_EQ(2, config->GetFallbackProxyBlockList().size());
  version = "5.0";
  ExectuteConfigUntilIdle(config, version5_path, version);
  expected_value.clear();
  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());
  delete config;
}

TEST_F(HwGlobalListConfigTest, TestInit) {
  HwGlobalListConfig* config = new HwGlobalListConfig();
  config->Init(service());
  HwGlobalListConfig::RegisterPrefs(registry());
  registry()->RegisterListPref("test");

  std::string version = "1.0";
  ExectuteConfigUntilIdle(config, version1_path, version);
  std::vector<std::string> expected_value;
  expected_value.push_back("a.com");
  expected_value.push_back("b.com");

  EXPECT_EQ(expected_value, config->GetFallbackProxyBlockList());
  delete config;
}

TEST_F(HwGlobalListConfigTest, TestWithoutInit) {
  HwGlobalListConfig* config = new HwGlobalListConfig();
  HwGlobalListConfig::RegisterPrefs(registry());
  registry()->RegisterListPref("test");

  std::string version = "1.0";
  config->AddConfigChangeObserver("fallbackProxyBlockList",
                                  &block_list_observer);
  ExectuteConfigUntilIdle(config, version1_path, version);
  EXPECT_EQ(config->GetFallbackProxyBlockList().size(), 0);
  EXPECT_EQ(0, block_list_observer.GetCallCount());
  delete config;
}

TEST_F(HwGlobalListConfigTest, TestObserverChange) {
  HwGlobalListConfig* config = new HwGlobalListConfig();
  config->Init(service());
  HwGlobalListConfig::RegisterPrefs(registry());
  registry()->RegisterListPref("test");

  config->AddConfigChangeObserver("fallbackProxyBlockList",
                                  &block_list_observer);

  config->AddConfigChangeObserver("test", &test_observer);

  std::string version = "2.0";
  ExectuteConfigUntilIdle(config, version1_path, version);
  EXPECT_EQ(1, block_list_observer.GetCallCount());
  EXPECT_EQ(1, test_observer.GetCallCount());

  ExectuteConfigUntilIdle(config, version2_path, version);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, block_list_observer.GetCallCount());
  EXPECT_EQ(1, test_observer.GetCallCount());

  version = "3.0";
  ExectuteConfigUntilIdle(config, version2_path, version);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, block_list_observer.GetCallCount());
  EXPECT_EQ(1, test_observer.GetCallCount());

  delete config;
}

TEST_F(HwGlobalListConfigTest, TestObserverRemove) {
  HwGlobalListConfig* config = new HwGlobalListConfig();
  config->Init(service());
  HwGlobalListConfig::RegisterPrefs(registry());
  registry()->RegisterListPref("test");

  config->AddConfigChangeObserver("fallbackProxyBlockList",
                                  &block_list_observer);
  config->AddConfigChangeObserver("test", &test_observer);

  std::string version = "2.0";
  ExectuteConfigUntilIdle(config, version1_path, version);
  EXPECT_EQ(1, block_list_observer.GetCallCount());
  EXPECT_EQ(1, test_observer.GetCallCount());
  config->RemoveConfigChangeObservers("fallbackProxyBlockList");
  version = "3.0";
  ExectuteConfigUntilIdle(config, version2_path, version);
  EXPECT_EQ(1, block_list_observer.GetCallCount());
  EXPECT_EQ(1, test_observer.GetCallCount());

  // test duplicate remove no crash
  version = "4.0";
  config->RemoveConfigChangeObservers("fallbackProxyBlockList");
  ExectuteConfigUntilIdle(config, version1_path, version);
  EXPECT_EQ(1, block_list_observer.GetCallCount());

  delete config;
}
