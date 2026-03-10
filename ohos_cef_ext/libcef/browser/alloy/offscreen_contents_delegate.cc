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

#include "cef/ohos_cef_ext/libcef/browser/alloy/offscreen_contents_delegate.h"

#include "cef/ohos_cef_ext/libcef/browser/offscreen_document_dialog_manager.h"
#include "cef/ohos_cef_ext/libcef/browser/permission/offscreen_permission_request_handler.h"
#include "extensions/browser/extension_host.h"
#include "ohos_nweb/src/nweb_impl.h"

namespace extensions {

OffscreenContentsDelegate::~OffscreenContentsDelegate() {}

ExtensionId OffscreenContentsDelegate::GetExtensionId() {
  if (extension_host_) {
    return extension_host_->extension_id();
  } else {
    return "";
  }
}

content::JavaScriptDialogManager*
OffscreenContentsDelegate::GetJavaScriptDialogManager(
    content::WebContents* source) {
  CefOffScreenDocumentDialogManager* manager =
      CefOffScreenDocumentDialogManager::GetInstance();
  manager->SetExtensionId(GetExtensionId());
  manager->SetBrowserHost(nullptr);
  return manager;
}

content::WebContents* OffscreenContentsDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (extension_host_) {
    return extension_host_->AddNewContents(
        source, std::move(new_contents), target_url, disposition,
        window_features, user_gesture, was_blocked);
  } else {
    return nullptr;
  }
}

void OffscreenContentsDelegate::CloseContents(content::WebContents* contents) {
  if (extension_host_) {
    extension_host_->CloseContents(contents);
  }
}

void OffscreenContentsDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  auto extension_id = GetExtensionId();
  OffscreenPermissionRequestHandler::GetInstance()
      ->HandleMediaStreamPermissionRequest(extension_id, request,
                                           std::move(callback));
}

bool OffscreenContentsDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (extension_host_) {
    return extension_host_->CheckMediaAccessPermission(render_frame_host,
                                                       security_origin, type);
  } else {
    return false;
  }
}

bool OffscreenContentsDelegate::IsNeverComposited(
    content::WebContents* web_contents) {
  if (extension_host_) {
    return extension_host_->IsNeverComposited(web_contents);
  } else {
    return false;
  }
}

content::PictureInPictureResult
OffscreenContentsDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  if (extension_host_) {
    return extension_host_->EnterPictureInPicture(web_contents);
  } else {
    return content::PictureInPictureResult::kNotSupported;
  }
}

void OffscreenContentsDelegate::ExitPictureInPicture() {
  if (extension_host_) {
    extension_host_->ExitPictureInPicture();
  }
}

std::string OffscreenContentsDelegate::GetTitleForMediaControls(
    content::WebContents* web_contents) {
  if (extension_host_) {
    return extension_host_->GetTitleForMediaControls(web_contents);
  } else {
    return "";
  }
}

bool OffscreenContentsDelegate::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
#if BUILDFLAG(ARKWEB_CONSOLE_LOGGING)
    blink::mojom::ConsoleMessageSource log_source,
#endif
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  // 扩展默认关闭consolelog
  return true;
}

}
