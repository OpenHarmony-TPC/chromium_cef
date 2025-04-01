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

#include "libcef_dll/ctocpp/screen_capture_callback_ctocpp.h"
#include "libcef_dll/shutdown_checker.h"

// VIRTUAL METHODS - Body may be edited by hand.

NO_SANITIZE("cfi-icall")
void CefScreenCaptureCallbackCToCpp::OnStateChange(int32_t nweb_id,
                                                   const CefString& session_id,
                                                   int32_t code) {
  shutdown_checker::AssertNotShutdown();

  cef_screen_capture_callback_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, on_state_change)) {
    return;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Verify param: session_id; type: string_byref_const
  DCHECK(!session_id.empty());
  if (session_id.empty()) {
    return;
  }

  // Execute
  _struct->on_state_change(_struct, nweb_id, session_id.GetStruct(), code);
}

// CONSTRUCTOR - Do not edit by hand.

CefScreenCaptureCallbackCToCpp::CefScreenCaptureCallbackCToCpp() {}

// DESTRUCTOR - Do not edit by hand.

CefScreenCaptureCallbackCToCpp::~CefScreenCaptureCallbackCToCpp() {
  shutdown_checker::AssertNotShutdown();
}

template <>
cef_screen_capture_callback_t* CefCToCppRefCounted<
    CefScreenCaptureCallbackCToCpp,
    CefScreenCaptureCallback,
    cef_screen_capture_callback_t>::UnwrapDerived(CefWrapperType type,
                                                  CefScreenCaptureCallback* c) {
  DCHECK(false) << "Unexpected class type: " << type;
  return nullptr;
}

template <>
CefWrapperType
    CefCToCppRefCounted<CefScreenCaptureCallbackCToCpp,
                        CefScreenCaptureCallback,
                        cef_screen_capture_callback_t>::kWrapperType =
        WT_SCREEN_CAPTURE_CALLBACK;