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

#ifndef CEF_INCLUDE_CEF_FIRST_MEANINGFUL_PAINT_DETAILS_H
#define CEF_INCLUDE_CEF_FIRST_MEANINGFUL_PAINT_DETAILS_H
#pragma once

#include "include/cef_base.h"

///
/// Class used to represent the details of first meaningful paint.
///
/*--cef(source=library)--*/
class CefFirstMeaningfulPaintDetails : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns start time of navigation
  ///
  /*--cef(default_retval=0)--*/
  virtual int64_t GetNavigationStartTime() = 0;

  ///
  /// Returns paint time of first meaningful content.
  ///
  /*--cef(default_retval=0)--*/
  virtual int64_t GetFirstMeaningfulPaintTime() = 0;
};

#endif  // CEF_INCLUDE_CEF_FIRST_MEANINGFUL_PAINT_DETAILS_H
