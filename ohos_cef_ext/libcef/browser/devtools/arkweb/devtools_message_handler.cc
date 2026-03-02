/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "libcef/browser/devtools/arkweb/devtools_message_handler.h"

#include "base/datashare_uri_utils.h"
#include "base/logging.h"
#include "base/json/json_reader.h"
#include "content/browser/web_contents/web_contents_impl_ext.h"
#include "libcef/browser/devtools/arkweb/devtools_frontend.h"
#include "libcef/browser/thread_util.h"
#include "ohos_cef_ext/libcef/common/cef_open_devtools_ext_opt.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

absl::optional<base::Value::Dict> ExtractProtocolMessage(
    const base::Value::List& params) {
  if (params.size() < 1) {
    return absl::nullopt;
  }
  const std::string* protocol_message = params[0].GetIfString();
  if (!protocol_message) {
    return absl::nullopt;
  }
  auto result = base::JSONReader::Read(*protocol_message);
  if (!result.has_value()) {
    return absl::nullopt;
  }
  if (!result->is_dict()) {
    return absl::nullopt;
  }
  return std::move(result->GetDict());
}

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  using CallbackType = CefFileDialogManager::RunFileChooserCallback;

  explicit CefFileDialogCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~CefFileDialogCallbackImpl() override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      CancelNow(std::move(callback_));
    }
  }

  void Continue(const std::vector<CefString>& file_paths) override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      std::vector<base::FilePath> vec;
      std::vector<CefString>::const_iterator it = file_paths.begin();
      for (; it != file_paths.end(); ++it) {
        base::FilePath path(*it);
        if (path.IsDataShareUri()) {
          vec.push_back(base::FilePath(base::GetRealPath(path)));
        } else {
          vec.push_back(path);
        }
      }
      std::move(callback_).Run(vec);
    }
  }

  void Cancel() override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      CancelNow(std::move(callback_));
    }
  }

  CallbackType ReleaseCallback() {
    return std::move(callback_);
  }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    std::move(callback).Run(file_paths);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

class CefInfoBarCallbackImpl : public CefInfoBarCallback {
 public:
  using CallbackType = CefDevToolsMessageHandler::InfoBarCallback;

  explicit CefInfoBarCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~CefInfoBarCallbackImpl() override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      std::move(callback_).Run(default_value_);
    }
  }

  void Allow(bool allow) override {
    if (!callback_.is_null()) {
      std::move(callback_).Run(allow);
    }
  }
 private:
  CallbackType callback_;
  bool default_value_ = true;

  IMPLEMENT_REFCOUNTING(CefInfoBarCallbackImpl);
};

} // namespace

using Result = CefDevToolsMessageHandler::Result;

