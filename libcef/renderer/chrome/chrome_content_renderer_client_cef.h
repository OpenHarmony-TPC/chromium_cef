// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_
#define CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_

#include <memory>

#include "arkweb/build/features/features.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/renderer/chrome_content_renderer_client.h"

class CefRenderManager;

// CEF override of ChromeContentRendererClient.
class ChromeContentRendererClientCef : public ChromeContentRendererClient {
 public:
  ChromeContentRendererClientCef();

  ChromeContentRendererClientCef(const ChromeContentRendererClientCef&) =
      delete;
  ChromeContentRendererClientCef& operator=(
      const ChromeContentRendererClientCef&) = delete;

  ~ChromeContentRendererClientCef() override;

  // Render thread task runner.
  base::SingleThreadTaskRunner* render_task_runner() const {
    return render_task_runner_.get();
  }

  // Returns the task runner for the current thread. Returns NULL if the current
  // thread is not the main render process thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetCurrentTaskRunner();

  // ChromeContentRendererClient overrides.
  void RenderThreadStarted() override;
  void RenderThreadConnected() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  void DevToolsAgentAttached() override;
  void DevToolsAgentDetached() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame) override;
#endif  // ARKWEB_NO_STATE_PREFETCH

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  bool HandleNavigation(content::RenderFrame* render_frame,
                        blink::WebFrame* frame,
                        const blink::WebURLRequest& request,
                        blink::WebNavigationType type,
                        blink::WebNavigationPolicy default_policy,
                        bool is_redirect) override;
#endif

#if BUILDFLAG(ARKWEB_SAFEBROWSING) && !BUILDFLAG(ARKWEB_NWEB_EX)
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
#endif
#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
#endif
#if BUILDFLAG(ARKWEB_JSPROXY)
  void RunScriptsAtHeadReady(content::RenderFrame* render_frame) override;
#endif
 private:
  void OnBrowserCreated(blink::WebView* web_view,
                        std::optional<bool> is_windowless);

  std::unique_ptr<CefRenderManager> render_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;
};

#endif  // CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_
