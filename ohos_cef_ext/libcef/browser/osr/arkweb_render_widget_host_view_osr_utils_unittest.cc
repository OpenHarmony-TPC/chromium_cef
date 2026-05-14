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

#include "arkweb/build/features/features.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)

#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_utils.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
#include "arkweb/chromium_ext/ui/ohos/overscroll_refresh_handler.h"
#endif

namespace {

class ArkWebRenderWidgetHostViewOSRUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  void TearDown() override { task_environment_.reset(); }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

// ============================================================================
// ConvertOrientationType Tests (Static Method)
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_Undefined) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      cef_screen_orientation_type_t::UNDEFINED);
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kUndefined);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_PortraitPrimary) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      cef_screen_orientation_type_t::PORTRAIT_PRIMARY);
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kPortraitPrimary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_LandscapePrimary) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      cef_screen_orientation_type_t::LANDSCAPE_PRIMARY);
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kLandscapePrimary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_PortraitSecondary) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      cef_screen_orientation_type_t::PORTRAIT_SECONDARY);
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kPortraitSecondary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_LandscapeSecondary) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      cef_screen_orientation_type_t::LANDSCAPE_SECONDARY);
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kLandscapeSecondary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, ConvertOrientationType_InvalidValue) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::ConvertOrientationType(
      static_cast<cef_screen_orientation_type_t>(999));
  EXPECT_EQ(result, display::mojom::ScreenOrientation::kUndefined);
}

// ============================================================================
// TruncateTooltipText Tests (Static Method)
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, TruncateTooltipText_ShortText) {
  std::u16string short_text = u"Hello World";
  auto result = ArkWebRenderWidgetHostViewOSRUtils::TruncateTooltipText(short_text);
  EXPECT_EQ(result, short_text);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, TruncateTooltipText_EmptyText) {
  std::u16string empty_text = u"";
  auto result = ArkWebRenderWidgetHostViewOSRUtils::TruncateTooltipText(empty_text);
  EXPECT_EQ(result, empty_text);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, TruncateTooltipText_LongText) {
  std::u16string long_text;
  for (int i = 0; i < 2000; ++i) {
    long_text += u"a";
  }
  auto result = ArkWebRenderWidgetHostViewOSRUtils::TruncateTooltipText(long_text);
  EXPECT_LT(result.length(), long_text.length());
  EXPECT_LE(result.length(), 1024u);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, TruncateTooltipText_ExactlyMaxLength) {
  std::u16string exact_text;
  for (int i = 0; i < 1024; ++i) {
    exact_text += u"a";
  }
  auto result = ArkWebRenderWidgetHostViewOSRUtils::TruncateTooltipText(exact_text);
  EXPECT_EQ(result.length(), 1024u);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, TruncateTooltipText_UnicodeCharacters) {
  std::u16string unicode_text = u"中文测试日本語テスト";
  auto result = ArkWebRenderWidgetHostViewOSRUtils::TruncateTooltipText(unicode_text);
  EXPECT_EQ(result, unicode_text);
}

// ============================================================================
// UpdateScreenInfoForArkweb Tests (Static Method)
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, UpdateScreenInfoForArkweb_Basic) {
  display::ScreenInfo screenInfo;
  CefScreenInfo src;
  src.angle = 90;
  src.orientation = cef_screen_orientation_type_t::PORTRAIT_PRIMARY;
  
  ArkWebRenderWidgetHostViewOSRUtils::UpdateScreenInfoForArkweb(screenInfo, src);
  
  EXPECT_EQ(screenInfo.orientation_angle, 90);
  EXPECT_EQ(screenInfo.orientation_type, display::mojom::ScreenOrientation::kPortraitPrimary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, UpdateScreenInfoForArkweb_ZeroAngle) {
  display::ScreenInfo screenInfo;
  CefScreenInfo src;
  src.angle = 0;
  src.orientation = cef_screen_orientation_type_t::LANDSCAPE_PRIMARY;
  
  ArkWebRenderWidgetHostViewOSRUtils::UpdateScreenInfoForArkweb(screenInfo, src);
  
  EXPECT_EQ(screenInfo.orientation_angle, 0);
  EXPECT_EQ(screenInfo.orientation_type, display::mojom::ScreenOrientation::kLandscapePrimary);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, UpdateScreenInfoForArkweb_UndefinedOrientation) {
  display::ScreenInfo screenInfo;
  CefScreenInfo src;
  src.angle = 180;
  src.orientation = cef_screen_orientation_type_t::UNDEFINED;
  
  ArkWebRenderWidgetHostViewOSRUtils::UpdateScreenInfoForArkweb(screenInfo, src);
  
  EXPECT_EQ(screenInfo.orientation_angle, 180);
  EXPECT_EQ(screenInfo.orientation_type, display::mojom::ScreenOrientation::kUndefined);
}

// ============================================================================
// GetCompositor Tests
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositor_WidgetNotFound) {
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  
  ui::Compositor* result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget);
  
  EXPECT_EQ(nullptr, result);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositor_ReturnsNullForNewWidget) {
  gfx::AcceleratedWidget widget = 12345;
  
  ui::Compositor* result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget);
  
  EXPECT_EQ(nullptr, result);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositor_MultipleWidgets) {
  gfx::AcceleratedWidget widget1 = 400;
  gfx::AcceleratedWidget widget2 = 500;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget1, nullptr);
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget2, nullptr);
  
  ui::Compositor* result1 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget1);
  ui::Compositor* result2 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget2);
  
  EXPECT_EQ(nullptr, result1);
  EXPECT_EQ(nullptr, result2);
}

