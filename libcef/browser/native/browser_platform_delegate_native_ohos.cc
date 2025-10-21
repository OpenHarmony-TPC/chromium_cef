/*
 * Copyright (c) 2023-2025 Haitai FangYuan Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cef/libcef/browser/native/browser_platform_delegate_native_ohos.h"

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/native/window_delegate_view.h"
#include "cef/libcef/browser/thread_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "ohos/adapter/file_manager/file_manager_adapter.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/display/screen_ohos.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#include "ui/gfx/font_render_params.h"
#include "ui/views/widget/widget.h"

using namespace ohos::adapter::xcomponent;

namespace {

void WriteTempFileAndView(const std::string& data) {
  CEF_REQUIRE_BLOCKING();

  base::FilePath tmp_file;
  if (!base::CreateTemporaryFile(&tmp_file)) {
    return;
  }
  tmp_file = tmp_file.AddExtension("txt");
  const bool write_success = base::WriteFile(tmp_file, data);
  DCHECK(write_success);

  auto& file_manager_adapter_instance =
      ohos::adapter::FileManagerAdapter::GetInstance();
  file_manager_adapter_instance.OpenVerifiedItem(tmp_file.value());
}

}  // namespace

CefBrowserPlatformDelegateNativeOhos::CefBrowserPlatformDelegateNativeOhos(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color) {}

void CefBrowserPlatformDelegateNativeOhos::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNativeAura::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeOhos::CreateHostWindow() {
  DCHECK(!window_widget_);

  int left = window_info_.bounds.x;
  int top = window_info_.bounds.y;
  int width = window_info_.bounds.width;
  int height = window_info_.bounds.height;
  if (width == 0 || height == 0) {
    auto window_rect = WindowAdapter::GetInstance().GetInitialBounds();
    gfx::Rect rect_in_pixels = {window_rect.left, window_rect.top,
                                window_rect.width, window_rect.height};
    gfx::Rect bounds_in_pixels =
        display::ohos::ScreenOhos::ConvertPixelToDIP(rect_in_pixels);
    left = bounds_in_pixels.x();
    top = bounds_in_pixels.y();
    width = bounds_in_pixels.width();
    height = bounds_in_pixels.height();
  }
  if (width == 0) {
    width = 1700;
  }
  if (height == 0) {
    height = 1007;
  }

  window_info_.bounds.x = left;
  window_info_.bounds.y = top;
  window_info_.bounds.width = width;
  window_info_.bounds.height = height;

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  gfx::Rect rect(window_info_.bounds.x, window_info_.bounds.y,
                 window_info_.bounds.width, window_info_.bounds.height);
  CefWindowDelegateView* delegate_view = new CefWindowDelegateView(
      GetBackgroundColor(), true, GetBoundsChangedCallback(),
      GetWidgetDeleteCallback());
  delegate_view->Init(
      static_cast<gfx::AcceleratedWidget>(window_info_.window),
      web_contents_, rect);

  window_widget_ = delegate_view->GetWidget();
  window_widget_->Show();

  // As an additional requirement on OHOS, we must set the colors for the
  // render widgets in webkit.
  auto prefs = web_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

  // Set font-related attributes.
  static const gfx::FontRenderParams params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;

  web_contents_->SyncRendererPrefs();

  return true;
}

void CefBrowserPlatformDelegateNativeOhos::CloseHostWindow() {
  if (browser_) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::WindowDestroyed,
                                 static_cast<AlloyBrowserHostImpl*>(browser_)));
  }
}

CefWindowHandle CefBrowserPlatformDelegateNativeOhos::GetHostWindowHandle()
    const {
  if (windowless_handler_) {
    return windowless_handler_->GetParentWindowHandle();
  }
  return window_info_.window;
}

views::Widget* CefBrowserPlatformDelegateNativeOhos::GetWindowWidget() const {
  return window_widget_;
}

void CefBrowserPlatformDelegateNativeOhos::SetFocus(bool setFocus) {
  if (!setFocus) {
    return;
  }

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    web_contents_->Focus();
  }
}

void CefBrowserPlatformDelegateNativeOhos::NotifyMoveOrResizeStarted() {
  // Call the parent method to dismiss any existing popups.
  CefBrowserPlatformDelegateNativeAura::NotifyMoveOrResizeStarted();

  if (!web_contents_) {
    return;
  }
}

void CefBrowserPlatformDelegateNativeOhos::SizeTo(int width, int height) {}

void CefBrowserPlatformDelegateNativeOhos::ViewText(const std::string& text) {
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(WriteTempFileAndView, text));
}

bool CefBrowserPlatformDelegateNativeOhos::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
  return false;
}

CefEventHandle CefBrowserPlatformDelegateNativeOhos::GetEventHandle(
    const input::NativeWebKeyboardEvent& event) const {
  // TODO(cef): We need to return an XEvent* from this method, but
  // |event.os_event->native_event()| now returns a ui::Event* instead.
  // See https://crbug.com/965991.
  return nullptr;
}

ui::KeyEvent CefBrowserPlatformDelegateNativeOhos::TranslateUiKeyEvent(
    const CefKeyEvent& key_event) const {
  int flags = TranslateUiEventModifiers(key_event.modifiers);
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(key_event.windows_key_code);
  ui::DomCode dom_code =
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code);

  char16_t character = key_event.character;

  base::TimeTicks time_stamp = GetEventTimeStamp();

  if (key_event.type == KEYEVENT_CHAR) {
    return ui::KeyEvent::FromCharacter(character, key_code, dom_code, flags,
                                       time_stamp);
  }

  ui::EventType type = ui::EventType::kUnknown;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      type = ui::EventType::kKeyPressed;
      break;
    case KEYEVENT_KEYUP:
      type = ui::EventType::kKeyReleased;
      break;
    default:
      DCHECK(false);
  }

  ui::DomKey dom_key = ui::DomKey::NONE;
  return ui::KeyEvent(type, key_code, dom_code, flags, dom_key, time_stamp);
}

input::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNativeOhos::TranslateWebKeyEvent(
    const CefKeyEvent& key_event) const {
  ui::KeyEvent ui_event = TranslateUiKeyEvent(key_event);
  if (key_event.type == KEYEVENT_CHAR) {
    return input::NativeWebKeyboardEvent(ui_event, key_event.character);
  }
  return input::NativeWebKeyboardEvent(ui_event);
}
