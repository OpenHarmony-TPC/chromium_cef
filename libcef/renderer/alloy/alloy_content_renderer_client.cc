// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/alloy_content_renderer_client.h"

#include <utility>

#include "build/build_config.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/browser/alloy/alloy_content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/common/alloy/alloy_content_client.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/features/runtime_checks.h"
#include "libcef/renderer/alloy/alloy_render_thread_observer.h"
#include "libcef/renderer/alloy/url_loader_throttle_provider_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/extensions/extensions_renderer_client.h"
#include "libcef/renderer/extensions/print_render_frame_helper_delegate.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_manager.h"
#include "libcef/renderer/thread_util.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/browser_exposed_renderer_interfaces.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/chrome_key_systems.h"
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/pdf/common/internal_plugin_helpers.h"
#include "components/pdf/renderer/internal_plugin_renderer_helpers.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "printing/print_settings.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

#if defined(OHOS_FCP) || defined(OHOS_ARKWEB_ADBLOCK)
#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"
#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/content/renderer/user_subresource_filter_agent.h"
#include "components/subresource_filter/content/renderer/user_unverified_ruleset_dealer.h"
#endif

#if BUILDFLAG(IS_OHOS)
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/js_injection/renderer/js_communication.h"
#include "libcef/renderer/alloy/alloy_content_settings_client.h"
#endif // BUILDFLAG(IS_OHOS)

#if defined(OHOS_PRINT)
#include "libcef/renderer/extensions/ohos_print_render_frame_helper_delegate.h"
#endif // defined(OHOS_PRINT)

#if defined(OHOS_NO_STATE_PREFETCH)
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_client.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/no_state_prefetch/renderer/prerender_render_frame_observer.h"
#endif  // defined(OHOS_NO_STATE_PREFETCH)

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "chrome/common/chrome_features.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_web_view_helper.h"
#include "printing/metafile_agent.h"
#endif

#include "libcef/renderer/alloy/ohos_safe_browsing_error_page_controller_delegate_impl.h"

#ifdef OHOS_NOTIFICATION
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif // OHOS_NOTIFICATION

AlloyContentRendererClient::AlloyContentRendererClient()
    : main_entry_time_(base::TimeTicks::Now()),
      render_manager_(new CefRenderManager) {
  if (extensions::ExtensionsEnabled()) {
    extensions_client_.reset(new extensions::CefExtensionsClient);
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_renderer_client_ =
        std::make_unique<extensions::CefExtensionsRendererClient>(this);
    extensions::ExtensionsRendererClient::Set(
        extensions_renderer_client_.get());
  }
}

AlloyContentRendererClient::~AlloyContentRendererClient() {}

// static
AlloyContentRendererClient* AlloyContentRendererClient::Get() {
  REQUIRE_ALLOY_RUNTIME();
  return static_cast<AlloyContentRendererClient*>(
      CefAppManager::Get()->GetContentClient()->renderer());
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentRendererClient::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT()) {
    return render_task_runner_;
  }
  return nullptr;
}

void AlloyContentRendererClient::RunSingleProcessCleanup() {
  DCHECK(content::RenderProcessHost::run_renderer_in_process());

  // Make sure the render thread was actually started.
  if (!render_task_runner_.get()) {
    return;
  }

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    RunSingleProcessCleanupOnUIThread();
  } else {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &AlloyContentRendererClient::RunSingleProcessCleanupOnUIThread,
            base::Unretained(this)));
  }

  // Wait for the render thread cleanup to complete. Spin instead of using
  // base::WaitableEvent because calling Wait() is not allowed on the UI
  // thread.
  bool complete = false;
  do {
    {
      base::AutoLock lock_scope(single_process_cleanup_lock_);
      complete = single_process_cleanup_complete_;
    }
    if (!complete) {
      base::PlatformThread::YieldCurrentThread();
    }
  } while (!complete);
}

void AlloyContentRendererClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner*) {
#ifdef OHOS_NOTIFICATION
  if (!(*base::CommandLine::ForCurrentProcess()).HasSwitch(
      switches::kEnableNwebEx)) {
    // TODO(cef):Enable these once the implementation supports it.
    blink::WebRuntimeFeatures::EnableNotifications(false);
    blink::WebRuntimeFeatures::EnablePushMessaging(false);
  }
#else
  // TODO(cef):Enable these once the implementation supports it.
  blink::WebRuntimeFeatures::EnableNotifications(false);
  blink::WebRuntimeFeatures::EnablePushMessaging(false);
#endif // OHOS_NOTIFICATION
}

