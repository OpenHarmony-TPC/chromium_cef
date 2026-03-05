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

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_DEVTOOLS_MESSAGE_HANDLER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_DEVTOOLS_MESSAGE_HANDLER_H_

#include "base/values.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

#include "chrome/browser/devtools/devtools_settings.h"
#include "libcef/browser/file_dialog_manager.h"
#include "include/cef_devtools_message_handler_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace blink::mojom {
class FileChooserParams;
}

class CefDevToolsFrontend;
struct CefOpenDevToolsExtOpt;
 
enum class DockMode { BOTTOM = 0, RIGHT, LEFT, UNDOCKED };
 
class CefResizingStrategy {
 public:
  CefResizingStrategy() : hide_inspected_contents_(false) {}
  explicit CefResizingStrategy(const gfx::Rect& bounds)
      : bounds_(bounds),
        hide_inspected_contents_(bounds_.IsEmpty() && !bounds_.x() &&
                                 !bounds_.y()) {}
 
  CefResizingStrategy(const CefResizingStrategy&) = delete;
  CefResizingStrategy& operator=(const CefResizingStrategy&) = delete;
 
  void CopyFrom(const CefResizingStrategy& strategy) {
    bounds_ = strategy.bounds();
    hide_inspected_contents_ = strategy.hide_inspected_contents();
  }
  bool Equals(const CefResizingStrategy& strategy) {
    return bounds_ == strategy.bounds() &&
           hide_inspected_contents_ == strategy.hide_inspected_contents();
  }
 
  const gfx::Rect& bounds() const { return bounds_; }
  bool hide_inspected_contents() const { return hide_inspected_contents_; }
 
 private:
  // Contents bounds. When non-empty, used instead of insets.
  gfx::Rect bounds_;
 
  // Determines whether inspected contents is visible.
  bool hide_inspected_contents_;
};

class CefDevToolsMessageHandler final {
 public:
  explicit CefDevToolsMessageHandler(
      CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
      Profile* profile,
      const CefOpenDevToolsExtOpt& extOpt);
  CefDevToolsMessageHandler(const CefDevToolsMessageHandler&) = delete;
  CefDevToolsMessageHandler(CefDevToolsMessageHandler&&) = delete;
  ~CefDevToolsMessageHandler();

  void SetDevToolsFrontend(CefDevToolsFrontend* frontend);

  CefFileDialogManager::RunFileChooserCallback ShowFileChooser(
      const blink::mojom::FileChooserParams& params,
      CefFileDialogManager::RunFileChooserCallback callback);

  using InfoBarCallback = base::OnceCallback<void(bool)>;
  void ShowInfoBar(
      const std::string& message,
      const std::string& path,
      InfoBarCallback callback);

  struct Result final {
    bool handled = false;
    base::Value result;
  };
  Result HandleMessage(int request_id, const std::string& method,
      const base::Value::List& params);

 private:
  Result HandleProtocolMessage(const base::Value::List&);
  Result BringToFront(const base::Value::List&);
  Result CloseWindow(const base::Value::List&);
  Result InspectedURLChanged(const base::Value::List&);
  Result PageBringToFront(const base::Value::List&);

  Result RegisterPreference(const base::Value::List& params);
  Result GetPreferences(const base::Value::List& params);
  Result GetPreference(const base::Value::List& params);
  Result SetPreference(const base::Value::List& params);
  Result RemovePreference(const base::Value::List& params);
  Result ClearPreferences(const base::Value::List& params);
  Result SetInspectedPageBounds(const base::Value::List& params);
  Result SetDockMode(const base::Value::List& params);
  void UpdateDockMode();

  CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate_;

  using Handler = base::RepeatingCallback<Result(const base::Value::List&)>;
  std::map<std::string, Handler> method_handlers_;
  std::map<std::string, Handler> protocol_message_handlers_;

  DevToolsSettings settings_;
  bool can_dock_;
  bool is_docked_;
  DockMode dock_mode_;
  bool dock_mode_changed_;
  CefResizingStrategy resizing_strategy_;
  base::raw_ptr<CefDevToolsFrontend> devtools_frontend_;
#if BUILDFLAG(ARKWEB_CRASHPAD)
  base::WeakPtrFactory<CefDevToolsMessageHandler> weak_factory_;
#endif
};
#endif // CEF_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_DEVTOOLS_MESSAGE_HANDLER_H_
