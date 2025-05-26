/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "chrome/common/extensions/api/tabs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/favicon_status.h"

namespace extensions {

namespace {
constexpr char kStatusKey[] = "status";

bool HasValidMainFrameProcess(content::WebContents* contents) {
  content::RenderFrameHost* main_frame_host = contents->GetPrimaryMainFrame();
  content::RenderProcessHost* process_host = main_frame_host->GetProcess();
  return process_host->IsReady() && process_host->IsInitializedAndNotDead();
}

api::tabs::TabStatus GetLoadingStatus(content::WebContents* contents) {
  if (contents->IsLoading())
    return api::tabs::TabStatus::kLoading;

  // Anything that isn't backed by a process is considered unloaded.
  if (!HasValidMainFrameProcess(contents))
    return api::tabs::TabStatus::kUnloaded;

  // Otherwise its considered loaded.
  return api::tabs::TabStatus::kComplete;
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
        info.reason = api::tabs::MutedInfoReason::kUser;
      } else if (*mutedInfo->reason == "capture") {
        info.reason = api::tabs::MutedInfoReason::kCapture;
      } else if (*mutedInfo->reason == "extension") {
        info.reason = api::tabs::MutedInfoReason::kExtension;
      }
    }
  } else {
    info.muted = contents->IsAudioMuted();
  }
  return info;
}

api::tabs::Tab CreateTabObject(
    int tab_id,
    content::WebContents* contents,
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
    content::WebContents* contents,
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
    content::WebContents* contents,
    std::string url,
    const std::vector<std::string> changed_property_names,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
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
    content::WebContents* contents,
    NWebExtensionTabChangeInfo info,
    const std::vector<std::string> changed_property_names,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
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

void TabsEventRouter::DispatchTabUpdatedEvent(
    int tab_id,
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    const std::string& url) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  std::vector<std::string> changes = changed_property_names;
  if (url.length() > 0) {
    changes.push_back(kStatusKey);
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
    content::WebContents* contents,
    const std::vector<std::string>& changed_property_names,
    std::unique_ptr<NWebExtensionTabChangeInfo> info) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  NWebExtensionTabChangeInfo in_info(*info);
  std::vector<std::string> changes = changed_property_names;
  if (info->url && info->url.value().length() > 0) {
    changes.push_back(kStatusKey);
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

}  // namespace extensions
