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

#include "build/buildflag.h"
#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_DEVTOOLS)
#include "cef/libcef/browser/devtools/devtools_window_runner.h"

#include "cef/ohos_cef_ext/libcef/browser/devtools/arkweb/devtools_frontend.h"

void CefDevToolsWindowRunner::OnFrontEndDestroyed() {
  LOG(INFO) << "OnFrontEndDestroyed";
  devtools_frontend_ = nullptr;
}

void CefDevToolsWindowRunner::ShowDevToolsWith(
    CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
    CefBrowserHostBase* inspected_browser,
    CefRefPtr<CefDevToolsMessageHandlerDelegate> devtools_message_handler,
    const CefPoint& inspect_element_at) {
  CEF_REQUIRE_UIT();
  auto* arkweb_host_ext = static_cast<ArkWebBrowserHostExtImpl*>(frontend_browser.get());
  auto alloy_frontend_browser = arkweb_host_ext->AsAlloyBrowserHostImpl();
  if (devtools_frontend_ &&
      devtools_frontend_->GetFrontendBrowser() == alloy_frontend_browser.get()) {
    LOG(INFO) << "ShowDevToolsWith, reuse devtools_frontend";
    if (!inspect_element_at.IsEmpty()) {
      devtools_frontend_->InspectElementAt(inspect_element_at.x,
                                           inspect_element_at.y);
    }
    devtools_frontend_->Focus();
    return;
  }

  if (!alloy_frontend_browser) {
    NOTIMPLEMENTED();
  } else {
    auto* web_contents = inspected_browser->GetWebContents();
    devtools_frontend_ = CefDevToolsFrontend::ShowWith(
        alloy_frontend_browser.get(), std::move(devtools_message_handler),
        web_contents, inspect_element_at,
        base::BindOnce(&CefDevToolsWindowRunner::OnFrontEndDestroyed,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CefDevToolsWindowRunner::ShowDevToolsWithByPb(
    CefRefPtr<ArkWebBrowserHostExt> frontend_browser,
    CefBrowserHostBase* inspected_browser,
    CefRefPtr<CefDevToolsMessageHandlerDelegate> devtools_message_handler,
    const CefPoint& inspect_element_at,
    const CefOpenDevToolsExtOpt& ext_opt) {
  CEF_REQUIRE_UIT();
  auto* arkweb_host_ext = static_cast<ArkWebBrowserHostExtImpl*>(frontend_browser.get());
  auto alloy_frontend_browser = arkweb_host_ext->AsAlloyBrowserHostImpl();
  if (devtools_frontend_ &&
      devtools_frontend_->GetFrontendBrowser() == alloy_frontend_browser.get()) {
    LOG(INFO) << "ShowDevToolsWithByPb, reuse devtools_frontend";
    if (!inspect_element_at.IsEmpty()) {
      devtools_frontend_->InspectElementAt(inspect_element_at.x,
                                           inspect_element_at.y);
    }
    devtools_frontend_->Focus();
    return;
  }

  if (!alloy_frontend_browser) {
    NOTIMPLEMENTED();
  } else {
    auto* web_contents = inspected_browser->GetWebContents();
    devtools_frontend_ = CefDevToolsFrontend::ShowWithByPb(
        alloy_frontend_browser.get(), std::move(devtools_message_handler),
        web_contents, inspect_element_at,
        base::BindOnce(&CefDevToolsWindowRunner::OnFrontEndDestroyed,
                       weak_ptr_factory_.GetWeakPtr()),
        ext_opt);
  }
}
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)
