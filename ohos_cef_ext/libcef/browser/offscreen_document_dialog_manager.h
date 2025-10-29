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

#ifndef CEF_LIBCEF_BROWSER_OFFSCREEN_DOCUMENT_DIALOG_MANAGER_H_
#define CEF_LIBCEF_BROWSER_OFFSCREEN_DOCUMENT_DIALOG_MANAGER_H_
#pragma once

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/singleton.h"
#include "cef/include/cef_jsdialog_handler.h"
#include "cef/libcef/browser/javascript_dialog_runner.h"
#include "nweb_js_dialog_result.h"

class CefOffScreenDocumentDialogManager : public content::JavaScriptDialogManager {
 public:

  CefOffScreenDocumentDialogManager(const CefOffScreenDocumentDialogManager&) = delete;
  CefOffScreenDocumentDialogManager& operator=(const CefOffScreenDocumentDialogManager&) =
      delete;

  CefOffScreenDocumentDialogManager();
  ~CefOffScreenDocumentDialogManager() override;

  static CefOffScreenDocumentDialogManager* GetInstance();

  void SetExtensionId(const std::string& extensionId) { extension_id_ = extensionId; }
  std::string GetExtensionId() { return extension_id_; }
  void SetBrowserHost(CefBrowserHostBase* browser) { browser_ = browser; }
  CefBrowserHostBase* GetBrowserHost() { return browser_; }

  void Alert(const int requestId);
  void Confirm(const bool type, const int requestId);
  void Prompt(const bool type, const std::string& value, const int requestId);

  int AddAlertRequest(std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback);
  int AddConfirmRequest(std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback);
  int AddPromptRequest(std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback);

  // JavaScriptDialogManager methods.
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType message_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

 private:
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> FindAlertRequestById(int request_id);
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> FindConfirmRequestById(int request_id);
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> FindPromptRequestById(int request_id);

  void RemoveAlertRequestById(int request_id);
  void RemoveConfirmRequestById(int request_id);
  void RemovePromptRequestById(int request_id);

  void ClearAlertRequest();
  void ClearConfirmRequest();
  void ClearPromptRequest();
  // Method executed by the callback passed to CefJavaScriptDialogRunner::Run.
  void DialogClosed(DialogClosedCallback callback,
                    bool success,
                    const std::u16string& user_input);
                    
  // CefBrowserHostBase pointer is guaranteed to outlive this object.
  raw_ptr<CefBrowserHostBase> browser_;
  std::string extension_id_;

  // Must be the last member.
  base::WeakPtrFactory<CefOffScreenDocumentDialogManager> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_OFFSCREEN_DOCUMENT_DIALOG_MANAGER_H_
