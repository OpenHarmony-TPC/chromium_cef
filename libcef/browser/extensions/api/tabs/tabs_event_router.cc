// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_event_router.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

using base::Value;
using content::WebContents;

namespace extensions {
namespace cef{
namespace {

bool HasValidMainFrameProcess(content::WebContents* contents) {
  content::RenderFrameHost* main_frame_host = contents->GetPrimaryMainFrame();
  content::RenderProcessHost* process_host = main_frame_host->GetProcess();
  return process_host->IsReady() && process_host->IsInitializedAndNotDead();
}

api::tabs::TabStatus GetLoadingStatus(WebContents* contents) {
  if (contents->IsLoading())
    return api::tabs::TAB_STATUS_LOADING;

  // Anything that isn't backed by a process is considered unloaded.
  if (!HasValidMainFrameProcess(contents))
    return api::tabs::TAB_STATUS_UNLOADED;

  // Otherwise its considered loaded.
  return api::tabs::TAB_STATUS_COMPLETE;
}

api::tabs::MutedInfo CreateMutedInfo(
    content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::MutedInfo info;
  info.muted = contents->IsAudioMuted();
  return info;
}

api::tabs::MutedInfo CreateMutedInfo(
    std::optional<NWebExtensionTabMutedInfo> mutedInfo,
    content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::MutedInfo info;
  if (mutedInfo) {
    info.muted = mutedInfo->muted;
    if (mutedInfo->extensionId) {
      info.extension_id = mutedInfo->extensionId.value();
    }
    if (mutedInfo->reason) {
      if (*mutedInfo->reason == "user") {
        info.reason = api::tabs::MUTED_INFO_REASON_USER;
      } else if (*mutedInfo->reason == "capture") {
        info.reason = api::tabs::MUTED_INFO_REASON_CAPTURE;
      } else if (*mutedInfo->reason == "extension") {
        info.reason = api::tabs::MUTED_INFO_REASON_EXTENSION;
      }
    }
  } else {
    info.muted = contents->IsAudioMuted();
  }
  return info;
}

api::tabs::Tab CreateTabObject(
    int tab_id,
    WebContents* contents,
    const std::string& url) {
  api::tabs::Tab tab_object;
  tab_object.id = tab_id;
  tab_object.index = -1;
  tab_object.window_id = -1;
  tab_object.status = GetLoadingStatus(contents);
  tab_object.active = false;
  tab_object.selected = false;
  tab_object.highlighted = false;
  tab_object.pinned = false;
  tab_object.group_id = -1;
  tab_object.audible = false;
  tab_object.discarded = false;
  tab_object.auto_discardable = false;
  tab_object.muted_info = CreateMutedInfo(contents);
  tab_object.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object.width = contents_size.width();
  tab_object.height = contents_size.height();
  tab_object.url = url.length() > 0 ? url :
                                      contents->GetLastCommittedURL().spec();

  content::NavigationEntry* pending_entry =
      contents->GetController().GetPendingEntry();
  if (pending_entry) {
    tab_object.pending_url = pending_entry->GetVirtualURL().spec();
  }
  tab_object.title = base::UTF16ToUTF8(contents->GetTitle());
  content::NavigationEntry* visible_entry =
      contents->GetController().GetVisibleEntry();
  if (visible_entry && visible_entry->GetFavicon().valid) {
    tab_object.fav_icon_url = visible_entry->GetFavicon().url.spec();
  }
  return tab_object;
}

api::tabs::Tab CreateTabObject(
    int tab_id,
    WebContents* contents,
    const NWebExtensionTabChangeInfo& info) {
  api::tabs::Tab tab_object;
  tab_object.id = tab_id;
  tab_object.index = -1;
  tab_object.window_id = -1;
  tab_object.status = info.status ? api::tabs::TabStatus(*info.status): GetLoadingStatus(contents);
  tab_object.active = false;
  tab_object.selected = false;
  tab_object.highlighted = false;
  tab_object.pinned = info.pinned ? *info.pinned : false;
  tab_object.group_id = info.groupId ? *info.groupId : -1;
  tab_object.audible = info.audible ? *info.audible : false;
  tab_object.discarded = info.discarded ? *info.discarded : false;
  tab_object.auto_discardable = info.autoDiscardable ? *info.autoDiscardable : false;
  tab_object.muted_info = CreateMutedInfo(info.mutedInfo, contents);
  tab_object.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object.width = contents_size.width();
  tab_object.height = contents_size.height();
  if (info.url) {
    tab_object.url = info.url.value().length() > 0 ? info.url.value() :
                                      contents->GetLastCommittedURL().spec();
  }

  content::NavigationEntry* pending_entry =
      contents->GetController().GetPendingEntry();
  if (pending_entry) {
    tab_object.pending_url = pending_entry->GetVirtualURL().spec();
  }
  tab_object.title = base::UTF16ToUTF8(contents->GetTitle());
  content::NavigationEntry* visible_entry =
      contents->GetController().GetVisibleEntry();
  if (visible_entry && visible_entry->GetFavicon().valid) {
    tab_object.fav_icon_url = visible_entry->GetFavicon().url.spec();
  }
  return tab_object;
}

bool WillDispatchTabUpdatedEvent(
    int tab_id,
    WebContents* contents,
    std::string url,
    const std::vector<std::string> changed_property_names,
    content::BrowserContext* browser_context,
    Feature::Context target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    absl::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  api::tabs::Tab tab_object = CreateTabObject(tab_id, contents, url);

  base::Value::Dict tab_value = tab_object.ToValue();

  base::Value::Dict changed_properties;
  for (const auto& property : changed_property_names) {
    if (const base::Value* value = tab_value.Find(property)) {
      changed_properties.Set(property, value->Clone());
    }
  }

  event_args_out.emplace();
  event_args_out->Append(tab_id);
  event_args_out->Append(std::move(changed_properties));
  event_args_out->Append(std::move(tab_value));
  return true;
}

bool WillDispatchTabUpdatedEventChangeInfo(
    int tab_id,
    WebContents* contents,
    NWebExtensionTabChangeInfo info,
    const std::vector<std::string> changed_property_names,
    content::BrowserContext* browser_context,
    Feature::Context target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    absl::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  api::tabs::Tab tab_object = CreateTabObject(tab_id, contents, info);

  base::Value::Dict tab_value = tab_object.ToValue();

  base::Value::Dict changed_properties;
  for (const auto& property : changed_property_names) {
    if (const base::Value* value = tab_value.Find(property)) {
      changed_properties.Set(property, value->Clone());
    }
  }

  event_args_out.emplace();
  event_args_out->Append(tab_id);
  event_args_out->Append(std::move(changed_properties));
  event_args_out->Append(std::move(tab_value));
  return true;
}

}  // namespace

TabsEventRouter::TabsEventRouter(Profile* profile) : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
}

TabsEventRouter::~TabsEventRouter() {}

void TabsEventRouter::DispatchTabUpdatedEvent(
    int tab_id,
    WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    const std::string& url) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  std::vector<std::string> changes = changed_property_names;
  if (url.length() > 0) {
    changes.push_back(tabs_constants::kStatusKey);
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  auto event = std::make_unique<Event>(
      events::TABS_ON_UPDATED, api::tabs::OnUpdated::kEventName,
      // The event arguments depend on the extension's permission. They are set
      // in WillDispatchTabUpdatedEvent().
      base::Value::List(), profile);
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabUpdatedEvent, tab_id,
                          contents, url, std::move(changes));
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

void TabsEventRouter::DispatchTabUpdatedEvent(
    int tab_id,
    WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> info) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  NWebExtensionTabChangeInfo in_info(*info);
  std::vector<std::string> changes = changed_property_names;
  if (info->url && info->url.value().length() > 0) {
    changes.push_back(tabs_constants::kStatusKey);
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  auto event = std::make_unique<Event>(
      events::TABS_ON_UPDATED, api::tabs::OnUpdated::kEventName,
      // The event arguments depend on the extension's permission. They are set
      // in WillDispatchTabUpdatedEvent().
      base::Value::List(), profile);
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabUpdatedEventChangeInfo, tab_id,
                          contents, std::move(in_info), std::move(changes));
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

void TabsEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args,
    EventRouter::UserGestureState user_gesture) {
  EventRouter* event_router = EventRouter::Get(profile);
 
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), profile);
  event->user_gesture = user_gesture;
  event_router->BroadcastEvent(std::move(event));
}
 
void TabsEventRouter::DispatchTabActiveEvent(int tab_id,
                                             int window_id,
                                             content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::OnActivated::ActiveInfo info;
  info.tab_id = tab_id;
  info.window_id = window_id;
 
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ACTIVATED,
                api::tabs::OnActivated::kEventName,
                api::tabs::OnActivated::Create(info),
                EventRouter::USER_GESTURE_UNKNOWN);
}

}  // namespace cef
}  // namespace extensions
