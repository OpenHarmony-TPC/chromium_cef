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

#ifndef CEF_INCLUDE_CEF_NATIVE_EMBED_FIRST_FRAME_PAINT_EVENT_H_
#define CEF_INCLUDE_CEF_NATIVE_EMBED_FIRST_FRAME_PAINT_EVENT_H_
#pragma once

#include "include/cef_base.h"

///
/// NativeEmbedFirstPaintEvent information.
///
/*--cef(source=library)--*/
class CefNativeEmbedFirstFramePaintEvent : public virtual CefBaseRefCounted {
 public:
  ///
  /// Get emebedId.
  ///
  /*--cef()--*/
  virtual CefString GetEmbedId() = 0;

  ///
  /// Get surfaceId.
  ///
  /*--cef()--*/
  virtual CefString GetSurfaceId() = 0;

  ///
  /// Get emebedIdAttribute.
  ///
  /*--cef()--*/
  virtual CefString GetEmbedIdAttribute() = 0;
};

#endif  // CEF_INCLUDE_CEF_NATIVE_EMBED_FIRST_FRAME_PAINT_EVENT_H_
