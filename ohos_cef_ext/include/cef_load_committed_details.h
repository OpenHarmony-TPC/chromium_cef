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

#ifndef CEF_INCLUDE_CEF_LOAD_COMMITTED_DETAILS_H_
#define CEF_INCLUDE_CEF_LOAD_COMMITTED_DETAILS_H_
#pragma once

#include "include/cef_base.h"

///
/// Class used to represent the details of a committed navigation entry.
///
/*--cef(source=library)--*/
class CefLoadCommittedDetails : public virtual CefBaseRefCounted {
 public:
  typedef cef_navigation_entry_type_t NavigationType;

  ///
  /// Returns true when the main frame was navigated. False means the
  /// navigation was a sub-frame.
  ///
  /*--cef()--*/
  virtual bool IsMainFrame() = 0;

  ///
  /// Returns true if the navigation haapped without changing document.
  /// Example of same document navigations are:
  /// * reference fragment navigations
  /// * pushState/replaceState
  /// * same page history navigation
  ///
  /*--cef()--*/
  virtual bool IsSameDocument() = 0;

  ///
  /// Returns true if the committed entry has replaced the existing one.
  ///
  /*--cef()--*/
  virtual bool DidReplaceEntry() = 0;

  ///
  /// Returns the type of navigation that just occurred.
  ///
  /*--cef(default_retval=NAVIGATION_TYPE_UNKNOWN)--*/
  virtual NavigationType GetNavigationType() = 0;

  ///
  /// Returns the url of the current navigation.
  ///
  /*--cef()--*/
  virtual CefString GetCurrentURL() = 0;
};

#endif  // CEF_INCLUDE_CEF_LOAD_COMMITTED_DETAILS_H_
