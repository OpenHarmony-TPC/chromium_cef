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

#ifndef CEF_LIBCEF_DLL_CPPTOC_SCREEN_CAPTURE_CALLBACK_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_SCREEN_CAPTURE_CALLBACK_CPPTOC_H_
#pragma once

#if !defined(WRAPPING_CEF_SHARED)
#error This file can be included wrapper-side only
#endif

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "libcef_dll/cpptoc/cpptoc_ref_counted.h"

// Wrap a C++ class with a C structure.
// This class may be instantiated and accessed wrapper-side only.
class CefScreenCaptureCallbackCppToC
    : public CefCppToCRefCounted<CefScreenCaptureCallbackCppToC,
                                 CefScreenCaptureCallback,
                                 cef_screen_capture_callback_t> {
public:
  CefScreenCaptureCallbackCppToC();
  virtual ~CefScreenCaptureCallbackCppToC();
};

#endif  // CEF_LIBCEF_DLL_CPPTOC_SCREEN_CAPTURE_CALLBACK_CPPTOC_H_