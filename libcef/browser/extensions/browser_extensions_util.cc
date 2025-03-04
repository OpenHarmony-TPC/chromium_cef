// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extensions/browser_extensions_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/frame_util.h"
#include "libcef/features/runtime_checks.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id,
    bool* is_guest_view) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* host =
        content::RenderFrameHost::FromID(global_id);
    if (host) {
      return GetOwnerBrowserForHost(host, is_guest_view);
    }
    return nullptr;
  } else {
    // Use the thread-safe approach.
    scoped_refptr<CefBrowserInfo> info =
        CefBrowserInfoManager::GetInstance()->GetBrowserInfo(global_id,
                                                             is_guest_view);
    if (info.get()) {
      CefRefPtr<CefBrowserHostBase> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id()
                     << " but no browser object matching frame "
                     << frame_util::GetFrameDebugString(global_id);
      }
      return browser;
    }
    return nullptr;
  }
}

CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForHost(
    content::RenderViewHost* host,
    bool* is_guest_view) {
  if (is_guest_view) {
    *is_guest_view = false;
  }

  CefRefPtr<CefBrowserHostBase> browser =
      CefBrowserHostBase::GetBrowserForHost(host);
  if (!browser.get() && ExtensionsEnabled()) {
    // Retrieve the owner browser, if any.
    content::WebContents* owner = GetOwnerForGuestContents(
        content::WebContents::FromRenderViewHost(host));
    if (owner) {
      browser = CefBrowserHostBase::GetBrowserForContents(owner);
      if (browser.get() && is_guest_view) {
        *is_guest_view = true;
      }
    }
  }
  return browser;
}

CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForHost(
    content::RenderFrameHost* host,
    bool* is_guest_view) {
  if (is_guest_view) {
    *is_guest_view = false;
  }

  CefRefPtr<CefBrowserHostBase> browser =
      CefBrowserHostBase::GetBrowserForHost(host);
  if (!browser.get() && ExtensionsEnabled()) {
    // Retrieve the owner browser, if any.
    content::WebContents* owner = GetOwnerForGuestContents(
        content::WebContents::FromRenderFrameHost(host));
    if (owner) {
      browser = CefBrowserHostBase::GetBrowserForContents(owner);
      if (browser.get() && is_guest_view) {
        *is_guest_view = true;
      }
    }
  }
  return browser;
}

CefRefPtr<AlloyBrowserHostImpl> GetBrowserForTabId(
    int tab_id,
    content::BrowserContext* browser_context) {
  REQUIRE_ALLOY_RUNTIME();
  CEF_REQUIRE_UIT();
  DCHECK(browser_context);
  if (tab_id < 0 || !browser_context) {
    return nullptr;
  }

  auto cef_browser_context =
      CefBrowserContext::FromBrowserContext(browser_context);

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser =
        static_cast<AlloyBrowserHostImpl*>(browser_info->browser().get());
    if (current_browser && current_browser->GetIdentifier() == tab_id) {
      // Make sure we're operating in the same CefBrowserContext.
      if (CefBrowserContext::FromBrowserContext(
              current_browser->GetBrowserContext()) == cef_browser_context) {
        return current_browser;
      } else {
        LOG(WARNING) << "Browser with tabId " << tab_id
                     << " cannot be accessed because is uses a different "
                        "CefRequestContext";
        break;
      }
    }
  }

  return nullptr;
}

#if defined(OHOS_ARKWEB_EXTENSIONS)
/*
 * Sometimes the extension function is called from background script, and it's
 * windowless AlloyBrowser does not have a client or handler.
 * Try to get an AlloyBrowserHostImpl with a non-null client, that means it has
 * a nweb_handler can be used to interact with APP.
 */
CefRefPtr<AlloyBrowserHostImpl> GetClientAvailableBrowser(int tab_id) {
  for (const auto& browser_info :
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser =
        static_cast<AlloyBrowserHostImpl*>(browser_info->browser().get());
    if (current_browser &&
        current_browser->GetTabId() == tab_id && tab_id >= 0) {
      return current_browser;
    }
  }

  for (const auto& browser_info :
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser =
        static_cast<AlloyBrowserHostImpl*>(browser_info->browser().get());
    if (current_browser &&
        current_browser->web_contents() && current_browser->client()) {
      return current_browser;
    }
  }
  return nullptr;
}

CefRefPtr<AlloyBrowserHostImpl> GetBrowserByTabIdForExtension(
    int tab_id,
    content::BrowserContext* browser_context) {
  REQUIRE_ALLOY_RUNTIME();
  CEF_REQUIRE_UIT();
  DCHECK(browser_context);
  if (tab_id < 0 || !browser_context) {
    return nullptr;
  }

  auto cef_browser_context =
      CefBrowserContext::FromBrowserContext(browser_context);

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<AlloyBrowserHostImpl> current_browser =
        static_cast<AlloyBrowserHostImpl*>(browser_info->browser().get());
    if (current_browser && current_browser->GetTabId() == tab_id) {
      // Make sure we're operating in the same CefBrowserContext.
      if (CefBrowserContext::FromBrowserContext(
              current_browser->GetBrowserContext()) == cef_browser_context) {
        return current_browser;
      } else {
        LOG(WARNING) << "[Extension] Browser with tabId " << tab_id
                     << " cannot be accessed because is uses a different "
                        "CefRequestContext";
        break;
      }
    }
  }

  return nullptr;
}
#endif

const Extension* GetExtensionForUrl(content::BrowserContext* browser_context,
                                    const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return nullptr;
  }
  std::string extension_id = url.host();
  return registry->enabled_extensions().GetByID(extension_id);
}

}  // namespace extensions
