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

#ifndef CEF_LIBCEF_BROWSER_UI_THREAD_H_
#define CEF_LIBCEF_BROWSER_UI_THREAD_H_
#pragma once

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/main_function_params.h"

class CefMainRunner;

// Used to run the UI on a separate thread.
class CefUIThread final : public base::PlatformThread::Delegate {
 public:
  CefUIThread(CefMainRunner* runner, base::OnceClosure setup_callback);
  ~CefUIThread() override;

  void Start();
  void Stop();

  bool WaitUntilThreadStarted() const;

  void InitializeBrowserRunner(
      content::MainFunctionParams main_function_params);

  void set_shutdown_callback(base::OnceClosure shutdown_callback) {
    shutdown_callback_ = std::move(shutdown_callback);
  }

 private:
  void ThreadMain() override;

  const raw_ptr<CefMainRunner> runner_;
  base::OnceClosure setup_callback_;
  base::OnceClosure shutdown_callback_;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;

  bool stopping_ = false;

  // The thread's handle.
  base::PlatformThreadHandle thread_;
  mutable base::Lock thread_lock_;  // Protects |thread_|.

  mutable base::WaitableEvent start_event_;

  // This class is not thread-safe, use this to verify access from the owning
  // sequence of the Thread.
  base::SequenceChecker owning_sequence_checker_;
};

#endif  // CEF_LIBCEF_BROWSER_UI_THREAD_H_
