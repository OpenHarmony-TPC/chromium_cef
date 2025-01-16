// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/extensions_renderer_client.h"

#include "libcef/renderer/alloy/alloy_content_renderer_client.h"
#include "libcef/renderer/alloy/alloy_render_thread_observer.h"
#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"

#include "base/stl_util.h"
#include "base/types/optional_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "extensions/common/manifest_handlers/background_info.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#endif

namespace extensions {

namespace {

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

}  // namespace

CefExtensionsRendererClient::CefExtensionsRendererClient(
    AlloyContentRendererClient* alloy_content_renderer_client)
    : alloy_content_renderer_client_(alloy_content_renderer_client) {}

CefExtensionsRendererClient::~CefExtensionsRendererClient() {}

bool CefExtensionsRendererClient::IsIncognitoProcess() const {
  return alloy_content_renderer_client_->GetAlloyObserver()
      ->IsIncognitoProcess();
}

int CefExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // CEF doesn't need to reserve world IDs for anything other than extensions,
  // so we always return 1. Note that 0 is reserved for the global world.
  return 1;
}

extensions::Dispatcher* CefExtensionsRendererClient::GetDispatcher() {
  return extension_dispatcher_.get();
}

void CefExtensionsRendererClient::OnExtensionLoaded(
    const extensions::Extension& extension) {
  resource_request_policy_->OnExtensionLoaded(extension);
}

void CefExtensionsRendererClient::OnExtensionUnloaded(
    const extensions::ExtensionId& extension_id) {
  resource_request_policy_->OnExtensionUnloaded(extension_id);
}

bool CefExtensionsRendererClient::ExtensionAPIEnabledForServiceWorkerScript(
    const GURL& scope,
    const GURL& script_url) const {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  if (!script_url.SchemeIs(extensions::kExtensionScheme) &&
      !script_url.SchemeIs(extensions::kArkwebExtensionScheme)) {
    return false;
  }

  const Extension* extension =
      extensions::RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          script_url);

  if (!extension ||
      !extensions::BackgroundInfo::IsServiceWorkerBased(extension)) {
    return false;
  }

  if (scope != extension->url()) {
    return false;
  }

  const std::string& sw_script =
      extensions::BackgroundInfo::GetBackgroundServiceWorkerScript(extension);

  return extension->GetResourceURL(sw_script) == script_url;
#else
  // TODO(extensions): Implement to support background sevice worker scripts
  // in extensions
  return false;
#endif
}

void CefExtensionsRendererClient::RenderThreadStarted() {
  content::RenderThread* thread = content::RenderThread::Get();

  extension_dispatcher_ = std::make_unique<extensions::Dispatcher>(
      std::make_unique<extensions::CefExtensionsDispatcherDelegate>());
  extension_dispatcher_->OnRenderThreadStarted(thread);
  resource_request_policy_ =
      std::make_unique<extensions::ResourceRequestPolicy>(
          extension_dispatcher_.get());

  thread->AddObserver(extension_dispatcher_.get());
}

void CefExtensionsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  new extensions::ExtensionsRenderFrameObserver(render_frame, registry);
  new extensions::ExtensionFrameHelper(render_frame,
                                       extension_dispatcher_.get());
  extension_dispatcher_->OnRenderFrameCreated(render_frame);
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
bool CefExtensionsRendererClient::AllowPopup() {
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension()) {
    return false;
  }

  // See http://crbug.com/117446 for the subtlety of this check.
  switch (current_context->context_type()) {
    case extensions::Feature::UNSPECIFIED_CONTEXT:
    case extensions::Feature::WEB_PAGE_CONTEXT:
    case extensions::Feature::UNBLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::WEBUI_CONTEXT:
    case extensions::Feature::WEBUI_UNTRUSTED_CONTEXT:
    case extensions::Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::USER_SCRIPT_CONTEXT:
    case extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return false;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
      return !current_context->IsForServiceWorker();
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
      return true;
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
      return current_context->web_frame()->IsOutermostMainFrame();
  }
}

blink::ProtocolHandlerSecurityLevel
CefExtensionsRendererClient::GetProtocolHandlerSecurityLevel() {
  // WARNING: This must match the logic of
  // Browser::GetProtocolHandlerSecurityLevel().
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension()) {
    return blink::ProtocolHandlerSecurityLevel::kStrict;
  }

  switch (current_context->context_type()) {
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
    case extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case extensions::Feature::UNBLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::UNSPECIFIED_CONTEXT:
    case extensions::Feature::USER_SCRIPT_CONTEXT:
    case extensions::Feature::WEBUI_CONTEXT:
    case extensions::Feature::WEBUI_UNTRUSTED_CONTEXT:
    case extensions::Feature::WEB_PAGE_CONTEXT:
      return blink::ProtocolHandlerSecurityLevel::kStrict;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
      return blink::ProtocolHandlerSecurityLevel::kExtensionFeatures;
  }
}
#endif  // defined(OHOS_ARKWEB_EXTENSIONS)

bool CefExtensionsRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  if (params.mime_type.Utf8() != content::kBrowserPluginMimeType) {
    return true;
  }

  bool guest_view_api_available = false;
  extension_dispatcher_->script_context_set_iterator()->ForEach(
      render_frame, base::BindRepeating(&IsGuestViewApiAvailableToScriptContext,
                                        &guest_view_api_available));
  return !guest_view_api_available;
}

void CefExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
  if ((url.ProtocolIs(extensions::kExtensionScheme)
#if defined(OHOS_ARKWEB_EXTENSIONS)
       || url.ProtocolIs(extensions::kArkwebExtensionScheme)
#endif
           ) &&
      !resource_request_policy_->CanRequestResource(
          GURL(url), frame, transition_type,
          base::OptionalFromPtr(initiator_origin))) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
  }
}

void CefExtensionsRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentIdle(render_frame);
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
// static
blink::WebFrame* CefExtensionsRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
  content::RenderFrame* result = extensions::ExtensionFrameHelper::FindFrame(
      content::RenderFrame::FromWebFrame(relative_to_frame), name);
  return result ? result->GetWebFrame() : nullptr;
}
#endif

}  // namespace extensions
