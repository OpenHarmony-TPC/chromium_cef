/*
 * Copyright (c) 2023-2025 Haitai FangYuan Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tests/shared/browser/main_message_loop_external_pump.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <memory>

#include "include/base/cef_logging.h"
#include "include/cef_app.h"

// From base/posix/eintr_wrapper.h.
// This provides a wrapper around system calls which may be interrupted by a
// signal and return EINTR. See man 7 signal.
// To prevent long-lasting loops (which would likely be a bug, such as a signal
// that should be masked) to go unnoticed, there is a limit after which the
// caller will nonetheless see an EINTR in Debug builds.
#if !defined(HANDLE_EINTR)
#if !DCHECK_IS_ON()

#define HANDLE_EINTR(x)                                     \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#else

#define HANDLE_EINTR(x)                                      \
  ({                                                         \
    int eintr_wrapper_counter = 0;                           \
    decltype(x) eintr_wrapper_result;                        \
    do {                                                     \
      eintr_wrapper_result = (x);                            \
    } while (eintr_wrapper_result == -1 && errno == EINTR && \
             eintr_wrapper_counter++ < 100);                 \
    eintr_wrapper_result;                                    \
  })

#endif  // !DCHECK_IS_ON()
#endif  // !defined(HANDLE_EINTR)

namespace client {

namespace {

class MainMessageLoopExternalPumpOHOS : public MainMessageLoopExternalPump {
 public:
  MainMessageLoopExternalPumpOHOS();
  ~MainMessageLoopExternalPumpOHOS();

  // MainMessageLoopStd methods:
  void Quit() override;
  int Run() override;

  // MainMessageLoopExternalPump methods:
  void OnScheduleMessagePumpWork(int64_t delay_ms) override;

 protected:
  // MainMessageLoopExternalPump methods:
  void SetTimer(int64_t delay_ms) override;
  void KillTimer() override;
  bool IsTimerPending() override;

 private:
  // Used to flag that the Run() invocation should return ASAP.
  bool should_quit_;
};

// Return a timeout suitable for the glib loop, -1 to block forever,
// 0 to return right away, or a timeout in milliseconds from now.
int GetTimeIntervalMilliseconds(const CefTime& from) {
  if (from.GetDoubleT() == 0.0) {
    return -1;
  }

  CefTime now;
  now.Now();

  // Be careful here. CefTime has a precision of microseconds, but we want a
  // value in milliseconds. If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  int delay =
      static_cast<int>(ceil((from.GetDoubleT() - now.GetDoubleT()) * 1000.0));

  // If this value is negative, then we need to run delayed work soon.
  return delay < 0 ? 0 : delay;
}

MainMessageLoopExternalPumpOHOS::MainMessageLoopExternalPumpOHOS()
    : should_quit_(false) {}

MainMessageLoopExternalPumpOHOS::~MainMessageLoopExternalPumpOHOS() {
}

void MainMessageLoopExternalPumpOHOS::Quit() {
  should_quit_ = true;
}

int MainMessageLoopExternalPumpOHOS::Run() {
  // We need to run the message pump until it is idle. However we don't have
  // that information here so we run the message loop "for a while".
  for (int i = 0; i < 10; ++i) {
    // Do some work.
    CefDoMessageLoopWork();

    // Sleep to allow the CEF proc to do work.
    usleep(50000);
  }

  return 0;
}

void MainMessageLoopExternalPumpOHOS::OnScheduleMessagePumpWork(
    int64_t delay_ms) {
}

void MainMessageLoopExternalPumpOHOS::SetTimer(int64_t delay_ms) {
  DCHECK_GT(delay_ms, 0);

}

void MainMessageLoopExternalPumpOHOS::KillTimer() {
}

bool MainMessageLoopExternalPumpOHOS::IsTimerPending() {
  return false;
}

}  // namespace

// static
std::unique_ptr<MainMessageLoopExternalPump>
MainMessageLoopExternalPump::Create() {
  return std::make_unique<MainMessageLoopExternalPumpOHOS>();
}

}  // namespace client
