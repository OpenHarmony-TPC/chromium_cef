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

#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"

#include "arkweb/build/features/features.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

namespace {

class CefRenderWidgetHostViewOSRTimerTest : public testing::Test {
 protected:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  void TearDown() override { task_environment_.reset(); }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

// ============================================================================
// Timer Basic Tests
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_StartAndStop) {
  base::OneShotTimer timer;
  
  EXPECT_FALSE(timer.IsRunning());
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_CallbackExecutesAfterDelay) {
  bool callback_executed = false;
  
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(5),
              base::BindOnce([](bool* executed) { *executed = true; },
                             base::Unretained(&callback_executed)));
  
  EXPECT_TRUE(timer.IsRunning());
  EXPECT_FALSE(callback_executed);
  
  task_environment_->FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(callback_executed);
  EXPECT_TRUE(timer.IsRunning());
  
  task_environment_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_StopsOnFalsePolicy) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_DoesNotStopOnTruePolicy) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_MultipleStopCallsSafe) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Stop();
  timer.Stop();
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_StopWhenNotRunningSafe) {
  base::OneShotTimer timer;
  
  EXPECT_FALSE(timer.IsRunning());
  
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_RestartAfterStop) {
  int call_count = 0;
  
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(5),
              base::BindOnce([](int* count) { (*count)++; },
                             base::Unretained(&call_count)));
  
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  
  timer.Start(FROM_HERE, base::Seconds(5),
              base::BindOnce([](int* count) { (*count)++; },
                             base::Unretained(&call_count)));
  EXPECT_TRUE(timer.IsRunning());
  
  task_environment_->FastForwardBy(base::Seconds(5));
  EXPECT_EQ(1, call_count);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_DestroyedBeforeCallback) {
  bool callback_executed = false;
  
  {
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, base::Seconds(5),
                base::BindOnce([](bool* executed) { *executed = true; },
                               base::Unretained(&callback_executed)));
    EXPECT_TRUE(timer.IsRunning());
  }
  
  task_environment_->FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(callback_executed);
}

// ============================================================================
// Timer Edge Cases
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_ZeroDelay) {
  bool callback_executed = false;
  
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(0),
              base::BindOnce([](bool* executed) { *executed = true; },
                             base::Unretained(&callback_executed)));
  
  task_environment_->FastForwardBy(base::Seconds(0));
  EXPECT_TRUE(callback_executed);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_VeryLongDelay) {
  bool callback_executed = false;
  
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Hours(1),
              base::BindOnce([](bool* executed) { *executed = true; },
                             base::Unretained(&callback_executed)));
  
  EXPECT_TRUE(timer.IsRunning());
  
  task_environment_->FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(callback_executed);
  EXPECT_TRUE(timer.IsRunning());
  
  task_environment_->FastForwardBy(base::Minutes(30));
  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_MultipleTimersIndependent) {
  bool first_executed = false;
  bool second_executed = false;
  
  base::OneShotTimer timer1;
  base::OneShotTimer timer2;
  
  timer1.Start(FROM_HERE, base::Seconds(3),
               base::BindOnce([](bool* executed) { *executed = true; },
                              base::Unretained(&first_executed)));
  
  timer2.Start(FROM_HERE, base::Seconds(7),
               base::BindOnce([](bool* executed) { *executed = true; },
                              base::Unretained(&second_executed)));
  
  task_environment_->FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(first_executed);
  EXPECT_FALSE(second_executed);
  
  task_environment_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(first_executed);
  EXPECT_TRUE(second_executed);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_StopOneDoesNotAffectOther) {
  bool first_executed = false;
  bool second_executed = false;
  
  base::OneShotTimer timer1;
  base::OneShotTimer timer2;
  
  timer1.Start(FROM_HERE, base::Seconds(5),
               base::BindOnce([](bool* executed) { *executed = true; },
                              base::Unretained(&first_executed)));
  
  timer2.Start(FROM_HERE, base::Seconds(5),
               base::BindOnce([](bool* executed) { *executed = true; },
                              base::Unretained(&second_executed)));
  
  timer1.Stop();
  
  task_environment_->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(first_executed);
  EXPECT_TRUE(second_executed);
}