void AlloyContentRendererClient::RenderThreadStarted() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  render_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  observer_ = std::make_unique<AlloyRenderThreadObserver>();
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
  visited_link_slave_ = std::make_unique<visitedlink::VisitedLinkReader>();

  content::RenderThread* thread = content::RenderThread::Get();

  const bool is_extension = CefRenderManager::IsExtensionProcess();

  thread->SetRendererProcessType(
      is_extension
          ? blink::scheduler::WebRendererProcessType::kExtensionRenderer
          : blink::scheduler::WebRendererProcessType::kRenderer);

  if (is_extension) {
    // The process name was set to "Renderer" in RendererMain(). Update it to
    // "Extension Renderer" to highlight that it's hosting an extension.
    base::CurrentProcess::GetInstance().SetProcessType(
        base::CurrentProcessType::PROCESS_RENDERER_EXTENSION);
  }

  thread->AddObserver(observer_.get());
#if defined(OHOS_FCP) || defined(OHOS_ARKWEB_ADBLOCK)
  subresource_filter_ruleset_dealer_ =
      std::make_unique<subresource_filter::UnverifiedRulesetDealer>();
  thread->AddObserver(subresource_filter_ruleset_dealer_.get());
  subresource_filter_user_ruleset_dealer_.reset(
      new subresource_filter::UserUnverifiedRulesetDealer());
  thread->AddObserver(subresource_filter_user_ruleset_dealer_.get());
#endif
  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_ = std::make_unique<SpellCheck>(this);
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running_in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::CurrentThread::Get()->AddDestructionObserver(this);
  }

#if BUILDFLAG(IS_MAC)
  {
    base::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
    base::ScopedCFTypeRef<CFStringRef> value;

    // If the command-line switch is specified then set the value that will be
    // checked in RenderThreadImpl::Init(). Otherwise, remove the application-
    // level value.
    if (command_line->HasSwitch(switches::kDisableScrollBounce)) {
      value.reset(base::SysUTF8ToCFStringRef("false"));
    }

    CFPreferencesSetAppValue(key, value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
  }
#endif  // BUILDFLAG(IS_MAC)

  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RenderThreadStarted();
#if defined(OHOS_ARKWEB_EXTENSIONS)
    blink::WebSecurityPolicy::RegisterURLSchemeAsExtension(
        blink::WebString::FromASCII(extensions::kExtensionScheme));
    blink::WebSecurityPolicy::RegisterURLSchemeAsExtension(
        blink::WebString::FromASCII(extensions::kArkwebExtensionScheme));
#endif
  }
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
bool AlloyContentRendererClient::AllowPopup() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions_renderer_client_->AllowPopup();
#else
  return false;
#endif
}

blink::ProtocolHandlerSecurityLevel
AlloyContentRendererClient::GetProtocolHandlerSecurityLevel() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions_renderer_client_->GetProtocolHandlerSecurityLevel();
#else
  return blink::ProtocolHandlerSecurityLevel::kStrict;
#endif
}
#endif

void AlloyContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(web_cache_impl_.get())),
      task_runner);

  binders->Add<visitedlink::mojom::VisitedLinkNotificationSink>(
      visited_link_slave_->GetBindCallback(), task_runner);

  if (spellcheck_) {
    binders->Add<spellcheck::mojom::SpellChecker>(
        base::BindRepeating(
            [](SpellCheck* spellcheck,
               mojo::PendingReceiver<spellcheck::mojom::SpellChecker>
                   receiver) { spellcheck->BindReceiver(std::move(receiver)); },
            base::Unretained(spellcheck_.get())),
        task_runner);
  }

  render_manager_->ExposeInterfacesToBrowser(binders);
}

void AlloyContentRendererClient::RenderThreadConnected() {
  // Register extensions last because it will trigger WebKit initialization.
  blink::WebScriptController::RegisterExtension(
      extensions_v8::LoadTimesExtension::Get());

  render_manager_->RenderThreadConnected();
}

#if BUILDFLAG(IS_OHOS) && !defined(OHOS_NWEB_EX)
void AlloyContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
    alternative_error_page_info,
    std::string* error_html) {
  AlloySafeBrowsingErrorPageControllerDelegateImpl::Get(render_frame)
       ->PrepareForErrorPage();
}
#endif


void AlloyContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if defined(OHOS_FCP) || defined(OHOS_ARKWEB_ADBLOCK)
  // Owned by |render_frame|.
  page_load_metrics::MetricsRenderFrameObserver* metrics_render_frame_observer =
      new page_load_metrics::MetricsRenderFrameObserver(render_frame);
  // There is no render thread, thus no UnverifiedRulesetDealer in
  // ChromeRenderViewTests.
  if (subresource_filter_ruleset_dealer_) {
    // Create AdResourceTracker to tracker ad resource loads at the chrome
    // layer.
    auto ad_resource_tracker =
        std::make_unique<subresource_filter::AdResourceTracker>();
    metrics_render_frame_observer->SetAdResourceTracker(
        ad_resource_tracker.get());

    auto* subresource_filter_agent =
        new subresource_filter::SubresourceFilterAgent(
            render_frame, subresource_filter_ruleset_dealer_.get(),
            std::move(ad_resource_tracker));
    subresource_filter_agent->Initialize();
  }

  page_load_metrics::MetricsRenderFrameObserver*
      user_metrics_render_frame_observer =
          new page_load_metrics::MetricsRenderFrameObserver(render_frame);
  if (subresource_filter_user_ruleset_dealer_) {
    auto user_ad_resource_tracker =
        std::make_unique<subresource_filter::AdResourceTracker>();
    user_metrics_render_frame_observer->SetAdResourceTracker(
        user_ad_resource_tracker.get());
    auto* user_subresource_filter_agent =
        new subresource_filter::UserSubresourceFilterAgent(
            render_frame, subresource_filter_user_ruleset_dealer_.get(),
            std::move(user_ad_resource_tracker));
    user_subresource_filter_agent->Initialize();
  }
#endif

#if BUILDFLAG(IS_OHOS)
  new js_injection::JsCommunication(render_frame);
  new AlloyContentSettingsClient(render_frame);
  new AlloySafeBrowsingErrorPageControllerDelegateImpl(render_frame);
#endif
  auto render_frame_observer = new CefRenderFrameObserver(render_frame);

#if BUILDFLAG(IS_OHOS) && defined(OHOS_EX_EXCEPTION_LIST)
  AlloyContentSettingsClient* alloy_content_settings_client =
      new AlloyContentSettingsClient(render_frame);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser) &&
      observer_.get() && alloy_content_settings_client) {
    alloy_content_settings_client->SetContentSettingRules(
        observer_->content_setting_rules());
  }
#endif

#if defined(OHOS_NO_STATE_PREFETCH)
  new prerender::PrerenderRenderFrameObserver(render_frame);
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
#endif  // defined(OHOS_NO_STATE_PREFETCH)

  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RenderFrameCreated(
        render_frame, render_frame_observer->registry());

    render_frame_observer->associated_interfaces()
        ->AddInterface<extensions::mojom::MimeHandlerViewContainerManager>(
            base::BindRepeating(
                &extensions::MimeHandlerViewContainerManager::BindReceiver,
                render_frame->GetRoutingID()));
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    new SpellCheckProvider(render_frame, spellcheck_.get(), this);
  }

  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->RenderFrameCreated(render_frame, render_frame_observer,
                                      browser_created, is_windowless);
  if (browser_created) {
    OnBrowserCreated(render_frame->GetWebView(), is_windowless);
  }
#if defined(OHOS_PRINT)
  if (is_windowless.has_value()) {
    new printing::PrintRenderFrameHelper(
        render_frame,
        base::WrapUnique(new extensions::OhosPrintRenderFrameHelperDelegate()));
  }
#else
  if (is_windowless.has_value()) {
    new printing::PrintRenderFrameHelper(
        render_frame,
        base::WrapUnique(
            new extensions::CefPrintRenderFrameHelperDelegate(*is_windowless)));
  }
#endif

#if BUILDFLAG(IS_OHOS)
  blink::AssociatedInterfaceRegistry* associated_interfaces =
      render_frame_observer->associated_interfaces();
  autofill::PasswordAutofillAgent* password_autofill_agent =
      new autofill::PasswordAutofillAgent(render_frame, associated_interfaces);
  autofill::PasswordGenerationAgent* password_generation_agent = nullptr;
