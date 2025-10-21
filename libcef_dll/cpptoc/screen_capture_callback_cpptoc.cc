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

#include "libcef_dll/cpptoc/screen_capture_callback_cpptoc.h"
#include "libcef_dll/shutdown_checker.h"

namespace {

// MEMBER FUNCTIONS - Body may be edited by hand.

void CEF_CALLBACK screen_capture_callback_on_state_change(
    struct _cef_screen_capture_callback_t* self,
    int32_t nweb_id,
    const cef_string_t* session_id,
    int32_t code) {
  shutdown_checker::AssertNotShutdown();

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  DCHECK(self);
  if (!self) {
    return;
  }
  // Verify param: session_id; type: string_byref_const
  DCHECK(session_id);
  if (!session_id) {
    return;
  }

  // Execute
  CefScreenCaptureCallbackCppToC::Get(self)->OnStateChange(nweb_id,
                                                           CefString(session_id),
                                                           code);
}

}  // namespace

// CONSTRUCTOR - Do not edit by hand.

CefScreenCaptureCallbackCppToC::CefScreenCaptureCallbackCppToC() {
  GetStruct()->on_state_change = screen_capture_callback_on_state_change;
}

// DESTRUCTOR - Do not edit by hand.

CefScreenCaptureCallbackCppToC::~CefScreenCaptureCallbackCppToC() {
  shutdown_checker::AssertNotShutdown();
}

template <>
CefRefPtr<CefScreenCaptureCallback> CefCppToCRefCounted<
    CefScreenCaptureCallbackCppToC,
    CefScreenCaptureCallback,
    cef_screen_capture_callback_t>::UnwrapDerived(CefWrapperType type,
                                                  cef_screen_capture_callback_t*
                                                      s) {
  DCHECK(false) << "Unexpected class type: " << type;
  return nullptr;
}

template <>
CefWrapperType
    CefCppToCRefCounted<CefScreenCaptureCallbackCppToC,
                        CefScreenCaptureCallback,
                        cef_screen_capture_callback_t>::kWrapperType =
        WT_SCREEN_CAPTURE_CALLBACK;