// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
#endif // OHOS_ARKWEB_EXTENSIONS

namespace extensions {
namespace cef {

class TabsEventRouter;

class TabsWindowsAPI : public BrowserContextKeyedAPI,
                       public EventRouter::Observer {
 public:
  explicit TabsWindowsAPI(content::BrowserContext* context);
  ~TabsWindowsAPI() override;

  // Convenience method to get the TabsWindowsAPI for a profile.
  static TabsWindowsAPI* Get(content::BrowserContext* context);

  TabsEventRouter* tabs_event_router();

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsWindowsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

  void TabUpdated(int tab_id,
      content::WebContents* contents,
      const std::vector<std::string>& changed_property_names,
      const std::string& url);

  void TabUpdated(int tab_id,
      content::WebContents* contents,
      const std::vector<std::string>& changed_property_names,
      std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo);

  void TabActived(int tab_id, int window_id, content::WebContents* contents);

 private:
  friend class BrowserContextKeyedAPIFactory<TabsWindowsAPI>;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "TabsWindowsAPI";
  }

  std::unique_ptr<TabsEventRouter> tabs_event_router_;
};

}  // namespace cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_
