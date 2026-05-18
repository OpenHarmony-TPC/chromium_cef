/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "cef/libcef/browser/osr/host_display_client_osr.h"

#include "arkweb/build/features/features.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace {

class CefHostDisplayClientOSRTest : public testing::Test {
 public:
  void IncrementCallCount() { call_count_++; }
  void IncrementFirstCount() { first_count_++; }
  void IncrementSecondCount() { second_count_++; }

 protected:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    call_count_ = 0;
    first_count_ = 0;
    second_count_ = 0;
  }

  void TearDown() override { task_environment_.reset(); }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  int call_count_ = 0;
  int first_count_ = 0;
  int second_count_ = 0;
};

// ============================================================================
// NotifyFirstRealSwapBuffer Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, NotifyFirstRealSwapBuffer_CallsListenerWhenSet) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));

  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(call_count_, 1);
}

TEST_F(CefHostDisplayClientOSRTest, NotifyFirstRealSwapBuffer_NoCallWhenListenerNotSet) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(call_count_, 0);
}

TEST_F(CefHostDisplayClientOSRTest, NotifyFirstRealSwapBuffer_MultipleCalls) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));

  client.NotifyFirstRealSwapBuffer();
  client.NotifyFirstRealSwapBuffer();
  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(call_count_, 3);
}

// ============================================================================
// AddFirstRealSwapBufferListener Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, AddFirstRealSwapBufferListener_StoresCallback) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));

  client.NotifyFirstRealSwapBuffer();
  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(call_count_, 2);
}

TEST_F(CefHostDisplayClientOSRTest, AddFirstRealSwapBufferListener_CanReplaceExisting) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementFirstCount,
                           base::Unretained(this)));

  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementSecondCount,
                           base::Unretained(this)));

  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(first_count_, 0);
  EXPECT_EQ(second_count_, 1);
}

TEST_F(CefHostDisplayClientOSRTest, AddFirstRealSwapBufferListener_NullCallback) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(base::RepeatingCallback<void()>());

  client.NotifyFirstRealSwapBuffer();
  
  EXPECT_EQ(call_count_, 0);
}

TEST_F(CefHostDisplayClientOSRTest, AddFirstRealSwapBufferListener_ReplaceWithNullCallback) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));

  client.AddFirstRealSwapBufferListener(base::RepeatingCallback<void()>());

  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(call_count_, 0);
}

TEST_F(CefHostDisplayClientOSRTest, AddFirstRealSwapBufferListener_ReplaceTwice) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementFirstCount,
                           base::Unretained(this)));
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementSecondCount,
                           base::Unretained(this)));
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));

  client.NotifyFirstRealSwapBuffer();

  EXPECT_EQ(first_count_, 0);
  EXPECT_EQ(second_count_, 0);
  EXPECT_EQ(call_count_, 1);
}

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, Constructor_NullView) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  EXPECT_EQ(client.GetPixelMemory(), nullptr);
  EXPECT_EQ(client.GetPixelSize(), gfx::Size());
}

TEST_F(CefHostDisplayClientOSRTest, Constructor_WidgetValue) {
  gfx::AcceleratedWidget widget = 12345;
  CefHostDisplayClientOSR client(nullptr, widget);
  
  EXPECT_EQ(client.GetPixelMemory(), nullptr);
  EXPECT_EQ(client.GetPixelSize(), gfx::Size());
}

// ============================================================================
// SetActive Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, SetActive_ToTrue) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.SetActive(true);
  
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  EXPECT_EQ(gfx::Size(0, 0), client.GetPixelSize());
}

TEST_F(CefHostDisplayClientOSRTest, SetActive_ToFalse) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.SetActive(false);
  
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  EXPECT_EQ(gfx::Size(0, 0), client.GetPixelSize());
}

TEST_F(CefHostDisplayClientOSRTest, SetActive_MultipleCalls) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.SetActive(true);
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  
  client.SetActive(false);
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  
  client.SetActive(true);
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  
  client.SetActive(false);
  EXPECT_EQ(nullptr, client.GetPixelMemory());
  EXPECT_EQ(gfx::Size(0, 0), client.GetPixelSize());
}

// ============================================================================
// GetPixelMemory Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, GetPixelMemory_ReturnsNullWhenNoLayeredWindowUpdater) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  const void* memory = client.GetPixelMemory();
  
  EXPECT_EQ(nullptr, memory);
}

// ============================================================================
// GetPixelSize Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, GetPixelSize_ReturnsEmptyWhenNoLayeredWindowUpdater) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  gfx::Size size = client.GetPixelSize();
  
  EXPECT_EQ(size, gfx::Size());
}

TEST_F(CefHostDisplayClientOSRTest, GetPixelSize_ReturnsZeroSize) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  gfx::Size size = client.GetPixelSize();
  
  EXPECT_EQ(size.width(), 0);
  EXPECT_EQ(size.height(), 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, Integration_SetActiveAndNotify) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));
  
  client.SetActive(true);
  client.NotifyFirstRealSwapBuffer();
  
  EXPECT_EQ(call_count_, 1);
}

TEST_F(CefHostDisplayClientOSRTest, Integration_SetActiveFalseBeforeNotify) {
  CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
  
  client.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementCallCount,
                           base::Unretained(this)));
  
  client.SetActive(true);
  client.SetActive(false);
  client.NotifyFirstRealSwapBuffer();
  
  EXPECT_EQ(call_count_, 1);
}

TEST_F(CefHostDisplayClientOSRTest, Integration_MultipleWidgets) {
  CefHostDisplayClientOSR client1(nullptr, 100);
  CefHostDisplayClientOSR client2(nullptr, 200);
  
  client1.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementFirstCount,
                           base::Unretained(this)));
  
  client2.AddFirstRealSwapBufferListener(
      base::BindRepeating(&CefHostDisplayClientOSRTest::IncrementSecondCount,
                           base::Unretained(this)));
  
  client1.NotifyFirstRealSwapBuffer();
  client2.NotifyFirstRealSwapBuffer();
  
  EXPECT_EQ(first_count_, 1);
  EXPECT_EQ(second_count_, 1);
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(CefHostDisplayClientOSRTest, Destructor_CleansUpListener) {
  int local_count = 0;
  
  {
    CefHostDisplayClientOSR client(nullptr, gfx::kNullAcceleratedWidget);
    client.AddFirstRealSwapBufferListener(
        base::BindRepeating([](int* count) { (*count)++; },
                             base::Unretained(&local_count)));
    client.NotifyFirstRealSwapBuffer();
    EXPECT_EQ(local_count, 1);
  }
  
  EXPECT_EQ(local_count, 1);
}

}  // namespace
#endif  // BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)