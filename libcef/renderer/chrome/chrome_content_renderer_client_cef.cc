// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/renderer/chrome/chrome_content_renderer_client_cef.h"

#include "arkweb/build/features/features.h"
#include "cef/libcef/renderer/render_frame_observer.h"
#include "cef/libcef/renderer/render_manager.h"
#include "cef/libcef/renderer/thread_util.h"
#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_client.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_render_frame_observer.h"
#endif  // ARKWEB_NO_STATE_PREFETCH

#if BUILDFLAG(ARKWEB_PRINT)
#include "libcef/renderer/extensions/ohos_print_render_frame_helper_delegate.h"
#endif  // BUILDFLAG(ARKWEB_PRINT)

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
#include "libcef/renderer/chrome_safe_browsing_error_page_controller_delegate_impl.h"
#endif

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
#include "third_party/blink/public/web/web_local_frame.h"
#endif

#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
#include "components/js_injection/renderer/js_communication.h"
#endif

#if BUILDFLAG(ARKWEB_MIXED_CONTENT)
#include "cef/ohos_cef_ext/libcef/renderer/arkweb_content_settings_client.h"
#endif

ChromeContentRendererClientCef::ChromeContentRendererClientCef()
    : render_manager_(new CefRenderManager) {}

ChromeContentRendererClientCef::~ChromeContentRendererClientCef() = default;

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentRendererClientCef::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT()) {
    return render_task_runner_;
  }
  return nullptr;
}

void ChromeContentRendererClientCef::RenderThreadStarted() {
  ChromeContentRendererClient::RenderThreadStarted();

  render_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void ChromeContentRendererClientCef::RenderThreadConnected() {
  ChromeContentRendererClient::RenderThreadConnected();

  render_manager_->RenderThreadConnected();
}

#if BUILDFLAG(ARKWEB_SAFEBROWSING) && !BUILDFLAG(ARKWEB_NWEB_EX)
void ChromeContentRendererClientCef::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  ChromeSafeBrowsingErrorPageControllerDelegateImpl::Get(render_frame)
      ->PrepareForErrorPage();
}
#endif

void ChromeContentRendererClientCef::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Will delete itself when no longer needed.
  CefRenderFrameObserver* render_frame_observer =
      new CefRenderFrameObserver(render_frame);

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
  new prerender::NoStatePrefetchRenderFrameObserver(render_frame);
  if (!render_frame->IsMainFrame()) {
    auto* main_frame_no_state_prefetch_helper =
        prerender::NoStatePrefetchHelper::Get(
            render_frame->GetMainRenderFrame());
    if (main_frame_no_state_prefetch_helper) {
      // Avoid any race conditions from having the browser tell subframes that
      // they're no-state prefetching.
      new prerender::NoStatePrefetchHelper(
          render_frame,
          main_frame_no_state_prefetch_helper->histogram_prefix());
    }
  }
#endif  // ARKWEB_NO_STATE_PREFETCH

  bool browser_created;
  std::optional<bool> is_windowless;
  std::optional<bool> print_preview_enabled;
  render_manager_->RenderFrameCreated(render_frame, render_frame_observer,
                                      browser_created, is_windowless,
                                      print_preview_enabled);
  if (browser_created) {
    OnBrowserCreated(render_frame->GetWebView(), is_windowless);
  }

  if (print_preview_enabled.has_value()) {
    // This value will be used when the when ChromeContentRendererClient
    // creates the new ChromePrintRenderFrameHelperDelegate below.
    ChromePrintRenderFrameHelperDelegate::SetNextPrintPreviewEnabled(
        *print_preview_enabled);
  }
  ChromeContentRendererClient::RenderFrameCreated(render_frame);

#if BUILDFLAG(ARKWEB_MIXED_CONTENT)
  new ArkWebContentSettingsClient(render_frame);
#endif

#if BUILDFLAG(ARKWEB_SAFEBROWSING) && !BUILDFLAG(ARKWEB_NWEB_EX)
  new ChromeSafeBrowsingErrorPageControllerDelegateImpl(render_frame);
