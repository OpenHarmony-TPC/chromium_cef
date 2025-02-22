// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/extensions_client.h"

#include <utility>

#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_api_provider.h"

#include "base/logging.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/url_pattern_set.h"

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "chrome/common/extensions/chrome_extensions_api_provider.h"
#endif

namespace extensions {

namespace {

#if defined(OHOS_ARKWEB_EXTENSIONS)
// TODO(battre): Delete the HTTP URL once the blocklist is downloaded via HTTPS.
const char kExtensionBlocklistUrlPrefix[] =
    "http://www.gstatic.com/chrome/extensions/blocklist";
const char kExtensionBlocklistHttpsUrlPrefix[] =
    "https://www.gstatic.com/chrome/extensions/blocklist";
#endif

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

}  // namespace

CefExtensionsClient::CefExtensionsClient()
    : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
      new_webstore_base_url_(extension_urls::kNewChromeWebstoreBaseURL),
      webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<CefExtensionsAPIProvider>());
#if defined(OHOS_ARKWEB_EXTENSIONS)
  AddAPIProvider(std::make_unique<ChromeExtensionsAPIProvider>());
#endif
}

CefExtensionsClient::~CefExtensionsClient() {}

void CefExtensionsClient::Initialize() {}

void CefExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {}

const PermissionMessageProvider&
CefExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string CefExtensionsClient::GetProductName() {
  return "cef";
}

void CefExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  NOTIMPLEMENTED();
}

void CefExtensionsClient::SetScriptingAllowlist(
    const ScriptingAllowlist& allowlist) {
  scripting_allowlist_ = allowlist;
}

const ExtensionsClient::ScriptingAllowlist&
CefExtensionsClient::GetScriptingAllowlist() const {
  // TODO(jamescook): Real allowlist.
  return scripting_allowlist_;
}

URLPatternSet CefExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  return URLPatternSet();
}

bool CefExtensionsClient::IsScriptableURL(const GURL& url,
                                          std::string* error) const {
  return true;
}

const GURL& CefExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& CefExtensionsClient::GetNewWebstoreBaseURL() const {
  return new_webstore_base_url_;
}

const GURL& CefExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool CefExtensionsClient::IsBlocklistUpdateURL(const GURL& url) const {
#if defined(OHOS_ARKWEB_EXTENSIONS)
  // The extension blocklist URL is returned from the update service and
  // therefore not determined by Chromium. If the location of the blocklist file
  // ever changes, we need to update this function. A DCHECK in the
  // ExtensionUpdater ensures that we notice a change. This is the full URL
  // of a blocklist:
  // http://www.gstatic.com/chrome/extensions/blocklist/l_0_0_0_7.txt
  return base::StartsWith(url.spec(), kExtensionBlocklistUrlPrefix,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(url.spec(), kExtensionBlocklistHttpsUrlPrefix,
                          base::CompareCase::SENSITIVE);
#endif
}

}  // namespace extensions
