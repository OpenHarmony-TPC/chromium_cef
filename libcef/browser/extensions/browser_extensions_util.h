// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_

#include <vector>

#include "include/internal/cef_ptr.h"
#include "contents_extensions_util.h"

#include "url/gurl.h"

namespace content {
class BrowserContext;
struct GlobalRenderFrameHostId;
class RenderFrameHost;
class RenderViewHost;
}  // namespace content

class CefBrowserHostBase;
class AlloyBrowserHostImpl;

namespace extensions {

class Extension;

// Returns the CefBrowserHostBase that owns the host identified by the specified
// global ID, if any. |is_guest_view| will be set to true if the ID
// matches a guest view associated with the returned browser instead of the
// browser itself.
CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id,
    bool* is_guest_view);

// Returns the CefBrowserHostBase that owns the specified |host|, if any.
// |is_guest_view| will be set to true if the host matches a guest view
// associated with the returned browser instead of the browser itself.
// TODO(cef): Delete the RVH variant once the remaining use case
// (via AlloyContentBrowserClient::OverrideWebkitPrefs) has been removed.
CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForHost(
    content::RenderViewHost* host,
    bool* is_guest_view);
CefRefPtr<CefBrowserHostBase> GetOwnerBrowserForHost(
    content::RenderFrameHost* host,
    bool* is_guest_view);

#if defined(OHOS_ARKWEB_EXTENSIONS)
/*
 * Sometimes the extension function is called from background script, and it's
 * windowless AlloyBrowser does not have a client or handler.
 * Try to get an AlloyBrowserHostImpl with a non-null client, that means it has
 * a nweb_handler can be used to interact with APP.
 */
CefRefPtr<AlloyBrowserHostImpl> GetClientAvailableBrowser(int tab_id);

// Get browser by tab_id for extension.
CefRefPtr<AlloyBrowserHostImpl> GetBrowserByTabIdForExtension(
    int tab_id,
    content::BrowserContext* browser_context);
#endif

// Returns the browser matching |tab_id| and |browser_context|. Returns false if
// |tab_id| is < 0 or a matching browser cannot be found within
// |browser_context|. Similar in concept to ExtensionTabUtil::GetTabById.
CefRefPtr<AlloyBrowserHostImpl> GetBrowserForTabId(
    int tab_id,
    content::BrowserContext* browser_context);

// Returns the extension associated with |url| in |profile|. Returns nullptr
// if the extension does not exist.
const Extension* GetExtensionForUrl(content::BrowserContext* browser_context,
                                    const GURL& url);

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
