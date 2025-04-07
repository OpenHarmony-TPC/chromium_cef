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

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include <memory>

#include "content/public/browser/download_manager_delegate.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace cef {

class DownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  // Called from the ChromeDownloadManagerDelegate constructor for Chrome
  // bootstrap. Alloy bootstrap uses AlloyDownloadManagerDelegate directly.
  static std::unique_ptr<DownloadManagerDelegate> Create(
      content::DownloadManager* download_manager);

 private:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<DownloadManagerDelegate>;
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
