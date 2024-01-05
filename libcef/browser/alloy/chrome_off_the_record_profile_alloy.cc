// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/chrome_off_the_record_profile_alloy.h"

#include "base/no_destructor.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "net/url_request/url_request_context.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/zoom/zoom_event_manager.h"

namespace {

class CefVariationsClient : public variations::VariationsClient {
 public:
  explicit CefVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~CefVariationsClient() override = default;

  bool IsOffTheRecord() const override {
    return browser_context_->IsOffTheRecord();
  }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(false /* is_signed_in */);
  }

 private:
  content::BrowserContext* browser_context_;
};

}  // namespace

ChromeOffTheRecordProfileAlloy::ChromeOffTheRecordProfileAlloy(
    Profile* original_profile) : original_profile_(original_profile) {
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kIncognito);
}

ChromeOffTheRecordProfileAlloy::~ChromeOffTheRecordProfileAlloy() {}

bool ChromeOffTheRecordProfileAlloy::IsOffTheRecord() {
  return true;
}

bool ChromeOffTheRecordProfileAlloy::IsOffTheRecord() const {
  // Alloy contexts are never flagged as off-the-record. It causes problems
  // for the extension system.
  return true;
}

const Profile::OTRProfileID& ChromeOffTheRecordProfileAlloy::GetOTRProfileID() const {
  DCHECK(false);
  static base::NoDestructor<Profile::OTRProfileID> otr_profile_id(
      Profile::OTRProfileID::PrimaryID());
  return *otr_profile_id;
}

variations::VariationsClient* ChromeOffTheRecordProfileAlloy::GetVariationsClient() {
  if (!variations_client_) {
    variations_client_ = std::make_unique<CefVariationsClient>(this);
  }
  return variations_client_.get();
}

scoped_refptr<base::SequencedTaskRunner> ChromeOffTheRecordProfileAlloy::GetIOTaskRunner() {
  DCHECK(false);
  return scoped_refptr<base::SequencedTaskRunner>();
}

std::string ChromeOffTheRecordProfileAlloy::GetProfileUserName() const {
  DCHECK(false);
  return std::string();
}

Profile* ChromeOffTheRecordProfileAlloy::GetOffTheRecordProfile(
    const Profile::OTRProfileID& otr_profile_id,
    bool create_if_needed) {
  DCHECK(false);
  return nullptr;
}

std::vector<Profile*> ChromeOffTheRecordProfileAlloy::GetAllOffTheRecordProfiles() {
  return {};
}

void ChromeOffTheRecordProfileAlloy::DestroyOffTheRecordProfile(Profile* otr_profile) {
  DCHECK(false);
}

bool ChromeOffTheRecordProfileAlloy::HasOffTheRecordProfile(
    const Profile::OTRProfileID& otr_profile_id) {
  return false;
}

bool ChromeOffTheRecordProfileAlloy::HasAnyOffTheRecordProfile() {
  return true;
}

Profile* ChromeOffTheRecordProfileAlloy::GetOriginalProfile() {
  return original_profile_;
}

const Profile* ChromeOffTheRecordProfileAlloy::GetOriginalProfile() const {
  return original_profile_;
}

bool ChromeOffTheRecordProfileAlloy::IsChild() const {
  return false;
}

ExtensionSpecialStoragePolicy*
ChromeOffTheRecordProfileAlloy::GetExtensionSpecialStoragePolicy() {
  DCHECK(false);
  return nullptr;
}

bool ChromeOffTheRecordProfileAlloy::IsSameOrParent(Profile* profile) {
  DCHECK(false);
  return false;
}

base::Time ChromeOffTheRecordProfileAlloy::GetStartTime() const {
  DCHECK(false);
  return base::Time();
}

base::FilePath ChromeOffTheRecordProfileAlloy::last_selected_directory() {
  return last_selected_directory_;
}

void ChromeOffTheRecordProfileAlloy::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

GURL ChromeOffTheRecordProfileAlloy::GetHomePage() {
  DCHECK(false);
  return GURL();
}

bool ChromeOffTheRecordProfileAlloy::WasCreatedByVersionOrLater(
    const std::string& version) {
  DCHECK(false);
  return false;
}

base::Time ChromeOffTheRecordProfileAlloy::GetCreationTime() const {
  DCHECK(false);
  return base::Time();
}

void ChromeOffTheRecordProfileAlloy::SetCreationTimeForTesting(base::Time creation_time) {
  DCHECK(false);
}

void ChromeOffTheRecordProfileAlloy::RecordPrimaryMainFrameNavigation() {
  DCHECK(false);
}

bool ChromeOffTheRecordProfileAlloy::IsSignedIn() {
  DCHECK(false);
  return false;
}

void ChromeOffTheRecordProfileAlloy::TrackZoomLevelsFromParent() {
    content::HostZoomMap* host_zoom_map =
        content::HostZoomMap::GetDefaultForBrowserContext(this);
    content::HostZoomMap* parent_host_zoom_map =
        content::HostZoomMap::GetDefaultForBrowserContext(original_profile_);
    host_zoom_map->CopyFrom(parent_host_zoom_map);
    track_zoom_subscription_ =
        parent_host_zoom_map->AddZoomLevelChangedCallback(base::BindRepeating(
            &ChromeOffTheRecordProfileAlloy::OnParentZoomLevelChanged,
            base::Unretained(this)));
    if (!original_profile_->GetZoomLevelPrefs()) {
      return;
    }

    // Also track changes to the parent profile's default zoom level.
    parent_default_zoom_level_subscription_ =
        original_profile_->GetZoomLevelPrefs()
            ->RegisterDefaultZoomLevelCallback(base::BindRepeating(
                &ChromeOffTheRecordProfileAlloy::UpdateDefaultZoomLevel,
                base::Unretained(this)));
}

void ChromeOffTheRecordProfileAlloy::OnParentZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(this);
  switch (change.mode) {
    case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
      return;
    case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST:
      host_zoom_map->SetZoomLevelForHost(change.host, change.zoom_level);
      return;
    case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
      host_zoom_map->SetZoomLevelForHostAndScheme(change.scheme,
          change.host, change.zoom_level);
      return;
  }
}

void ChromeOffTheRecordProfileAlloy::UpdateDefaultZoomLevel() {
  if (!original_profile_->GetZoomLevelPrefs()) {
      return;
  }
  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(this);
  double default_zoom_level =
      original_profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  host_zoom_map->SetDefaultZoomLevel(default_zoom_level);
  zoom::ZoomEventManager::GetForBrowserContext(this)
      ->OnDefaultZoomLevelChanged();
}