#if defined(OHOS_EX_PASSWORD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser)) {
    password_generation_agent = new autofill::PasswordGenerationAgent(
        render_frame, password_autofill_agent, associated_interfaces);
  }
#endif
  new autofill::AutofillAgent(render_frame, password_autofill_agent,
                              password_generation_agent, associated_interfaces);
#endif
}

void AlloyContentRendererClient::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless);
  if (browser_created) {
    OnBrowserCreated(web_view, is_windowless);
  }

#if defined(OHOS_NO_STATE_PREFETCH)
  new prerender::NoStatePrefetchClient(web_view);
#endif  // defined(OHOS_NO_STATE_PREFETCH)

#if defined(OHOS_ARKWEB_EXTENSIONS)
  new extensions::ExtensionWebViewHelper(web_view, outermost_origin);
#endif
}

#if defined(OHOS_NO_STATE_PREFETCH)
std::unique_ptr<blink::WebPrescientNetworking>
AlloyContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

bool AlloyContentRendererClient::IsPrefetchOnly(
    content::RenderFrame* render_frame) {
  return prerender::NoStatePrefetchHelper::IsPrefetching(render_frame);
}
#endif  // defined(OHOS_NO_STATE_PREFETCH)

bool AlloyContentRendererClient::IsPluginHandledExternally(
    content::RenderFrame* render_frame,
    const blink::WebElement& plugin_element,
    const GURL& original_url,
    const std::string& mime_type) {
  if (!extensions::ExtensionsEnabled()) {
    return false;
  }

  DCHECK(plugin_element.HasHTMLTagName("object") ||
         plugin_element.HasHTMLTagName("embed"));
  // Blink will next try to load a WebPlugin which would end up in
  // OverrideCreatePlugin, sending another IPC only to find out the plugin is
  // not supported. Here it suffices to return false but there should perhaps be
  // a more unified approach to avoid sending the IPC twice.
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  ChromeContentRendererClient::GetPluginInfoHost()->GetPluginInfo(
      original_url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      mime_type, &plugin_info);
  // TODO(ekaramad): Not continuing here due to a disallowed status should take
  // us to CreatePlugin. See if more in depths investigation of |status| is
  // necessary here (see https://crbug.com/965747). For now, returning false
  // should take us to CreatePlugin after HTMLPlugInElement which is called
  // through HTMLPlugInElement::LoadPlugin code path.
  if (plugin_info->status != chrome::mojom::PluginStatus::kAllowed &&
      plugin_info->status !=
          chrome::mojom::PluginStatus::kPlayImportantContent) {
    // We could get here when a MimeHandlerView is loaded inside a <webview>
    // which is using permissions API (see WebViewPluginTests).
    ChromeExtensionsRendererClient::DidBlockMimeHandlerViewForDisallowedPlugin(
        plugin_element);
    return false;
  }
  if (plugin_info->actual_mime_type == pdf::kInternalPluginMimeType) {
    // Only actually treat the internal PDF plugin as externally handled if
    // used within an origin allowed to create the internal PDF plugin;
    // otherwise, let Blink try to create the in-process PDF plugin.
    if (IsPdfInternalPluginAllowedOrigin(
            render_frame->GetWebFrame()->GetSecurityOrigin())) {
      return true;
    }
  }
  return ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
      plugin_element, original_url, plugin_info->actual_mime_type,
      plugin_info->plugin);
}

bool AlloyContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  std::string orig_mime_type = params.mime_type.Utf8();
  if (extensions::ExtensionsEnabled() &&
      !extensions_renderer_client_->OverrideCreatePlugin(render_frame,
                                                         params)) {
    return false;
  }

  GURL url(params.url);
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  ChromeContentRendererClient::GetPluginInfoHost()->GetPluginInfo(
      url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      orig_mime_type, &plugin_info);
  *plugin = ChromeContentRendererClient::CreatePlugin(render_frame, params,
                                                      *plugin_info);
  return true;
}

void AlloyContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->WillSendRequest(frame, transition_type, url,
                                                 site_for_cookies,
                                                 initiator_origin, new_url);
    if (!new_url->is_empty()) {
      return;
    }
  }
}

uint64_t AlloyContentRendererClient::VisitedLinkHash(const char* canonical_url,
                                                     size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool AlloyContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

bool AlloyContentRendererClient::IsOriginIsolatedPepperPlugin(
    const base::FilePath& plugin_path) {
  // Isolate all the plugins (including the PDF plugin).
  return true;
}

void AlloyContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  GetChromeKeySystems(std::move(cb));
}

void AlloyContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentStart(render_frame);
  }

#ifdef OHOS_ARKWEB_ADBLOCK
  auto routing_id = render_frame->GetRoutingID();
  render_frame->GetTaskRunner(blink::TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &AlloyContentRendererClient::TriggerElementHidingInFrame,
                     base::Unretained(this), routing_id));

  render_frame->GetTaskRunner(blink::TaskType::kDOMManipulation)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &AlloyContentRendererClient::TriggerUserElementHidingInFrame,
              base::Unretained(this), routing_id));
#endif  // OHOS_ARKWEB_ADBLOCK

#if BUILDFLAG(IS_OHOS)
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  if (communication) {
    communication->RunScriptsAtDocumentStart();
  }
#endif //IS_OHOS
}

void AlloyContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentEnd(render_frame);
  }
#if BUILDFLAG(IS_OHOS)
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  if (communication) {
    communication->RunScriptsAtDocumentEnd();
  }
#endif //IS_OHOS
}

#if defined(OHOS_JSPROXY)
void AlloyContentRendererClient::RunScriptsAtHeadReady(
    content::RenderFrame* render_frame) {
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);

  if (communication) {
    communication->RunScriptsAtHeadReady();
  }
}
#endif

void AlloyContentRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentIdle(render_frame);
  }
}

void AlloyContentRendererClient::DevToolsAgentAttached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AlloyContentRendererClient::DevToolsAgentAttached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentAttached();
}

void AlloyContentRendererClient::DevToolsAgentDetached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AlloyContentRendererClient::DevToolsAgentDetached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentDetached();
}

void AlloyContentRendererClient::SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  blink::WebRuntimeFeatures::EnablePerformanceManagerInstrumentation(true);

#if defined(OHOS_ARKWEB_EXTENSIONS)
  // WebHID and WebUSB on service workers is only available in extensions.
  if (CefRenderManager::IsExtensionProcess()) {
    if (base::FeatureList::IsEnabled(
            features::kEnableWebUsbOnExtensionServiceWorker)) {
      blink::WebRuntimeFeatures::EnableWebUSBOnServiceWorkers(true);
    }
#if !BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            features::kEnableWebHidOnExtensionServiceWorker)) {
      blink::WebRuntimeFeatures::EnableWebHIDOnServiceWorkers(true);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }
#endif  // defined(OHOS_ARKWEB_EXTENSIONS)
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
bool AlloyContentRendererClient::AllowScriptExtensionForServiceWorker(
    const url::Origin& script_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return (script_origin.scheme() == extensions::kExtensionScheme ||
          script_origin.scheme() == extensions::kArkwebExtensionScheme);
#else
  return false;
#endif
}

void AlloyContentRendererClient::
    DidInitializeServiceWorkerContextOnWorkerThread(
        blink::WebServiceWorkerContextProxy* context_proxy,
        const GURL& service_worker_scope,
        const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context_proxy, service_worker_scope, script_url);
#endif
}
void AlloyContentRendererClient::WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()
      ->WillEvaluateServiceWorkerOnWorkerThread(
          context_proxy, v8_context, service_worker_version_id,
          service_worker_scope, script_url);
#endif
}

void AlloyContentRendererClient::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()
      ->DidStartServiceWorkerContextOnWorkerThread(
          service_worker_version_id, service_worker_scope, script_url);
#endif
}

void AlloyContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()
      ->WillDestroyServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id, service_worker_scope, script_url);
#endif
}

// If we're in an extension, there is no need disabling multiple routes as
// chrome.system.network.getNetworkInterfaces provides the same
// information. Also, the enforcement of sending and binding UDP is already done
// by chrome extension permission model.
bool AlloyContentRendererClient::ShouldEnforceWebRTCRoutingPreferences() {
  return !CefRenderManager::IsExtensionProcess();
}

