// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple/simple_handler.h"

#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/cef_path_util.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#if defined(OS_OHOS)
#include <native_window/external_window.h>
#include <stdint.h>
#include <sys/mman.h>
#include <algorithm>
#include <cmath>
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos/adapter/window/app_window_adapter.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#endif

#if defined(OS_OHOS)
using namespace ohos::adapter::xcomponent;
#endif

namespace {

SimpleHandler* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

SimpleHandler::SimpleHandler(bool is_alloy_style)
    : is_alloy_style_(is_alloy_style) {
  DCHECK(!g_instance);
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  } else if (is_alloy_style_) {
    // Set the title of the window using platform APIs.
    PlatformTitleChange(browser, title);
  }
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Sanity-check the configured runtime style.
  CHECK_EQ(is_alloy_style_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME,
           browser->GetHost()->GetRuntimeStyle());

  // Add to the list of existing browsers.
  browser_list_.push_back(browser);
}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  if (browser_list_.size() == 1) {
    // Set a flag to indicate that the window close should be allowed.
    is_closing_ = true;
  }

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove from the list of existing browsers.
  BrowserList::iterator bit = browser_list_.begin();
  for (; bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (browser_list_.empty()) {
    // All browser windows have closed. Quit the application message loop.
    CefQuitMessageLoop();
  }
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Allow Chrome to show the error page.
  if (!is_alloy_style_) {
    return;
  }

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  // Display a load error message using a data: URI.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

bool SimpleHandler::CanDownload(CefRefPtr<CefBrowser> browser,
                                const CefString& url,
                                const CefString& request_method) {
  return true;
}

bool SimpleHandler::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  CefString download_dir;
  if (CefGetPath(PK_DIR_TEMP, download_dir) && !download_dir.empty()) {
    std::string download_path =
        download_dir.ToString() + "/" + suggested_name.ToString();
    CefString file_path = CefString(download_path);
    callback->Continue(file_path, true);
    return true;
  }
  return false;
}

void SimpleHandler::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {}

void SimpleHandler::ShowMainWindow() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::ShowMainWindow, this));
    return;
  }

  if (browser_list_.empty()) {
    return;
  }

  auto main_browser = browser_list_.front();

  if (auto browser_view = CefBrowserView::GetForBrowser(main_browser)) {
    // Show the window using the Views framework.
    if (auto window = browser_view->GetWindow()) {
      window->Show();
    }
  } else if (is_alloy_style_) {
    PlatformShowWindow(main_browser);
  }
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                       force_close));
    return;
  }

  if (browser_list_.empty()) {
    return;
  }

  BrowserList::const_iterator it = browser_list_.begin();
  for (; it != browser_list_.end(); ++it) {
    (*it)->GetHost()->CloseBrowser(force_close);
  }
}

#if !defined(OS_MAC)
void SimpleHandler::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  NOTIMPLEMENTED();
}
#endif

bool SimpleHandler::RenderOnScreen(void* window,
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

void SimpleHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
  LOG(INFO) << "call API: " << __FUNCTION__ << ": " << __FILE__ << " line: " << __LINE__;
  // Default OSR widget size.
  auto window_rect = WindowAdapter::GetInstance().GetInitialBounds();
  int width = window_rect.width;
  int height = window_rect.height;
  LOG(INFO) << "call API: width" << width << "  height: " << height;
  rect = CefRect(0, 0, width, height);
}

void SimpleHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const void* buffer,
                            int width,
                            int height) {
  LOG(INFO) << "call API: " << __FUNCTION__ << ": " << width << ":" << height;
  // render to screen
  WindowWidgetType nextWindowid =
      WindowAdapter::GetInstance().GetWindowWidgetId();
  WindowIdType windowId = WindowAdapter::GetInstance().GetWindowId(nextWindowid);
  WindowType window = WindowAdapter::GetInstance().GetWindow(windowId);
  bool result = RenderOnScreen((void*)window, buffer, width, height);
  if (!result) {
    LOG(ERROR) << "RenderOnScreen failed";
  }

}