CefDevToolsMessageHandler::CefDevToolsMessageHandler(
    CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
    Profile* profile,
    const CefOpenDevToolsExtOpt& extOpt)
    : delegate_(std::move(delegate)),
      settings_(profile),
      can_dock_(extOpt.canDock),
      is_docked_(false),
      dock_mode_changed_(false),
      weak_factory_(this) {
  LOG(INFO) << "CefDevToolsMessageHandler canDock: " << extOpt.canDock;
  method_handlers_["dispatchProtocolMessage"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->HandleProtocolMessage(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["bringToFront"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->BringToFront(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["closeWindow"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->CloseWindow(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["inspectedURLChanged"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->InspectedURLChanged(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["registerPreference"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->RegisterPreference(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["getPreferences"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->GetPreferences(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["getPreference"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->GetPreference(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["setPreference"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->SetPreference(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["removePreference"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->RemovePreference(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["clearPreferences"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->ClearPreferences(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["setInspectedPageBounds"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->SetInspectedPageBounds(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
  method_handlers_["setIsDocked"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->SetDockMode(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());

  protocol_message_handlers_["Page.bringToFront"] = base::BindRepeating(
      [](base::WeakPtr<CefDevToolsMessageHandler> self, const base::Value::List& params) {
        if (self) {
          return self->PageBringToFront(params);
        }
        return Result{false, {}};
      }, weak_factory_.GetWeakPtr());
}

CefDevToolsMessageHandler::~CefDevToolsMessageHandler() {}

void CefDevToolsMessageHandler::SetDevToolsFrontend(
    CefDevToolsFrontend* frontend) {
  DCHECK(frontend != nullptr && devtools_frontend_ == nullptr);
  devtools_frontend_ = frontend;
}

CefFileDialogManager::RunFileChooserCallback
CefDevToolsMessageHandler::ShowFileChooser(
    const blink::mojom::FileChooserParams& params,
    CefFileDialogManager::RunFileChooserCallback callback) {
  LOG(INFO) << "CefDevToolsMessageHandler::ShowFileChooser";
  if (!delegate_) {
    LOG(INFO) << "ShowFileChooser, no delegate_";
    return callback;
  }

  int mode = FILE_DIALOG_OPEN;
  switch (params.mode) {
    case blink::mojom::FileChooserParams::Mode::kOpen:
      mode = FILE_DIALOG_OPEN;
      break;
    case blink::mojom::FileChooserParams::Mode::kOpenMultiple:
      mode = FILE_DIALOG_OPEN_MULTIPLE;
      break;
    case blink::mojom::FileChooserParams::Mode::kUploadFolder:
      mode = FILE_DIALOG_OPEN_FOLDER;
      break;
    case blink::mojom::FileChooserParams::Mode::kSave:
      mode = FILE_DIALOG_SAVE;
      break;
    default:
      DCHECK(false);
      break;
  }

  std::vector<std::u16string>::const_iterator it = params.accept_types.begin();
  std::vector<CefString> accept_filters;
  for (; it != params.accept_types.end(); ++it) {
    accept_filters.push_back(*it);
  }

  CefRefPtr<CefFileDialogCallbackImpl> callback_impl(
      new CefFileDialogCallbackImpl(std::move(callback)));

  bool handled = delegate_->ShowFileChooser(
      static_cast<cef_file_dialog_mode_t>(mode), params.title,
      params.default_file_name.value(), accept_filters,
      params.use_media_capture, callback_impl.get());
  if (handled) {
    return CefFileDialogManager::RunFileChooserCallback();
  }

  return callback_impl->ReleaseCallback();
}

void CefDevToolsMessageHandler::ShowInfoBar(
    const std::string& message,
    const std::string& path,
    InfoBarCallback callback) {
  LOG(INFO) << "CefDevToolsMessageHandler::ShowInfoBar";
  if (!delegate_) {
    LOG(INFO) << "ShowInfoBar, no delegate_";
    return;
  }

  CefRefPtr<CefInfoBarCallbackImpl> callback_impl(
      new CefInfoBarCallbackImpl(std::move(callback)));
  delegate_->ShowInfoBar(message, path, callback_impl);
}

Result CefDevToolsMessageHandler::HandleMessage(int request_id,
    const std::string& method,
    const base::Value::List& params) {
  auto iter = method_handlers_.find(method);
  if (iter == method_handlers_.end()) {
    return {false, {}};
  }
  return iter->second.Run(params);
}

Result CefDevToolsMessageHandler::HandleProtocolMessage(
    const base::Value::List& message_params) {
  auto message = ExtractProtocolMessage(message_params);
  if (!message) {
    return {false, {}};
  }
  const std::string* method = message->FindString("method");
  if (!method) {
    return {false, {}};
  }

  const base::Value::List* params_value = message->FindList("params");

  auto iter = protocol_message_handlers_.find(*method);
  if (iter == protocol_message_handlers_.end()) {
    return {false, {}};
  }

  if (params_value) {
    iter->second.Run(*params_value);
  } else {
    iter->second.Run({});
  }

  // Never prevent protocol message.
  return {false, {}};
}

Result CefDevToolsMessageHandler::BringToFront(const base::Value::List&) {
  LOG(INFO) << "CefDevToolsMessageHandler::BringToFront";
  if (!delegate_) {
    LOG(INFO) << "BringToFront, no delegate_";
    return {false, {}};
  }
  return {delegate_->ActiveDevToolsWindow(), {}};
}

Result CefDevToolsMessageHandler::CloseWindow(const base::Value::List&) {
  LOG(INFO) << "CefDevToolsMessageHandler::CloseWindow";
  if (!delegate_) {
    LOG(INFO) << "CloseWindow, no delegate_";
    return {false, {}};
  }
  return {delegate_->CloseWindow(), {}};
}

Result CefDevToolsMessageHandler::InspectedURLChanged(const base::Value::List& param) {
  LOG(INFO) << "CefDevToolsMessageHandler::InspectedURLChanged";
  if (!delegate_) {
    LOG(INFO) << "InspectedURLChanged, no delegate_";
    return {false, {}};
  }
  return {false, {}};
}

Result CefDevToolsMessageHandler::RegisterPreference(const base::Value::List& params) {
  if (params.size() < 2) {
    return {false, {}};
  }
  const std::string* name = params[0].GetIfString();

  if (!name || !params[1].is_dict()) {
    return {false, {}};
  }

  const bool synced = params[1].GetDict().FindBool("synced").value_or(false);
  RegisterOptions options{RegisterOptions::SyncMode::kDontSync};
  if (synced) {
    options.sync_mode = RegisterOptions::SyncMode::kSync;
  }
  settings_.Register(*name, options);
  return {true, {}};
}

Result CefDevToolsMessageHandler::GetPreferences(const base::Value::List& params) {
  return {true, base::Value(settings_.Get())};
}

Result CefDevToolsMessageHandler::GetPreference(const base::Value::List& params) {
  if (params.empty()) {
    return {false, {}};
  }
  const std::string* name = params[0].GetIfString();
  if (!name) {
    return {false, {}};
  }
  return {true, settings_.Get(*name).value_or(base::Value())};
}

Result CefDevToolsMessageHandler::SetPreference(const base::Value::List& params) {
  if (params.size() < 2) {
    return {false, {}};
  }

  const std::string* name = params[0].GetIfString();
  const std::string* value = params[1].GetIfString();
  if (!name || !value) {
    return {false, {}};
  }

  settings_.Set(*name, *value);
  return {true, {}};
}

Result CefDevToolsMessageHandler::RemovePreference(const base::Value::List& params) {
  if (params.empty()) {
    return {false, {}};
  }

  const std::string* name = params[0].GetIfString();
  if (!name) {
    return {false, {}};
  }

  settings_.Remove(*name);
  return {true, {}};
}

Result CefDevToolsMessageHandler::ClearPreferences(const base::Value::List& params) {
  settings_.Clear();
  return {true, {}};
}

Result CefDevToolsMessageHandler::SetInspectedPageBounds(
    const base::Value::List& params) {
  if (params.empty()) {
    LOG(INFO)
        << "CefDevToolsMessageHandler::SetInspectedPageBounds params is empty.";
    return {false, {}};
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto& dict = params[0].GetDict();
 
  int left = dict.FindInt("x").value_or(0);
  int top = dict.FindInt("y").value_or(0);
  int width = dict.FindInt("width").value_or(0);
  int height = dict.FindInt("height").value_or(0);
 
  const gfx::Rect rect(left, top, width, height);
  CefResizingStrategy strategy(rect);
  if (resizing_strategy_.Equals(strategy)) {
    LOG(INFO)
        << "CefDevToolsMessageHandler::SetInspectedPageBounds resizing_strategy_.Equals.";
    if (dock_mode_changed_) {
      dock_mode_changed_ = false;
      delegate_->SetDockMode((int)dock_mode_);
    }
    return {delegate_->SetInspectedPageBounds(left, top, width, height), {}};
  }
  resizing_strategy_.CopyFrom(strategy);
  if (dock_mode_changed_ && can_dock_) {
    UpdateDockMode();
  }
 
  return {delegate_->SetInspectedPageBounds(left, top, width, height), {}};
}
 
void CefDevToolsMessageHandler::UpdateDockMode() {
  dock_mode_changed_ = false;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  gfx::Rect inspected_page_bounds = resizing_strategy_.bounds();
 
  gfx::Rect devtools_bounds = devtools_frontend_->web_contents()
                                  ->GetRenderWidgetHostView()
                                  ->GetViewBounds();
  if (inspected_page_bounds.x() > 0) {
    dock_mode_ = DockMode::LEFT;
    delegate_->SetDockMode((int)dock_mode_);
    return;
  }
  if (inspected_page_bounds.width() == devtools_bounds.width()) {
    dock_mode_ = DockMode::BOTTOM;
    delegate_->SetDockMode((int)dock_mode_);
  } else {
    dock_mode_ = DockMode::RIGHT;
    delegate_->SetDockMode((int)dock_mode_);
  }
}
 
Result CefDevToolsMessageHandler::SetDockMode(const base::Value::List& params) {
  if (params.empty()) {
    LOG(INFO) << "CefDevToolsMessageHandler::SetDockMode params is empty or "
                    "not candock.";
    return {false, {}};
  }
  LOG(INFO) << "CefDevToolsMessageHandler::SetDockMode isdocked: " << params[0].GetBool()
            << ", can_dock_: " << can_dock_;
  bool dock_requested = params[0].is_bool() ? params[0].GetBool() : false;

  is_docked_ = dock_requested;
  if (!is_docked_) {
    dock_mode_ = DockMode::UNDOCKED;
    return {delegate_->SetDockMode((int)dock_mode_), {}};
  }
  dock_mode_changed_ = true;
 
  return {true, {}};
}

Result CefDevToolsMessageHandler::PageBringToFront(const base::Value::List&) {
  LOG(INFO) << "CefDevToolsMessageHandler::PageBringToFront";
  if (!delegate_) {
    LOG(INFO) << "PageBringToFront, no delegate_";
    return {false, {}};
  }
  return {delegate_->BringToFront(), {}};
}