#endif
}

void ChromeContentRendererClientCef::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  ChromeContentRendererClient::WebViewCreated(web_view, was_created_by_renderer,
                                              outermost_origin);

  bool browser_created;
  std::optional<bool> is_windowless;
  std::optional<bool> print_preview_enabled;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless,
                                  print_preview_enabled);
  if (browser_created) {
    OnBrowserCreated(web_view, is_windowless);
  }

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
  new prerender::NoStatePrefetchClient(web_view);
#endif  // ARKWEB_NO_STATE_PREFETCH
}

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
bool ChromeContentRendererClientCef::HandleNavigation(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  // Only GETs can be overridden.
  if (!request.HttpMethod().Equals("GET")) {
    return false;
  }

  // Any navigation from loadUrl, and goBack/Forward are considered application-
  // initiated and hence will not yield a shouldOverrideUrlLoading() callback.
  // Webview classic does not consider reload application-initiated so we
  // continue the same behavior.
  bool application_initiated = type == blink::kWebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect) {
    return false;
  }

  bool is_outermost_main_frame = frame->IsOutermostMainFrame();
  const GURL& gurl = request.Url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_outermost_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme))) {
    return false;
  }

  blink::WebView* web_view = render_frame->GetWebView();

  bool browser_created;
  std::optional<bool> is_windowless;
  std::optional<bool> print_preview_enabled;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless,
                                  print_preview_enabled);
  if (!browser_created) {
    return false;
  }

  blink::WebLocalFrame* local_frame = render_frame->GetWebFrame();
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(local_frame->Top());

  if (!browserPtr) {
    return false;
  }

  bool ignore_navigation = false;
  bool has_user_gesture = request.HasUserGesture();
  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(local_frame);

  ignore_navigation = framePtr->ShouldOverrideUrlLoading(
      gurl.possibly_invalid_spec(), request.HttpMethod().Utf8(),
      has_user_gesture, is_redirect, is_outermost_main_frame);

  return ignore_navigation;
}
#endif

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
std::unique_ptr<blink::WebPrescientNetworking>
ChromeContentRendererClientCef::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

bool ChromeContentRendererClientCef::IsPrefetchOnly(
    content::RenderFrame* render_frame) {
  return prerender::NoStatePrefetchHelper::IsPrefetching(render_frame);
}
#endif  // BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)

void ChromeContentRendererClientCef::DevToolsAgentAttached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeContentRendererClientCef::DevToolsAgentAttached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentAttached();
}

void ChromeContentRendererClientCef::DevToolsAgentDetached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeContentRendererClientCef::DevToolsAgentDetached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentDetached();
}

void ChromeContentRendererClientCef::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  ChromeContentRendererClient::ExposeInterfacesToBrowser(binders);

  render_manager_->ExposeInterfacesToBrowser(binders);
}

void ChromeContentRendererClientCef::OnBrowserCreated(
    blink::WebView* web_view,
    std::optional<bool> is_windowless) {
#if BUILDFLAG(IS_MAC)
  const bool windowless = is_windowless.has_value() && *is_windowless;

  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  web_view->SetUseExternalPopupMenusThisInstance(!windowless);
#endif
}

void ChromeContentRendererClientCef::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  ChromeContentRendererClient::RunScriptsAtDocumentStart(render_frame);
#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  if (communication) {
    communication->RunScriptsAtDocumentStart();
  }
#endif
}

void ChromeContentRendererClientCef::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  ChromeContentRendererClient::RunScriptsAtDocumentEnd(render_frame);
#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  if (communication) {
    communication->RunScriptsAtDocumentEnd();
  }
#endif  // ARKWEB_JS_ON_DOCUMENT_END
}

#if BUILDFLAG(ARKWEB_JSPROXY)
void ChromeContentRendererClientCef::RunScriptsAtHeadReady(
    content::RenderFrame* render_frame) {
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);

  if (communication) {
    communication->RunScriptsAtHeadReady();
  }
}
#endif
