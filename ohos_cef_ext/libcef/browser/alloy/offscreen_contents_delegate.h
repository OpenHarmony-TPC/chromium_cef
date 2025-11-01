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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_ALLOY_OFFSCREEN_CONTENTS_DELEGATE_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_ALLOY_OFFSCREEN_CONTENTS_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionHost;

class OffscreenContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit OffscreenContentsDelegate(
      base::WeakPtr<ExtensionHost> extension_host)
      : extension_host_(std::move(extension_host)) {}

  virtual ~OffscreenContentsDelegate();

  ExtensionId GetExtensionId();

  // content WebContentsDelegate interface
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void CloseContents(content::WebContents* contents) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool IsNeverComposited(content::WebContents* web_contents) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  std::string GetTitleForMediaControls(
      content::WebContents* web_contents) override;

 private:
  base::WeakPtr<ExtensionHost> extension_host_;
};
}  // namespace extensions
#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_ALLOY_OFFSCREEN_CONTENTS_DELEGATE_H_