blink::WebFrame* AlloyContentRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::CefExtensionsRendererClient::FindFrame(relative_to_frame,
                                                            name);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool AlloyContentRendererClient::IsSafeRedirectTarget(const GURL& from_url,
                                                      const GURL& to_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (to_url.SchemeIs(extensions::kExtensionScheme)
#if defined(OHOS_ARKWEB_EXTENSIONS)
      || to_url.SchemeIs(extensions::kArkwebExtensionScheme)
#endif
  ) {
    const extensions::Extension* extension =
        extensions::RendererExtensionRegistry::Get()->GetByID(to_url.host());
    if (!extension) {
      return false;
    }
    // TODO(solomonkinard): Use initiator_origin and add tests.
    if (extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
            extension, to_url.path(), absl::optional<url::Origin>())) {
      return true;
    }
    return extension->guid() == from_url.host();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return true;
}

void AlloyContentRendererClient::DidSetUserAgent(
    const std::string& user_agent) {
#if BUILDFLAG(ENABLE_PRINTING)
  printing::SetAgent(user_agent);
#endif
}
#endif  // defined(OHOS_ARKWEB_EXTENSIONS)

std::unique_ptr<blink::URLLoaderThrottleProvider>
AlloyContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<CefURLLoaderThrottleProviderImpl>(provider_type,
                                                            this);
}

void AlloyContentRendererClient::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  // Don't apply default CSP to PDF renderers.
  // TODO(crbug.com/1252096): Lock down the CSP once style and script are no
  // longer injected inline by `pdf::PluginResponseWriter`. That class may be a
  // better place to define such CSP, or we may continue doing so here.
  if (pdf::IsPdfRenderer()) {
    return;
  }

  DCHECK(csp);
  GURL gurl(url);
  const extensions::Extension* extension =
      extensions::RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          gurl);
  if (!extension) {
    return;
  }

  // Append a minimum CSP to ensure the extension can't relax the default
  // applied CSP through means like Service Worker.
  const std::string* default_csp =
      extensions::CSPInfo::GetMinimumCSPToAppend(*extension, gurl.path());
  if (!default_csp) {
    return;
  }

  csp->push_back({blink::WebString::FromUTF8(*default_csp),
                  network::mojom::ContentSecurityPolicyType::kEnforce,
                  network::mojom::ContentSecurityPolicySource::kHTTP});
}

void AlloyContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // TODO(crbug.com/977637): Get rid of the use of this implementation of
  // |service_manager::LocalInterfaceProvider|. This was done only to avoid
  // churning spellcheck code while eliminating the "chrome" and
  // "chrome_renderer" services. Spellcheck is (and should remain) the only
  // consumer of this implementation.
  content::RenderThread::Get()->BindHostReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

void AlloyContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

void AlloyContentRendererClient::OnBrowserCreated(
    blink::WebView* web_view,
    absl::optional<bool> is_windowless) {
#if BUILDFLAG(IS_MAC)
  const bool windowless = is_windowless.has_value() && *is_windowless;

  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  web_view->SetUseExternalPopupMenusThisInstance(!windowless);
#endif
}

void AlloyContentRendererClient::RunSingleProcessCleanupOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Clean up the single existing RenderProcessHost.
  content::RenderProcessHost* host = nullptr;
  content::RenderProcessHost::iterator iterator(
      content::RenderProcessHost::AllHostsIterator());
  if (!iterator.IsAtEnd()) {
    host = iterator.GetCurrentValue();
    host->Cleanup();
    iterator.Advance();
    DCHECK(iterator.IsAtEnd());
  }
  DCHECK(host);

  // Clear the run_renderer_in_process() flag to avoid a DCHECK in the
  // RenderProcessHost destructor.
  content::RenderProcessHost::SetRunRendererInProcess(false);

  // Deletion of the RenderProcessHost object will stop the render thread and
  // result in a call to WillDestroyCurrentMessageLoop.
  // Cleanup() will cause deletion to be posted as a task on the UI thread but
  // this task will only execute when running in multi-threaded message loop
  // mode (because otherwise the UI message loop has already stopped). Therefore
  // we need to explicitly delete the object when not running in this mode.
  if (!CefContext::Get()->settings().multi_threaded_message_loop) {
    delete host;
  }
}

#if BUILDFLAG(IS_OHOS)
bool AlloyContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  // Only GETs can be overridden.
  if (!request.HttpMethod().Equals("GET"))
    return false;

  // Any navigation from loadUrl, and goBack/Forward are considered application-
  // initiated and hence will not yield a shouldOverrideUrlLoading() callback.
  // Webview classic does not consider reload application-initiated so we
  // continue the same behavior.
  bool application_initiated = type == blink::kWebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  bool is_outermost_main_frame = frame->IsOutermostMainFrame();
  const GURL& gurl = request.Url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_outermost_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;

  blink::WebView* web_view = render_frame->GetWebView();

  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless);
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
      gurl.possibly_invalid_spec(), request.HttpMethod().Utf8(), has_user_gesture, is_redirect, is_outermost_main_frame);

  return ignore_navigation;
}
#endif

