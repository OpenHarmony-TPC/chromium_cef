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

#include "cef/ohos_cef_ext/libcef/browser/arkweb_browser_info_manager_utils.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_info_manager.h"

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "content/public/common/url_constants.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#endif
#if BUILDFLAG(ARKWEB_READER_MODE)
#include "content/browser/web_contents/web_contents_impl.h"
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_reader_mode_config.h"
#endif

ArkwebBrowserInfoManagerUtils::ArkwebBrowserInfoManagerUtils(
    CefBrowserInfoManager* cef_browser_info_manager)
    : cef_browser_info_manager_(cef_browser_info_manager) {
  DCHECK(cef_browser_info_manager);
}

ArkwebBrowserInfoManagerUtils::~ArkwebBrowserInfoManagerUtils() = default;

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
bool ArkwebBrowserInfoManagerUtils::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    bool user_gesture,
    const gfx::Rect& window_features,
    CefRefPtr<CefCallback> callback) {
  CEF_REQUIRE_UIT();
  content::Referrer referrer;
  content::OpenURLParams params(target_url, referrer, disposition,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  params.user_gesture = user_gesture;

  CefRefPtr<CefBrowserHostBase> browser;
  if (!cef_browser_info_manager_->MaybeAllowNavigation(opener, params,
                                                       browser) ||
      !browser) {
    LOG(INFO) << "not allow popup, cancel the popup";
    callback->Cancel();
    return false;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  bool allow = true;
  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get()) {
      CefRefPtr<CefFrame> opener_frame = browser->GetFrameForHost(opener);
      DCHECK(opener_frame);
      CefRect features(window_features.x(), window_features.y(),
                       window_features.width(), window_features.height());
      allow = !handler->OnPreBeforePopup(
          browser.get(), opener_frame, target_url.spec(),
          static_cast<cef_window_open_disposition_t>(disposition), user_gesture,
          features, callback);
    }
  }
  if (!allow) {
    callback->Cancel();
  }
  return allow;
}
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
bool ArkwebBrowserInfoManagerUtils::IsExtensionsOptionsUiFrame(
    const content::GlobalRenderFrameHostToken& global_token) {
  constexpr char kExtensionsHost[] = "extensions";

  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);

  bool is_extensions_options_ui_frame =
      web_contents &&
      (web_contents->GetURL().SchemeIs(extensions::kExtensionScheme) ||
       web_contents->GetURL().SchemeIs(extensions::kArkwebExtensionScheme)) &&
      web_contents->GetResponsibleWebContents() &&
      web_contents->GetResponsibleWebContents()->GetURL().SchemeIs(
          content::kArkWebUIScheme) &&
      web_contents->GetResponsibleWebContents()->GetURL().host() ==
          kExtensionsHost;
  return is_extensions_options_ui_frame;
}

bool ArkwebBrowserInfoManagerUtils::IsExtensionsOffscreenFrame(
    const content::GlobalRenderFrameHostToken& global_token) {
  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);

  if (web_contents &&
      (web_contents->GetURL().SchemeIs(extensions::kExtensionScheme) ||
       web_contents->GetURL().SchemeIs(extensions::kArkwebExtensionScheme)) &&
      (extensions::GetViewType(web_contents) ==
       extensions::mojom::ViewType::kOffscreenDocument)) {
    LOG(INFO) << "need to exclude GetNewBrowserInfo for offscreen document";
    return true;
  }
  return false;
}

bool ArkwebBrowserInfoManagerUtils::IsExtensionsBackgroundFrame(
    const content::GlobalRenderFrameHostToken& global_token) {
  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    LOG(INFO) << "IsExtensionsBackgroundFrame: web_contents is null";
    return false;
  }

  const GURL& url = web_contents->GetURL();

  if (!(url.SchemeIs(extensions::kExtensionScheme) ||
        url.SchemeIs(extensions::kArkwebExtensionScheme))) {
    return false;
  }

  auto view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::mojom::ViewType::kExtensionBackgroundPage) {
    return true;
  }

  return false;
}
#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

#if BUILDFLAG(ARKWEB_READER_MODE)
bool ArkwebBrowserInfoManagerUtils::IsDistillerPageWebContents(content::WebContents* web_contents) {
  if (!nweb_ex::AlloyBrowserReaderModeConfig::GetInstance()->IsReaderModeEnabled()) {
    return false;
  }
  if (!web_contents) {
    LOG(ERROR) << "web_contents is nullptr";
    return false;
  }
  content::WebContentsImpl* web_contents_impl = static_cast<content::WebContentsImpl*>(web_contents);
  if (!web_contents_impl || !web_contents_impl->AsWebContentsImplExt()) {
    LOG(ERROR) << "get web_contents_impl_ext failed";
    return false;
  }
  return web_contents_impl->AsWebContentsImplExt()->IsDistillerPageWebContents();
}
#endif // ARKWEB_READER_MODE

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
bool ArkwebBrowserInfoManagerUtils::IsPrerendering(content::WebContents* web_contents) {
  for (auto& context : CefBrowserContext::GetAll()) {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            context->AsBrowserContext());
    if (no_state_prefetch_manager) {
      auto* no_state_prefetch_content =
          no_state_prefetch_manager->GetNoStatePrefetchContents(web_contents);
      if (no_state_prefetch_content) {
        return true;
      }
    }
  }

  return false;
}
#endif // ARKWEB_NO_STATE_PREFETCH

#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH) || BUILDFLAG(ARKWEB_READER_MODE)
bool ArkwebBrowserInfoManagerUtils::ShouldCancel(
    const content::GlobalRenderFrameHostToken& global_token) {
  std::vector<CefBrowserContext*> browser_context_all =
      CefBrowserContext::GetAll();
  if (browser_context_all.size() == 0) {
    return false;
  }

  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return false;
  }

  bool should_cancel = false;
#if BUILDFLAG(ARKWEB_NO_STATE_PREFETCH)
  should_cancel = should_cancel || IsPrerendering(web_contents);
#endif

#if BUILDFLAG(ARKWEB_READER_MODE)
  should_cancel = should_cancel || IsDistillerPageWebContents(web_contents);
#endif

  return should_cancel;
}

void ArkwebBrowserInfoManagerUtils::CancelForSomeCases(
    const content::GlobalRenderFrameHostToken& global_token,
    int timeout_id) {
  CEF_REQUIRE_UIT();
  LOG(INFO) << "cancel for some cases";
  CefBrowserInfoManager* cef_browser_info_manager =
      CefBrowserInfoManager::GetInstance();
  if (!cef_browser_info_manager) {
    return;
  }

  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  if (!rfh) {
    LOG(INFO) <<"RFH is null, cancel to get new browser info.";
    cef_browser_info_manager->ContinueNewBrowserInfo(global_token, nullptr, true);
    return;
  }

  if (!ShouldCancel(global_token)) {
    return;
  }

  cef_browser_info_manager->TimeoutNewBrowserInfoResponse(global_token,
                                                          timeout_id);
}
#endif
