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