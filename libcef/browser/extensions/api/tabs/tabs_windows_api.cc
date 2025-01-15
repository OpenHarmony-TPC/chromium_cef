// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_windows_api.h"

#include <memory>

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "libcef/browser/extensions/api/tabs/tabs_event_router.h"
#include "libcef/browser/extensions/api/tabs/tabs_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {
namespace cef {

TabsWindowsAPI::TabsWindowsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);

  // Register these events' observer for initialize tabs_event_router.
  // Tabs API Events.
  event_router->RegisterObserver(this, api::tabs::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnUpdated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnMoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnSelectionChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActiveChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActivated::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnHighlightChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnHighlighted::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnDetached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnAttached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnRemoved::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnReplaced::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnZoomChange::kEventName);

  // Windows API Events.
  event_router->RegisterObserver(this, api::windows::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::windows::OnRemoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::windows::OnFocusChanged::kEventName);
  event_router->RegisterObserver(this,
                                 api::windows::OnBoundsChanged::kEventName);
}

TabsWindowsAPI::~TabsWindowsAPI() {
}

// static
TabsWindowsAPI* TabsWindowsAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<TabsWindowsAPI>::Get(context);
}

TabsEventRouter* TabsWindowsAPI::tabs_event_router() {
  if (!tabs_event_router_.get()) {
    tabs_event_router_ = std::make_unique<TabsEventRouter>(
        Profile::FromBrowserContext(browser_context_));
  }
  return tabs_event_router_.get();
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> changeInfo) {
  tabs_event_router()->DispatchTabUpdatedEvent(
    tab_id, contents, changed_property_names, std::move(changeInfo));
}

void TabsWindowsAPI::TabUpdated(int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    const std::string& url) {
  tabs_event_router()->DispatchTabUpdatedEvent(
    tab_id, contents, changed_property_names, url);
}

void TabsWindowsAPI::TabActived(int tab_id,
                                int window_id,
                                content::WebContents* contents) {
  tabs_event_router()->DispatchTabActiveEvent(tab_id, window_id, contents);
}

void TabsWindowsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabsWindowsAPI>>::
    DestructorAtExit g_tabs_windows_api_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<TabsWindowsAPI>*
TabsWindowsAPI::GetFactoryInstance() {
  return g_tabs_windows_api_factory.Pointer();
}

void TabsWindowsAPI::OnListenerAdded(const EventListenerInfo& details) {
  // Initialize the event routers.
  tabs_event_router();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace cef
}  // namespace extensions
