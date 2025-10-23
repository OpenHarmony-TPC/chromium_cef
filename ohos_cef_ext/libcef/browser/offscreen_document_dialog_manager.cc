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

#include "cef/ohos_cef_ext/libcef/browser/offscreen_document_dialog_manager.h"

#include <utility>

#include "arkweb/build/features/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/lock.h"
#include "base/logging.h"
#include "cef/libcef/browser/browser_guest_util.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/thread_util.h"

#if BUILDFLAG(IS_ARKWEB)
#include "arkweb/ohos_nweb/src/nweb_impl.h"
#endif  // BUILDFLAG(IS_ARKWEB)

namespace {

class CefJSDialogCallbackImpl : public CefJSDialogCallback {
 public:
  using CallbackType = content::JavaScriptDialogManager::DialogClosedCallback;

  explicit CefJSDialogCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}
  ~CefJSDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefJSDialogCallbackImpl::CancelNow,
                                     std::move(callback_)));
      }
    }
  }

  void Continue(bool success, const CefString& user_input) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::move(callback_).Run(success, user_input);
      }
    } else {
      CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefJSDialogCallbackImpl::Continue,
                                            this, success, user_input));
    }
  }

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(false, std::u16string());
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefJSDialogCallbackImpl);
};

static std::atomic<int> g_request_new_id = 0;
static std::unordered_map<int, std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>>
    g_alert_requests_map;
static std::unordered_map<int, std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>>
    g_confirm_requests_map;
static std::unordered_map<int, std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>>
    g_prompt_requests_map;

static base::Lock g_requests_lock;

}  // namespace

CefOffScreenDocumentDialogManager*
CefOffScreenDocumentDialogManager::GetInstance() {
  return base::Singleton<CefOffScreenDocumentDialogManager>::get();
}

CefOffScreenDocumentDialogManager::CefOffScreenDocumentDialogManager()
    : browser_(nullptr),
      extension_id_(std::string()),
      weak_ptr_factory_(this) {}

CefOffScreenDocumentDialogManager::~CefOffScreenDocumentDialogManager() {
  ClearAlertRequest();
  ClearConfirmRequest();
  ClearPromptRequest();
}

void CefOffScreenDocumentDialogManager::Alert(const int requestId) {
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback =
      FindAlertRequestById(requestId);
  if (callback) {
    callback->Confirm();
    RemoveAlertRequestById(requestId);
    return;
  }
  LOG(ERROR) << "CefOffScreenDocumentDialogManager::Alert callback not find";
}

void CefOffScreenDocumentDialogManager::Confirm(const bool type,
                                                const int requestId) {
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback =
      FindConfirmRequestById(requestId);
  if (callback) {
    if (type) {
      callback->Confirm();
      RemoveConfirmRequestById(requestId);
      return;
    }
    callback->Cancel();
    RemoveConfirmRequestById(requestId);
    return;
  }
  LOG(ERROR) << "CefOffScreenDocumentDialogManager::Confirm callback not find";
}
void CefOffScreenDocumentDialogManager::Prompt(const bool type,
                                               const std::string& value,
                                               const int requestId) {
  std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback =
      FindPromptRequestById(requestId);
  if (callback) {
    if (type) {
      callback->Confirm(value);
      RemovePromptRequestById(requestId);
      return;
    }
    callback->Cancel();
    RemovePromptRequestById(requestId);
    return;
  }
  LOG(ERROR) << "CefOffScreenDocumentDialogManager::Prompt callback not find";
}

int CefOffScreenDocumentDialogManager::AddAlertRequest(
    std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback) {
  base::AutoLock lock_scope(g_requests_lock);
  g_alert_requests_map[++g_request_new_id] = std::move(callback);
  return g_request_new_id;
}

int CefOffScreenDocumentDialogManager::AddConfirmRequest(
    std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback) {
  base::AutoLock lock_scope(g_requests_lock);
  g_confirm_requests_map[++g_request_new_id] = std::move(callback);
  return g_request_new_id;
}