// ============================================================================
// GetCompositorData Tests
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositorData_WidgetNotFound) {
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  
  auto result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  
  EXPECT_EQ(nullptr, result.first);
  EXPECT_EQ(nullptr, result.second);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositorData_ReturnsNullForNewWidget) {
  gfx::AcceleratedWidget widget = 99999;
  
  auto result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  
  EXPECT_EQ(nullptr, result.first);
  EXPECT_EQ(nullptr, result.second);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositorData_ReturnsValidAllocator) {
  gfx::AcceleratedWidget widget = 100;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget, nullptr);
  
  auto result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  
  EXPECT_EQ(nullptr, result.first);
  EXPECT_NE(nullptr, result.second);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetCompositorData_ConsistentCalls) {
  gfx::AcceleratedWidget widget = 300;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget, nullptr);
  
  auto result1 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  auto result2 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  
  EXPECT_EQ(result1.first, result2.first);
  EXPECT_EQ(result1.second, result2.second);
}

// ============================================================================
// AddCompositor Tests
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, AddCompositor_GetCompositorReturnsNullWhenCompositorIsNull) {
  gfx::AcceleratedWidget widget = 12345;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget, nullptr);
  
  ui::Compositor* result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget);
  
  EXPECT_EQ(nullptr, result);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, AddCompositor_MultipleWidgets) {
  gfx::AcceleratedWidget widget1 = 100;
  gfx::AcceleratedWidget widget2 = 200;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget1, nullptr);
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget2, nullptr);
  
  auto result1 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget1);
  auto result2 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget2);
  
  EXPECT_NE(nullptr, result1.second);
  EXPECT_NE(nullptr, result2.second);
  EXPECT_NE(result1.second, result2.second);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, AddCompositor_IntegrationWithGetCompositorData) {
  gfx::AcceleratedWidget widget = 600;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget, nullptr);
  
  auto compositor_result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositor(widget);
  auto data_result = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget);
  
  EXPECT_EQ(nullptr, compositor_result);
  EXPECT_EQ(nullptr, data_result.first);
  EXPECT_NE(nullptr, data_result.second);
}

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, AddCompositor_DifferentWidgetIds) {
  gfx::AcceleratedWidget widget1 = 700;
  gfx::AcceleratedWidget widget2 = 800;
  
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget1, nullptr);
  ArkWebRenderWidgetHostViewOSRUtils::AddCompositor(widget2, nullptr);
  
  auto result1 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget1);
  auto result2 = ArkWebRenderWidgetHostViewOSRUtils::GetCompositorData(widget2);
  
  EXPECT_NE(nullptr, result1.second);
  EXPECT_NE(nullptr, result2.second);
  EXPECT_NE(result1.second, result2.second);
}

// ============================================================================
// IsRenderWidgetHostViewForActiveMainFrame Tests (requires view context)
// Note: This method requires a valid view_ pointer, so we test edge cases only
// ============================================================================

TEST_F(ArkWebRenderWidgetHostViewOSRUtilsTest, GetContextFactory_ReturnsNonNull) {
  auto result = ArkWebRenderWidgetHostViewOSRUtils::GetContextFactory();
  EXPECT_NE(nullptr, result);
}

}  // namespace
#endif  // BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)