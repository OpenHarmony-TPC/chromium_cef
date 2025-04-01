// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/permission/alloy_access_request.h"

#include "libcef/browser/permission/alloy_permission_request_handler.h"

#if BUILDFLAG(ARKWEB_WEBRTC)
#include "base/logging.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/media_capture_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

AlloyAccessRequest::AlloyAccessRequest(const CefString& origin,
                                       int resources,
                                       cef_permission_callback_t callback)
    : origin_(origin), resources_(resources), callback_(std::move(callback)) {}

AlloyAccessRequest::~AlloyAccessRequest() {
#if BUILDFLAG(IS_OHOS)
  if (!callback_.is_null()) {
    std::move(callback_).Run(false);
  }
#else
  std::move(callback_).Run(false);
#endif
}

CefString AlloyAccessRequest::Origin() {
  return origin_;
}

int AlloyAccessRequest::ResourceAcessId() {
  return resources_;
}

void AlloyAccessRequest::ReportRequestResult(bool allowed) {
#if BUILDFLAG(IS_OHOS)
  if (!callback_.is_null()) {
    std::move(callback_).Run(allowed);
  }
#else
  std::move(callback_).Run(allowed);
#endif
}

#if BUILDFLAG(ARKWEB_SENSOR)
AlloySensorAccessRequest::AlloySensorAccessRequest(
    CefBrowserHostBase* const browser,
    const CefString& origin,
    cef_permission_callback_t callback)
    : browser_(browser), origin_(origin), callback_(std::move(callback)) {}

AlloySensorAccessRequest::~AlloySensorAccessRequest() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(false);
  }
}

CefString AlloySensorAccessRequest::Origin() {
  return origin_;
}

int AlloySensorAccessRequest::ResourceAcessId() {
  return AlloyAccessRequest::Resources::SENSORS;
}

void AlloySensorAccessRequest::ReportRequestResult(bool allowed) {
  if (callback_.is_null()) {
    LOG(ERROR) << "ReportRequestResult callback is null.";
    return;
  }
  LOG(INFO) << "ReportRequestResult, permission status: " << allowed;
  if (browser_) {
    AlloyPermissionRequestHandler* permission_handler =
        browser_->GetPermissionRequestHandler();
    if (permission_handler) {
      permission_handler->SetSensorPermission(origin_, allowed);
    }
  }
  std::move(callback_).Run(allowed);
}
#endif  // BUILDFLAG(ARKWEB_SENSOR)

#if BUILDFLAG(ARKWEB_WEBRTC)
AlloyMediaAccessRequest::AlloyMediaAccessRequest(
    CefBrowserHostBase* const browser,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback)
    : browser_(browser), request_(request), callback_(std::move(callback)) {}

AlloyMediaAccessRequest::~AlloyMediaAccessRequest() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
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
  if (!allowed) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::unique_ptr<content::MediaStreamUI>());
    }
    return;
  }

  if (callback_.is_null()) {
    return;
  }

  blink::mojom::StreamDevicesSet devices_set;
  devices_set.stream_devices.emplace_back(blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices = *devices_set.stream_devices[0];

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  blink::mojom::MediaStreamRequestResult result =
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE;

  if (request_.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    blink::MediaStreamDevices devices =
        MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
    if (!devices.empty()) {
      stream_devices.audio_device = devices[0];
      result = blink::mojom::MediaStreamRequestResult::OK;
    }
  }

  if (request_.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    if (!request_.requested_video_device_ids.empty()) {
      LOG(INFO) << "RequestMediaAccessPermission requested_video_device_id: "
                << request_.requested_video_device_ids.front();
    }
    blink::MediaStreamDevices devices =
        MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();
    if (!devices.empty()) {
      stream_devices.video_device = devices[0];
      result = blink::mojom::MediaStreamRequestResult::OK;
    }
  }
  bool has_video = stream_devices.video_device.has_value();
  bool has_audio = stream_devices.audio_device.has_value();
  auto media_stream_ui =
      browser_->GetMediaStreamRegistrar()->MaybeCreateMediaStreamUI(has_video,
                                                                    has_audio);
  std::move(callback_).Run(devices_set, result, std::move(media_stream_ui));
}

AlloyScreenCaptureAccessRequest::AlloyScreenCaptureAccessRequest(
    CefBrowserHostBase* const browser,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback)
    : browser_(browser),
      request_(request),
      callback_(std::move(callback)),
      mode_(cef_screen_capture_mode_t::CAPTURE_INVAILD_MODE),
      sourceId_(-1) {}

AlloyScreenCaptureAccessRequest::~AlloyScreenCaptureAccessRequest() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
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
  if (!allowed) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::unique_ptr<content::MediaStreamUI>());
    }
    return;
  }

  if (callback_.is_null()) {
    return;
  }

  blink::mojom::StreamDevicesSet devices_set;
  devices_set.stream_devices.emplace_back(blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices = *devices_set.stream_devices[0];

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  if (request_.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    blink::MediaStreamDevices devices =
        MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
    if (!devices.empty()) {
      stream_devices.audio_device = devices[0];
    }
  }

  content::DesktopMediaID media_id;
  if (request_.requested_video_device_ids.empty() ||
      request_.requested_video_device_ids.front().empty()) {
    if (mode_ == cef_screen_capture_mode_t::CAPTURE_HOME_SCREEN_MODE) {
      media_id = content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                         -1 /* webrtc::kFullDesktopScreenId */);
    } else if (mode_ ==
               cef_screen_capture_mode_t::CAPTURE_SPECIFIED_SCREEN_MODE) {
      media_id = content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                         sourceId_);
    } else if (mode_ ==
               cef_screen_capture_mode_t::CAPTURE_SPECIFIED_WINDOW_MODE) {
      media_id = content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW,
                                         sourceId_);
    }
  } else {
    media_id = content::DesktopMediaID::Parse(
        request_.requested_video_device_ids.front());
  }
  stream_devices.video_device = blink::MediaStreamDevice(
      request_.video_type, media_id.ToString(), "Screen");
  bool has_audio = stream_devices.audio_device.has_value();
  auto media_stream_ui =
      browser_->GetMediaStreamRegistrar()->MaybeCreateMediaStreamUI(true,
                                                                    has_audio);
  std::move(callback_).Run(devices_set,
                           blink::mojom::MediaStreamRequestResult::OK,
                           std::move(media_stream_ui));
}
#endif  // BUILDFLAG(ARKWEB_WEBRTC)
