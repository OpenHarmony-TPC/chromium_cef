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

#include "cef/libcef/browser/views/color_provider_tracker.h"

#include "base/check.h"

CefColorProviderTracker::CefColorProviderTracker(Observer* observer)
    : observer_(observer) {
  DCHECK(observer_);
  color_provider_observation_.Observe(&ui::ColorProviderManager::Get());
}

void CefColorProviderTracker::OnNativeThemeUpdated() {
  got_theme_updated_ = true;
}

void CefColorProviderTracker::OnColorProviderCacheReset() {
  // May be followed by a call to OnNativeThemeUpdated.
  got_theme_updated_ = false;
}

void CefColorProviderTracker::OnAfterNativeThemeUpdated() {
  if (!got_theme_updated_) {
    observer_->OnColorProviderCacheResetMissed();
  }
}
