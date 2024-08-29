// Copyright (c) 2024 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "motion_event_osr.h"
#include <cstddef>
#include "cef/include/internal/cef_types.h"
#include "cef/include/internal/cef_types_wrappers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef {
class MockCefMotionEventOSR : public CefMotionEventOSR {};

class CefMotionEventOSRTest : public ::testing::Test {
 public:
  MockCefMotionEventOSR motionEventOSR;
};

TEST_F(CefMotionEventOSRTest, TestOnTouch) {
  CefTouchEvent touchEvent;
  touchEvent.id = 1;
  touchEvent.type = CEF_TET_MOVED;
  touchEvent.x = 100;
  touchEvent.y = 200;
  touchEvent.modifiers = 0;

  bool result = motionEventOSR.OnTouch(touchEvent);

  EXPECT_FALSE(result);
}
}  // namespace cef