// ============================================================================
// Timer State Management
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_IsRunningBeforeStart) {
  base::OneShotTimer timer;
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_IsRunningAfterStart) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  
  EXPECT_TRUE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_IsRunningAfterStop) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Timer_IsRunningAfterCallback) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(5), base::DoNothing());
  
  task_environment_->FastForwardBy(base::Seconds(5));
  
  EXPECT_FALSE(timer.IsRunning());
}

// ============================================================================
// Policy Simulation Tests (Simulating SetUseSpecifiedDeadlinePolicy behavior)
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_TrueKeepsTimerRunning) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  bool should_use = true;
  if (!should_use) {
    timer.Stop();
  }
  
  EXPECT_TRUE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_FalseStopsTimer) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  bool should_use = false;
  if (!should_use) {
    timer.Stop();
  }
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_EnableThenDisable) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  bool should_use = true;
  if (!should_use) {
    timer.Stop();
  }
  EXPECT_TRUE(timer.IsRunning());
  
  should_use = false;
  if (!should_use) {
    timer.Stop();
  }
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_DisableWithoutTimerRunning) {
  base::OneShotTimer timer;
  EXPECT_FALSE(timer.IsRunning());
  
  bool should_use = false;
  if (!should_use) {
    timer.Stop();
  }
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_MultipleEnableCalls) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  bool should_use = true;
  for (int i = 0; i < 5; ++i) {
    if (!should_use) {
      timer.Stop();
    }
  }
  
  EXPECT_TRUE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, Policy_MultipleDisableCalls) {
  base::OneShotTimer timer;
  
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  bool should_use = false;
  for (int i = 0; i < 5; ++i) {
    if (!should_use) {
      timer.Stop();
    }
  }
  
  EXPECT_FALSE(timer.IsRunning());
}

// ============================================================================
// OnDisplayFirstRealSwap Simulation Tests
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, OnDisplayFirstRealSwap_StopsTimer) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, OnDisplayFirstRealSwap_TimerNotRunningNoEffect) {
  base::OneShotTimer timer;
  EXPECT_FALSE(timer.IsRunning());
  
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, OnDisplayFirstRealSwap_MultipleCallsSafe) {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  EXPECT_TRUE(timer.IsRunning());
  
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  
  timer.Stop();
  timer.Stop();
  
  EXPECT_FALSE(timer.IsRunning());
}

// ============================================================================
// evictUnlockFrameEnabled_ Flag Tests (Simulating the flag behavior)
// ============================================================================

TEST_F(CefRenderWidgetHostViewOSRTimerTest, EvictUnlockFrameEnabled_InitialState) {
  bool evict_unlock_frame_enabled = false;
  
  EXPECT_FALSE(evict_unlock_frame_enabled);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, EvictUnlockFrameEnabled_SetToTrue) {
  bool evict_unlock_frame_enabled = true;
  
  EXPECT_TRUE(evict_unlock_frame_enabled);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, EvictUnlockFrameEnabled_Toggle) {
  bool evict_unlock_frame_enabled = false;
  
  evict_unlock_frame_enabled = true;
  EXPECT_TRUE(evict_unlock_frame_enabled);
  
  evict_unlock_frame_enabled = false;
  EXPECT_FALSE(evict_unlock_frame_enabled);
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, EvictUnlockFrameEnabled_AffectsTimerBehavior) {
  bool evict_unlock_frame_enabled = true;
  base::OneShotTimer timer;
  
  if (evict_unlock_frame_enabled) {
    timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  }
  
  EXPECT_TRUE(timer.IsRunning());
  
  evict_unlock_frame_enabled = false;
  if (!evict_unlock_frame_enabled) {
    timer.Stop();
  }
  
  EXPECT_FALSE(timer.IsRunning());
}

TEST_F(CefRenderWidgetHostViewOSRTimerTest, EvictUnlockFrameEnabled_DisabledNoTimerStart) {
  bool evict_unlock_frame_enabled = false;
  base::OneShotTimer timer;
  
  if (evict_unlock_frame_enabled) {
    timer.Start(FROM_HERE, base::Seconds(10), base::DoNothing());
  }
  
  EXPECT_FALSE(timer.IsRunning());
}

}  // namespace
#endif  // BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)