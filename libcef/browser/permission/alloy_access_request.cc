// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/permission/alloy_access_request.h"

#if BUILDFLAG(IS_OHOS)
#include "base/logging.h"
#include "content/public/browser/media_capture_devices.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#endif // BUILDFLAG(IS_OHOS)

AlloyAccessRequest::AlloyAccessRequest(const CefString& origin,
                                       int resources,
                                       cef_permission_callback_t callback)
    : origin_(origin), resources_(resources), callback_(std::move(callback)) {}

AlloyAccessRequest::~AlloyAccessRequest() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(false);
  }
}

CefString AlloyAccessRequest::Origin() {
  return origin_;
}

int AlloyAccessRequest::ResourceAcessId() {
  return resources_;
}

void AlloyAccessRequest::ReportRequestResult(bool allowed) {
  if (!callback_.is_null()) {
    std::move(callback_).Run(allowed);
  }
}

#if BUILDFLAG(IS_OHOS)
AlloyMediaAccessRequest::AlloyMediaAccessRequest(const content::MediaStreamRequest& request,
                                                 content::MediaResponseCallback callback)
    : request_(request), callback_(std::move(callback)) {}

AlloyMediaAccessRequest::~AlloyMediaAccessRequest() {
  if (!callback_.is_null()) {
    blink::MediaStreamDevices devices;
    std::move(callback_).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
  }
}

CefString AlloyMediaAccessRequest::Origin() {
  return request_.security_origin.spec();
}

int AlloyMediaAccessRequest::ResourceAcessId() {
  return (request_.audio_type ==
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
              ? AlloyAccessRequest::Resources::AUDIO_CAPTURE
              : 0) |
         (request_.video_type ==
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE
              ? AlloyAccessRequest::Resources::VIDEO_CAPTURE
              : 0);
}

void AlloyMediaAccessRequest::ReportRequestResult(bool allowed) {
  blink::MediaStreamDevices devices;
  if (!allowed) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(
          devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::unique_ptr<content::MediaStreamUI>());
    }
    return;
  }
  // Based on chrome/browser/media/media_stream_devices_controller.cc
  if (request_.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request_.requested_audio_device_id, true, false, &devices);
  }

  if (request_.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    LOG(INFO) << "RequestMediaAccessPermission requested_video_device_id: " <<
        request_.requested_video_device_id;
    CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
        request_.requested_video_device_id, false, true, &devices);
  }

  if (!callback_.is_null()) {
    std::move(callback_).Run(
        devices,
        devices.empty() ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                        : blink::mojom::MediaStreamRequestResult::OK,
        std::unique_ptr<content::MediaStreamUI>());
  }
}

AlloyScreenCaptureAccessRequest::AlloyScreenCaptureAccessRequest(const content::MediaStreamRequest& request,
                                                                 content::MediaResponseCallback callback)
    : request_(request), callback_(std::move(callback)), mode_(cef_screen_capture_mode_t::CAPTURE_INVAILD_MODE), sourceId_(-1) {}

AlloyScreenCaptureAccessRequest::~AlloyScreenCaptureAccessRequest() {
  if (!callback_.is_null()) {
    blink::MediaStreamDevices devices;
    std::move(callback_).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
  }
}

CefString AlloyScreenCaptureAccessRequest::Origin() {
  return request_.security_origin.spec();
}

void AlloyScreenCaptureAccessRequest::SetCaptureMode(int32_t mode) {
  mode_ = mode;
}

void AlloyScreenCaptureAccessRequest::SetCaptureSourceId(int32_t sourceId) {
  sourceId_ = sourceId;
}

void AlloyScreenCaptureAccessRequest::ReportRequestResult(bool allowed) {
  blink::MediaStreamDevices devices;
  if (!allowed) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(
          devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::unique_ptr<content::MediaStreamUI>());
    }
    return;
  }
  // Based on chrome/browser/media/media_stream_devices_controller.cc
  if (request_.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request_.requested_audio_device_id, true, false, &devices);
  }

  content::DesktopMediaID media_id;
  if (request_.requested_video_device_id.empty()) {
    if (mode_ == cef_screen_capture_mode_t::CAPTURE_HOME_SCREEN_MODE) {
      media_id =
        content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                -1 /* webrtc::kFullDesktopScreenId */);
    } else if (mode_ == cef_screen_capture_mode_t::CAPTURE_SPECIFIED_SCREEN_MODE) {
      media_id =
        content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, sourceId_);
    } else if (mode_ == cef_screen_capture_mode_t::CAPTURE_SPECIFIED_WINDOW_MODE) {
      media_id =
        content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, sourceId_);
    }
  } else {
    media_id =
        content::DesktopMediaID::Parse(request_.requested_video_device_id);
  }
  devices.push_back(blink::MediaStreamDevice(request_.video_type, media_id.ToString(), "Screen"));

  if (!callback_.is_null()) {
    std::move(callback_).Run(
        devices,
        devices.empty() ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                        : blink::mojom::MediaStreamRequestResult::OK,
        std::unique_ptr<content::MediaStreamUI>());
  }
}
#endif // BUILDFLAG(IS_OHOS)