#ifdef OHOS_ARKWEB_ADBLOCK
void AlloyContentRendererClient::TriggerElementHidingInFrame(int routing_id) {
  // |render_frame| might be dead by now.
  auto* render_frame = content::RenderFrame::FromRoutingID(routing_id);
  if (!render_frame) {
    LOG(ERROR) << "[AdBlock] TriggerElementHidingInFrame render_frame null";
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  if (!web_frame) {
    LOG(ERROR) << "[AdBlock] TriggerElementHidingInFrame web_frame null";
    return;
  }

  blink::WebDocumentSubresourceFilter* filter =
      web_frame->GetDocumentLoader()->GetWebSubresourceFilter();
  if (!filter) {
    LOG(DEBUG) << "[AdBlock] TriggerElementHidingInFrame filter null";
    return;
  }

  blink::WebDocument document = web_frame->GetDocument();
  if (!document.Url().ProtocolIs("https") &&
      !document.Url().ProtocolIs("http")) {
    LOG(ERROR) << "[AdBlock] TriggerElementHidingInFrame scheme error";
    return;
  }

  if (web_frame->GetHasDocumentTypeOption()) {
    LOG(WARNING) << "[AdBlock] Match $document for "
                 << "***";
    return;
  }

  if (web_frame->GetHasElemHideTypeOption()) {
    LOG(WARNING) << "[AdBlock] Match selemhide for "
                 << "***";
    return;
  }

  bool has_generichide = web_frame->GetHasGenericHideTypeOption();
  if (has_generichide) {
    LOG(WARNING) << "[AdBlock] Match sgenerichide for "
                 << "***";
  }

  base::TimeTicks start = base::TimeTicks::Now();
  std::unique_ptr<std::string> selectors;
  selectors =
      filter->GetElementHidingSelectors(document.Url(), !has_generichide);

  if (!selectors->empty()) {
    document.InsertStyleSheet(blink::WebString::FromUTF8(*selectors), nullptr,
                              blink::WebCssOrigin::kAuthor,
                              blink::BackForwardCacheAware::kAllow,
                              blink::WebDocument::StyleSheetType::kAdBlock);
    base::TimeDelta duration = base::TimeTicks::Now() - start;
    LOG(WARNING) << "[AdBlock] Element hiding for "
                 << "***" << "assumming "
                 << duration.InMicroseconds() << "microseconds";
    return;
  }

  selectors.reset();
}

void AlloyContentRendererClient::TriggerUserElementHidingInFrame(
    int routing_id) {
  // |render_frame| might be dead by now.
  auto* render_frame = content::RenderFrame::FromRoutingID(routing_id);
  if (!render_frame) {
    LOG(ERROR) << "[AdBlock] TriggerUserElementHidingInFrame render_frame null";
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  if (!web_frame) {
    return;
  }

  blink::WebDocument document = web_frame->GetDocument();
  if (!document.Url().ProtocolIs("https") &&
      !document.Url().ProtocolIs("http")) {
    return;
  }
  blink::WebDocumentSubresourceFilter* filter =
      web_frame->GetDocumentLoader()->GetWebUserSubresourceFilter();
  if (!filter) {
    return;
  }
  base::TimeTicks start = base::TimeTicks::Now();
  std::unique_ptr<std::string> selectors;
  selectors = filter->GetElementHidingSelectors(document.Url(), true);
  if (!selectors->empty()) {
    document.InsertStyleSheet(blink::WebString::FromUTF8(*selectors), nullptr,
                              blink::WebCssOrigin::kAuthor,
                              blink::BackForwardCacheAware::kAllow,
                              blink::WebDocument::StyleSheetType::kUserAdBlock);
    base::TimeDelta duration = base::TimeTicks::Now() - start;
    LOG(WARNING) << "[User AdBlock] Element hiding for "
                 << "***" << " assumming "
                 << duration.InMicroseconds() << " microseconds";
    return;
  }
  selectors.reset();
}
#endif

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
