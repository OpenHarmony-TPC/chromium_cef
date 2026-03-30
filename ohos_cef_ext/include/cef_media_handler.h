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

#ifndef CEF_INCLUDE_CEF_MEDIA_HANDLER_H_
#define CEF_INCLUDE_CEF_MEDIA_HANDLER_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_browser.h"

///
/// Implement this interface to handle events related to browser media status.
/// The methods of this class will be called on the browser process UI thread or
/// render process main thread (TID_RENDERER).
///
/*--cef(source=client)--*/
class CefMediaHandler : public virtual CefBaseRefCounted {
 public:
  typedef cef_media_playing_state_t MediaPlayingState;
  typedef cef_media_type_t MediaType;
  ///
  /// Called when the audio state changed.
  ///
  /*--cef()--*/
  virtual void OnAudioStateChanged(CefRefPtr<CefBrowser> browser,
                                   bool audible) {}
  ///
  /// Called when the media playing state changed.
  ///
  /*--cef()--*/
  virtual void OnMediaStateChanged(CefRefPtr<CefBrowser> browser,
                                   MediaType type,
                                   MediaPlayingState state) {}

#if BUILDFLAG(ARKWEB_WEBRTC)
  ///
  /// Called when the camera capture state changed.
  ///
  /*--cef()--*/
  virtual void OnCameraCaptureStateChanged(int original_state, int new_state) {}
#endif

#if BUILDFLAG(ARKWEB_WEBRTC)
  ///
  /// Called when the microphone capture state changed.
  ///
  /*--cef()--*/
  virtual void OnMicrophoneCaptureStateChanged(int original_state, int new_state) {}
#endif
};

#endif  // CEF_INCLUDE_CEF_MEDIA_HANDLER_H_
