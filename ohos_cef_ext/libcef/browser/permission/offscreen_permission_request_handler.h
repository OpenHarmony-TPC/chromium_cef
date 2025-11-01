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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_PERMISSION_OFFSCREEN_PERMISSION_REQUEST_HANDLER_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_PERMISSION_OFFSCREEN_PERMISSION_REQUEST_HANDLER_H_

#include "base/memory/singleton.h"
#include "content/public/browser/media_stream_request.h"

class AlloyMediaAccessRequest;
class AlloyScreenCaptureAccessRequest;

class OffscreenPermissionRequestHandler {
 public:
  static OffscreenPermissionRequestHandler* GetInstance();

  void HandleMediaStreamPermissionRequest(
      const std::string& extension_id,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback);
  void Grant(int resources, int request_key);
  void Deny(int resources, int request_key);

  virtual ~OffscreenPermissionRequestHandler();

 private:
  void ReportRequestResult(int resources, int request_key, bool granted);

  int AddMediaAccessRequest(
      std::shared_ptr<AlloyMediaAccessRequest> request);
  int AddScreenCaptureRequest(
      std::shared_ptr<AlloyScreenCaptureAccessRequest> request);
  std::shared_ptr<AlloyMediaAccessRequest> FindMediaAccessRequestByKey(
      int request_key);
  std::shared_ptr<AlloyScreenCaptureAccessRequest> FindScreenCaptureRequestByKey(
      int request_key);
  void RemoveMediaAccessRequestByKey(int request_key);
  void RemoveScreenCaptureRequestByKey(int request_key);

  int GetResourcesFromRequest(const content::MediaStreamRequest& request,
                              bool& is_media_device,
                              bool& is_screen_capture);
  bool IsMediaDeviceRequest(int resources);
  bool IsScreenCaptureRequest(int resources);

  friend struct base::DefaultSingletonTraits<OffscreenPermissionRequestHandler>;
  OffscreenPermissionRequestHandler() = default;
  OffscreenPermissionRequestHandler(const OffscreenPermissionRequestHandler&) =
      delete;
  OffscreenPermissionRequestHandler& operator=(
      const OffscreenPermissionRequestHandler&) = delete;
};
#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_PERMISSION_OFFSCREEN_PERMISSION_REQUEST_HANDLER_H_
