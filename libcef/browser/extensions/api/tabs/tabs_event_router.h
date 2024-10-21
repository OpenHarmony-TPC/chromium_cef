// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <map>
#include <vector>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "libcef/browser/extensions/api/tabs/tabs_api.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"

namespace content {
class WebContents;
}

namespace extensions {
namespace cef {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
class TabsEventRouter {
 public:
  explicit TabsEventRouter(Profile* profile);

  TabsEventRouter(const TabsEventRouter&) = delete;
  TabsEventRouter& operator=(const TabsEventRouter&) = delete;

  ~TabsEventRouter();

  // Packages |changed_property_names| as a tab updated event for the tab
  // |contents| and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(
      int tab_id,
      content::WebContents* contents,
      const std::vector<std::string>& changed_property_names,
      const std::string& url);

 private:
  // The main profile that owns this event router.
  raw_ptr<Profile> profile_;
};

}  // namespace cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
