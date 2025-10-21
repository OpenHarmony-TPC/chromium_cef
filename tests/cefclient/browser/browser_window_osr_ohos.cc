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

#include "tests/cefclient/browser/browser_window_osr_ohos.h"

#include <algorithm>

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"

#include <native_window/external_window.h>
#include <stdint.h>
#include <sys/mman.h>
#include <algorithm>
#include <cmath>
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

using namespace ohos::adapter::xcomponent;

namespace client {

namespace {

// Static BrowserWindowOsrOhos::EventFilter needs to forward touch events
// to correct browser, so we maintain a vector of all windows.
std::vector<BrowserWindowOsrOhos*> g_browser_windows;
}  // namespace

BrowserWindowOsrOhos::BrowserWindowOsrOhos(BrowserWindow::Delegate* delegate,
                                           bool with_controls,
                                           const std::string& startup_url,
                                           const OsrRendererSettings& settings)
    : BrowserWindow(delegate),
      gl_enabled_(false),
      painting_popup_(false),
      hidden_(false),
      drag_leave_(false),
      drag_drop_(false),
      device_scale_factor_(1.0f) {
  client_handler_ =
      new ClientHandlerOsr(this, this, with_controls, startup_url);
  g_browser_windows.push_back(this);
}

BrowserWindowOsrOhos::~BrowserWindowOsrOhos() {
  g_browser_windows.erase(
      std::find(g_browser_windows.begin(), g_browser_windows.end(), this));
}

void BrowserWindowOsrOhos::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  // Windowless rendering requires Alloy style.
  DCHECK(delegate_->UseAlloyStyle());

  // Create the native window.
  Create(parent_handle);
  // Retrieve the OHOS Window ID for the parent window.
  CefWindowInfo window_info;
  window_info.SetAsWindowless(parent_handle);
  window_info.SetAsChild(parent_handle, rect);

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, window_info.runtime_style);

  // Create the browser asynchronously.
  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowOsrOhos::GetPopupConfig(CefWindowHandle temp_handle,
                                          CefWindowInfo& windowInfo,
                                          CefRefPtr<CefClient>& client,
                                          CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  windowInfo.SetAsWindowless(temp_handle);

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, windowInfo.runtime_style);

  client = client_handler_;
}

void BrowserWindowOsrOhos::ShowPopup(ClientWindowHandle parent_handle,
                                     int x,
                                     int y,
                                     size_t width,
                                     size_t height) {
  REQUIRE_MAIN_THREAD();
  DCHECK(browser_.get());

  // Create the native window.
  Create(parent_handle);

  // Send resize notification so the compositor is assigned the correct
  // viewport size and begins rendering.
  browser_->GetHost()->WasResized();

  Show();
}

void BrowserWindowOsrOhos::Show() {
  REQUIRE_MAIN_THREAD();

  if (hidden_) {
    // Set the browser as visible.
    browser_->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  browser_->GetHost()->SetFocus(true);
}

void BrowserWindowOsrOhos::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!browser_) {
    return;
  }

  // Remove focus from the browser.
  browser_->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    browser_->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void BrowserWindowOsrOhos::SetBounds(int x,
                                     int y,
                                     size_t width,
                                     size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here.
}

void BrowserWindowOsrOhos::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
}

void BrowserWindowOsrOhos::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  {
    base::AutoLock lock_scope(lock_);
    if (device_scale_factor == device_scale_factor_) {
      return;
    }

    // Apply some sanity checks.
    if (device_scale_factor < 1.0f || device_scale_factor > 4.0f) {
      return;
    }

    device_scale_factor_ = device_scale_factor;
  }

  if (browser_) {
    browser_->GetHost()->NotifyScreenInfoChanged();
    browser_->GetHost()->WasResized();
  }
}

float BrowserWindowOsrOhos::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  base::AutoLock lock_scope(lock_);
  return device_scale_factor_;
}

ClientWindowHandle BrowserWindowOsrOhos::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return glarea_;
}

void BrowserWindowOsrOhos::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Detach |this| from the ClientHandlerOsr.
  static_cast<ClientHandlerOsr*>(client_handler_.get())->DetachOsrDelegate();

  UnregisterDragDrop();

  DisableGL();
}

bool BrowserWindowOsrOhos::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                             CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  return false;
}

void BrowserWindowOsrOhos::GetViewRect(CefRefPtr<CefBrowser> browser,
                                       CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  rect.x = 0;
  rect.y = 0;

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // The simulated screen and view rectangle are the same. This is necessary
  // for popup menus to be located and sized inside the view.
  auto window_rect = WindowAdapter::GetInstance().GetInitialBounds();
  int width = window_rect.width;
  int height = window_rect.height;
  rect = CefRect(0, 0, width, height);
}

bool BrowserWindowOsrOhos::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                          int viewX,
                                          int viewY,
                                          int& screenX,
                                          int& screenY) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // Get the widget position in the window.
  return true;
}

