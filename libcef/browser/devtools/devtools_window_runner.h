// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
#pragma once

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cef/include/cef_client.h"

#if BUILDFLAG(ARKWEB_DEVTOOLS)
#include "ohos_cef_ext/libcef/common/cef_open_devtools_ext_opt.h"
class CefDevToolsFrontend;
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)

class CefBrowserHostBase;
class ChromeBrowserHostImpl;

// Parameters passed to ShowDevTools.
struct CefShowDevToolsParams {
  CefShowDevToolsParams(const CefWindowInfo& windowInfo,
                        CefRefPtr<CefClient> client,
                        const CefBrowserSettings& settings,
                        const CefPoint& inspect_element_at)
      : window_info_(windowInfo),
        client_(client),
        settings_(settings),
        inspect_element_at_(inspect_element_at) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefBrowserSettings settings_;
  CefPoint inspect_element_at_;
};

// Creates and runs a DevTools window instance. Only accessed on the UI thread.
class CefDevToolsWindowRunner final {
 public:
  CefDevToolsWindowRunner() = default;

  CefDevToolsWindowRunner(const CefDevToolsWindowRunner&) = delete;
  CefDevToolsWindowRunner& operator=(const CefDevToolsWindowRunner&) = delete;

  void ShowDevTools(CefBrowserHostBase* opener,
                    std::unique_ptr<CefShowDevToolsParams> params);
#if BUILDFLAG(ARKWEB_DEVTOOLS)
  static void StaticShowDevToolsWith(
      const CefString& source_id,
      const CefString& target_id,
      CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> devtools_message_handler,
      const CefPoint& inspect_element_at,
      const CefOpenDevToolsExtOpt& ext_opt);
  void ShowDevToolsWith(
      CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
      CefBrowserHostBase* inspected_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> devtools_message_handler,
      const CefPoint& inspect_element_at);

  void ShowDevToolsWithByPb(
      CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
      CefBrowserHostBase* inspected_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> devtools_message_handler,
      const CefPoint& inspect_element_at,
      const CefOpenDevToolsExtOpt& ext_opt);
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
  void CloseDevTools();
  bool HasDevTools();

  std::unique_ptr<CefShowDevToolsParams> TakePendingParams();

  void SetDevToolsBrowserHost(
      base::WeakPtr<ChromeBrowserHostImpl> browser_host);

 private:
#if BUILDFLAG(ARKWEB_DEVTOOLS)
  void OnFrontEndDestroyed();
  static void OnStaticFrontEndDestroyed();
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)

  std::unique_ptr<CefShowDevToolsParams> pending_params_;

  base::WeakPtr<ChromeBrowserHostImpl> browser_host_;

#if BUILDFLAG(ARKWEB_DEVTOOLS)
  raw_ptr<CefDevToolsFrontend> devtools_frontend_ = nullptr;
  static raw_ptr<CefDevToolsFrontend> static_devtools_frontend_;
  base::WeakPtrFactory<CefDevToolsWindowRunner> weak_ptr_factory_{this};
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
