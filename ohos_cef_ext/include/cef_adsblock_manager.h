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

#ifndef CEF_INCLUDE_CEF_ADSBLOCK_MANAGER_H_
#define CEF_INCLUDE_CEF_ADSBLOCK_MANAGER_H_
#pragma once

#include <vector>

#include "build/build_config.h"
#include "include/cef_base.h"
#include "include/cef_callback.h"

///
/// Class used for managing adsblock's rules.
///
/*--cef(source=library,no_debugct_check)--*/
class CefAdsBlockManager : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the global adsblock manager.
  ///
  /*--cef(optional_param=callback)--*/
  static CefRefPtr<CefAdsBlockManager> GetGlobalAdsBlockManager(
      CefRefPtr<CefCompletionCallback> callback);

  ///
  /// Set Ads Block ruleset file, containing easylist rules.
  ///
  /*--cef()--*/
  virtual void SetAdsBlockRules(const CefString& rulesFile, bool replace) = 0;

  ///
  /// Add items to Ads Block Disallowed list.
  ///
  /*--cef()--*/
  virtual void AddAdsBlockDisallowedList(
      const std::vector<CefString>& domainSuffixes) = 0;

  ///
  /// Remove items from Ads Block disallowed list.
  ///
  /*--cef()--*/
  virtual void RemoveAdsBlockDisallowedList(
      const std::vector<CefString>& domainSuffixes) = 0;

  ///
  /// Clear Ads Block disallowed list.
  ///
  /*--cef()--*/
  virtual void ClearAdsBlockDisallowedList() = 0;

  ///
  /// Add items to Ads Block Allowed list.
  ///
  /*--cef()--*/
  virtual void AddAdsBlockAllowedList(
      const std::vector<CefString>& domainSuffixes) = 0;

  ///
  /// Remove items from Ads Block allowed list.
  ///
  /*--cef()--*/
  virtual void RemoveAdsBlockAllowedList(
      const std::vector<CefString>& domainSuffixes) = 0;

  ///
  /// Clear Ads Block allow list.
  ///
  /*--cef()--*/
  virtual void ClearAdsBlockAllowedList() = 0;
};

#endif  // CEF_INCLUDE_CEF_ADSBLOCK_MANAGER_H_
