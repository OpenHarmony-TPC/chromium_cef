/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#define protected public
#include "libcef/browser/browser_platform_delegate.h"
#undef protected

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#include "base/logging.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

std::shared_ptr<CefBrowserPlatformDelegate> g_browser_platform_delegate;

class CefBrowserPlatformDelegateTest : public testing::Test {
 public:
  static void SetUpTestCase(void);
  static void TearDownTestCase(void);
  void SetUp();
  void TearDown();
};

void CefBrowserPlatformDelegateTest::SetUpTestCase(void) {}

void CefBrowserPlatformDelegateTest::TearDownTestCase(void) {}

void CefBrowserPlatformDelegateTest::SetUp(void) {
  g_browser_platform_delegate = std::make_shared<CefBrowserPlatformDelegate>();
  ASSERT_NE(g_browser_platform_delegate, nullptr);
}

void CefBrowserPlatformDelegateTest::TearDown(void) {
  g_browser_platform_delegate = nullptr;
}

TEST_F(CefBrowserPlatformDelegateTest, WebPageSnapshot) {
  const char* id = "test_id";
  int width = 1024;
  int height = 768;
  bool result = false;

  result = g_browser_platform_delegate->WebPageSnapshot(
      id, width, height,
      base::BindOnce([](const char* str, bool is, void* ptr, int a, int b) {}));
  EXPECT_FALSE(result);
}