int CefOffScreenDocumentDialogManager::AddPromptRequest(
    std::shared_ptr<OHOS::NWeb::NWebJSDialogResult> callback) {
  base::AutoLock lock_scope(g_requests_lock);
  g_prompt_requests_map[++g_request_new_id] = std::move(callback);
  return g_request_new_id;
}

std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>
CefOffScreenDocumentDialogManager::FindAlertRequestById(int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  if (auto it = g_alert_requests_map.find(request_id);
      it != g_alert_requests_map.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>
CefOffScreenDocumentDialogManager::FindConfirmRequestById(int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  if (auto it = g_confirm_requests_map.find(request_id);
      it != g_confirm_requests_map.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<OHOS::NWeb::NWebJSDialogResult>
CefOffScreenDocumentDialogManager::FindPromptRequestById(int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  if (auto it = g_prompt_requests_map.find(request_id);
      it != g_prompt_requests_map.end()) {
    return it->second;
  }
  return nullptr;
}

void CefOffScreenDocumentDialogManager::RemoveAlertRequestById(int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  g_alert_requests_map.erase(request_id);
}

void CefOffScreenDocumentDialogManager::RemoveConfirmRequestById(
    int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  g_confirm_requests_map.erase(request_id);
}

void CefOffScreenDocumentDialogManager::RemovePromptRequestById(
    int request_id) {
  base::AutoLock lock_scope(g_requests_lock);
  g_prompt_requests_map.erase(request_id);
}

void CefOffScreenDocumentDialogManager::ClearAlertRequest() {
  base::AutoLock lock_scope(g_requests_lock);
  g_alert_requests_map.clear();
}

void CefOffScreenDocumentDialogManager::ClearConfirmRequest() {
  base::AutoLock lock_scope(g_requests_lock);
  g_confirm_requests_map.clear();
}

void CefOffScreenDocumentDialogManager::ClearPromptRequest() {
  base::AutoLock lock_scope(g_requests_lock);
  g_prompt_requests_map.clear();
}

void CefOffScreenDocumentDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType message_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  *did_suppress_message = false;

  const GURL& origin_url = render_frame_host->GetLastCommittedURL();
  // Always call DialogClosed().
  callback =
      base::BindOnce(&CefOffScreenDocumentDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
      new CefJSDialogCallbackImpl(std::move(callback)));

  std::string extension_id = extension_id_;
  std::string message = base::UTF16ToUTF8(message_text);
  std::string url = origin_url.spec();
  std::string prompt = base::UTF16ToUTF8(default_prompt_text);

  switch (message_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      OHOS::NWeb::NWebImpl::OnAlertDialogByJS(
          extension_id, url, message, callbackPtr.get(), *did_suppress_message);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      OHOS::NWeb::NWebImpl::OnConfirmDialogByJS(
          extension_id, url, message, callbackPtr.get(), *did_suppress_message);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      OHOS::NWeb::NWebImpl::OnPromptDialogByJS(extension_id, url, message,
                                               prompt, callbackPtr.get(),
                                               *did_suppress_message);
      break;
    default:
      break;
  }

  if (*did_suppress_message) {
    // Call OnResetDialogState but don't execute |callback|.
    CancelDialogs(web_contents, /*reset_state=*/true);
    return;
  }
}

void CefOffScreenDocumentDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  // Always call DialogClosed().
  callback =
      base::BindOnce(&CefOffScreenDocumentDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
      new CefJSDialogCallbackImpl(std::move(callback)));

  // |callback| may be null if the user executed it despite returning false.
  callbackPtr->Continue(false, CefString(std::string()));
  return;
}

bool CefOffScreenDocumentDialogManager::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  DialogClosed(base::NullCallback(), accept,
               prompt_override ? *prompt_override : std::u16string());
  return true;
}

void CefOffScreenDocumentDialogManager::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  // Null when called from DialogClosed().
  if (!web_contents) {
    return;
  }
}

void CefOffScreenDocumentDialogManager::DialogClosed(
    DialogClosedCallback callback,
    bool success,
    const std::u16string& user_input) {
  CancelDialogs(/*web_contents=*/nullptr, /*reset_state=*/true);

  // Null when called from HandleJavaScriptDialog().
  if (!callback.is_null()) {
    std::move(callback).Run(success, user_input);
  }
}
