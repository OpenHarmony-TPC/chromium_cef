// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PERMISSION_ALLOY_ACCESS_REQUEST_H_
#define CEF_LIBCEF_BROWSER_PERMISSION_ALLOY_ACCESS_REQUEST_H_

#include <memory>

#include "include/cef_permission_request.h"

// This class is used to handle the permission request which just needs
// a callback with bool parameter to indicate the permission granted or not. It
// works with PermissionRequestHandler. The specific permission request should
// implement the CefAccessRequest.
class AlloyAccessRequest : public CefAccessRequest {
 public:
  AlloyAccessRequest(const AlloyAccessRequest&) = delete;
  AlloyAccessRequest& operator=(const AlloyAccessRequest&) = delete;

  enum Resources {
    GEOLOCATION = 1 << 0,
    VIDEO_CAPTURE = 1 << 1,
    AUDIO_CAPTURE = 1 << 2,
    PROTECTED_MEDIA_ID = 1 << 3,
    MIDI_SYSEX = 1 << 4,
  };

  AlloyAccessRequest(const CefString& origin,
                     int resources,
                     cef_permission_callback_t callback);
  ~AlloyAccessRequest() override;

  CefString Origin() override;

  int ResourceAcessId() override;

  void ReportRequestResult(bool allowed) override;

 private:
  CefString origin_;
  int resources_;
  cef_permission_callback_t callback_;

  IMPLEMENT_REFCOUNTING(AlloyAccessRequest);
};

#endif  // CEF_LIBCEF_BROWSER_PERMISSION_ALLOY_ACCESS_REQUEST_H_
