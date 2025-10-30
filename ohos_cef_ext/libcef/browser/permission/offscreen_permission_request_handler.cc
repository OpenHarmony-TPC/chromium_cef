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

#include "cef/ohos_cef_ext/libcef/browser/permission/offscreen_permission_request_handler.h"

#include <map>
#include <memory>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/ohos_cef_ext/libcef/browser/permission/alloy_access_request.h"
#include "ohos_nweb/src/nweb_impl.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace {
static std::atomic<int> g_request_new_key = 0;
static std::unordered_map<int, std::shared_ptr<AlloyMediaAccessRequest>>
    g_media_access_requests_map;
static std::unordered_map<int, std::shared_ptr<AlloyScreenCaptureAccessRequest>>
    g_screen_capture_requests_map;
static base::Lock g_requests_lock;
}  // namespace

// static
OffscreenPermissionRequestHandler*
OffscreenPermissionRequestHandler::GetInstance() {
  return base::Singleton<OffscreenPermissionRequestHandler>::get();
}

OffscreenPermissionRequestHandler::~OffscreenPermissionRequestHandler() {
  base::AutoLock lock_scope(g_requests_lock);
  for (const auto& [key, request] : g_media_access_requests_map) {
    if (request) {
      request->ReportRequestResult(false);
    }
  }
  g_media_access_requests_map.clear();
  for (const auto& [key, request] : g_screen_capture_requests_map) {
    if (request) {
      request->ReportRequestResult(false);
    }
  }
  g_screen_capture_requests_map.clear();
}

void OffscreenPermissionRequestHandler::HandleMediaStreamPermissionRequest(
    const std::string& extension_id,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
#if BUILDFLAG(ARKWEB_WEBRTC)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMediaStream)) {
    LOG(ERROR) << __FUNCTION__ << " media stream not enabled.";
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }

  bool is_media_device = false;
  bool is_screen_capture = false;
  int resources =
      GetResourcesFromRequest(request, is_media_device, is_screen_capture);
  int request_key = -1;
  if (!is_screen_capture && is_media_device) {
    request_key =
        AddMediaAccessRequest(std::make_shared<AlloyMediaAccessRequest>(
            /*browser=*/nullptr, request, std::move(callback)));
  } else {
    request_key = AddScreenCaptureRequest(
        std::make_shared<AlloyScreenCaptureAccessRequest>(
            /*browser=*/nullptr, request, std::move(callback)));
  }

  OHOS::NWeb::NWebImpl::OnOffscreenDocumentPermissionRequest(
      extension_id, request.security_origin.spec(), resources, request_key);
#else
  std::move(callback).Run(
      blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
      /*ui=*/nullptr);
#endif
}

void OffscreenPermissionRequestHandler::Grant(int resources, int request_key) {
  ReportRequestResult(resources, request_key, true);
}

void OffscreenPermissionRequestHandler::Deny(int resources, int request_key) {
  ReportRequestResult(resources, request_key, false);
}

void OffscreenPermissionRequestHandler::ReportRequestResult(int resources,
                                                            int request_key,
                                                            bool granted) {
  if (IsMediaDeviceRequest(resources)) {
    auto request = FindMediaAccessRequestByKey(request_key);
    if (request) {
      request->ReportRequestResult(granted);
      RemoveMediaAccessRequestByKey(request_key);
      return;
    }
  }
  if (IsScreenCaptureRequest(resources)) {
    auto request = FindScreenCaptureRequestByKey(request_key);
    if (request) {
      // CAPTURE_HOME_SCREEN_MODE is the only supported mode.
      request->SetCaptureMode(
          cef_screen_capture_mode_t::CAPTURE_HOME_SCREEN_MODE);
      request->ReportRequestResult(granted);
      RemoveScreenCaptureRequestByKey(request_key);
      return;
    }
  }
  LOG(ERROR) << __FUNCTION__ << " invalid resources " << resources
             << " with key " << request_key << ", granted " << granted;
}

int OffscreenPermissionRequestHandler::AddMediaAccessRequest(
    std::shared_ptr<AlloyMediaAccessRequest> request) {
  base::AutoLock lock_scope(g_requests_lock);
  g_media_access_requests_map[++g_request_new_key] = std::move(request);
  return g_request_new_key;
}

int OffscreenPermissionRequestHandler::AddScreenCaptureRequest(
    std::shared_ptr<AlloyScreenCaptureAccessRequest> request) {
  base::AutoLock lock_scope(g_requests_lock);
  g_screen_capture_requests_map[++g_request_new_key] = std::move(request);
  return g_request_new_key;
}

std::shared_ptr<AlloyMediaAccessRequest>
OffscreenPermissionRequestHandler::FindMediaAccessRequestByKey(
    int request_key) {
  base::AutoLock lock_scope(g_requests_lock);
  if (auto it = g_media_access_requests_map.find(request_key);
      it != g_media_access_requests_map.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<AlloyScreenCaptureAccessRequest>
OffscreenPermissionRequestHandler::FindScreenCaptureRequestByKey(
    int request_key) {
  base::AutoLock lock_scope(g_requests_lock);
  if (auto it = g_screen_capture_requests_map.find(request_key);
      it != g_screen_capture_requests_map.end()) {
    return it->second;
  }
  return nullptr;
}

void OffscreenPermissionRequestHandler::RemoveMediaAccessRequestByKey(
    int request_key) {
  base::AutoLock lock_scope(g_requests_lock);
  g_media_access_requests_map.erase(request_key);
}

void OffscreenPermissionRequestHandler::RemoveScreenCaptureRequestByKey(
    int request_key) {
  base::AutoLock lock_scope(g_requests_lock);
  g_screen_capture_requests_map.erase(request_key);
}

int OffscreenPermissionRequestHandler::GetResourcesFromRequest(
    const content::MediaStreamRequest& request,
    bool& is_media_device,
    bool& is_screen_capture) {
  int resources = 0;
  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    resources |= static_cast<int>(
        OffscreenDocumentPermissionResourceType::AUDIO_CAPTURE);
    is_media_device = true;
  }
  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    resources |= static_cast<int>(
        OffscreenDocumentPermissionResourceType::VIDEO_CAPTURE);
    is_media_device = true;
  }
  if ((request.video_type ==
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) ||
      (request.video_type ==
       blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE)) {
    resources |= static_cast<int>(
        OffscreenDocumentPermissionResourceType::DISPLAY_VIDEO_CAPTURE);
    is_screen_capture = true;
  }
  if (request.audio_type ==
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
    resources |= static_cast<int>(
        OffscreenDocumentPermissionResourceType::DISPLAY_AUDIO_CAPTURE);
    is_screen_capture = true;
  }
  return resources;
}

bool OffscreenPermissionRequestHandler::IsMediaDeviceRequest(int resources) {
  int audio_device =
      static_cast<int>(OffscreenDocumentPermissionResourceType::AUDIO_CAPTURE);
  int video_device =
      static_cast<int>(OffscreenDocumentPermissionResourceType::VIDEO_CAPTURE);
  return (((resources & audio_device) == audio_device) ||
          ((resources & video_device) == video_device));
}

bool OffscreenPermissionRequestHandler::IsScreenCaptureRequest(int resources) {
  int screen_capture = static_cast<int>(
      OffscreenDocumentPermissionResourceType::DISPLAY_VIDEO_CAPTURE);
  return ((resources & screen_capture) == screen_capture);
}