bool BrowserWindowOsrOhos::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                         CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();

  CefRect view_rect;
  GetViewRect(browser, view_rect);

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  screen_info.device_scale_factor = device_scale_factor;

  // The screen info rectangles are used by the renderer to create and position
  // popups. Keep popups inside the view rectangle.
  screen_info.rect = view_rect;
  screen_info.available_rect = view_rect;
  return true;
}

void BrowserWindowOsrOhos::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                       bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (!show) {
    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

void BrowserWindowOsrOhos::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                       const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }
}

bool BrowserWindowOsrOhos::RenderOnScreen(void* window,
                                          const void* osr_buffer,
                                          int w,
                                          int h) {
  uint8_t* sourceData = (uint8_t*)osr_buffer;
  uint8_t* targetData = NULL;
  Region region{nullptr, 0};
  int32_t ret = 0;
  int32_t stride = 0x8;
  BufferHandle* bufferHandle = NULL;
  uint8_t* mappedAddr = NULL;
  int32_t code;

  OHNativeWindowBuffer* buffer = nullptr;
  int fenceFd;

  OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(window);
  if (nativeWindow == NULL) {
    LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
    goto err;
  }

  code = SET_BUFFER_GEOMETRY;
  ret = OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, w, h);
  if (ret != 0) {
    LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
    goto err;
  }

  code = SET_STRIDE;
  ret = OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, stride);
  if (ret != 0) {
    LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
    goto err;
  }

  ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindow, &buffer,
                                                  &fenceFd);
  if (ret != 0) {
    LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
    goto err;
  }

  bufferHandle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
  mappedAddr = static_cast<uint8_t*>(
      mmap(bufferHandle->virAddr, bufferHandle->size, PROT_READ | PROT_WRITE,
           MAP_SHARED, bufferHandle->fd, 0));
  if (mappedAddr == MAP_FAILED) {
    LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
    goto err;
  }

  if (bufferHandle->format == 12) {
    int bpp = 4;  // src and dest: RGBA8888
    targetData = mappedAddr;
    for (int row = 0; row < h; row++) {
      memcpy(targetData, sourceData, w * bpp);
      sourceData += w * bpp;
      targetData += bufferHandle->stride;
    }

    OH_NativeWindow_NativeWindowFlushBuffer(nativeWindow, buffer, fenceFd,
                                            region);

    int result = munmap(mappedAddr, bufferHandle->size);
    // munmap failed
    if (result == -1) {
      LOG(ERROR) << "RenderOnScreen failed:" << __LINE__;
      goto err;
    }
  }
  return true;
err:
  return false;
}

void BrowserWindowOsrOhos::OnPaint(CefRefPtr<CefBrowser> browser,
                                   CefRenderHandler::PaintElementType type,
                                   const CefRenderHandler::RectList& dirtyRects,
                                   const void* buffer,
                                   int width,
                                   int height) {
  CEF_REQUIRE_UI_THREAD();
  // render to screen
  WindowWidgetType nextWindowid =
      WindowAdapter::GetInstance().GetWindowWidgetId() + 1;
  WindowIdType windowId = WindowAdapter::GetInstance().GetWindowId(nextWindowid);
  WindowType window = WindowAdapter::GetInstance().GetWindow(windowId);

  bool result = RenderOnScreen((void*)window, buffer, width, height);
  if (!result) {
    LOG(ERROR) << "RenderOnScreen failed";
  }
}

void BrowserWindowOsrOhos::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();
}

bool BrowserWindowOsrOhos::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();

  DragReset();

  // Send drag enter event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  browser->GetHost()->DragTargetDragEnter(drag_data, ev, allowed_ops);

  return true;
}

void BrowserWindowOsrOhos::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::UpdateAccessibilityTree(CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::Create(ClientWindowHandle parent_handle) {
  REQUIRE_MAIN_THREAD();
}

bool BrowserWindowOsrOhos::IsOverPopupWidget(int x, int y) const {
  return false;
}

int BrowserWindowOsrOhos::GetPopupXOffset() const {
  return 0;
}

int BrowserWindowOsrOhos::GetPopupYOffset() const {
  return 0;
}

void BrowserWindowOsrOhos::ApplyPopupOffset(int& x, int& y) const {
  if (IsOverPopupWidget(x, y)) {
    x += GetPopupXOffset();
    y += GetPopupYOffset();
  }
}

void BrowserWindowOsrOhos::EnableGL() {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::DisableGL() {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrOhos::RegisterDragDrop() {
  REQUIRE_MAIN_THREAD();

  // Succession of CEF d&d calls:
  // 1. DragTargetDragEnter
  // 2. DragTargetDragOver
  // 3. DragTargetDragLeave - optional
  // 4. DragSourceSystemDragEnded - optional, to cancel dragging
  // 5. DragTargetDrop
  // 6. DragSourceEndedAt
  // 7. DragSourceSystemDragEnded
  // Source widget events.
}

void BrowserWindowOsrOhos::UnregisterDragDrop() {
  // Drag events are unregistered in OnBeforeClose by calling
  // g_signal_handlers_disconnect_matched.
}

void BrowserWindowOsrOhos::DragReset() {}

}  // namespace client
