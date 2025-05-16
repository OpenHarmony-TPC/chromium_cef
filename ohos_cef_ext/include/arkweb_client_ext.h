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

#ifndef OHOS_CEF_EXT_INCLUDE_ARKWEB_CLIENT_EXT_H_
#define OHOS_CEF_EXT_INCLUDE_ARKWEB_CLIENT_EXT_H_
#pragma once

#include "arkweb/build/features/features.h"
#include "include/cef_client.h"
#include "include/cef_form_handler.h"
#include "include/cef_media_handler.h"
#include "include/cef_permission_request.h"
#include "include/cef_web_client_extension_handler.h"
#include "include/cef_web_extension_api_handler.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
#include "include/cef_custom_media_info.h"
#include "include/cef_custom_media_player_delegate.h"
#include "include/cef_media_player_listener.h"
#endif

///
/// Extended from CefClient
///
class ArkWebClientExt : public CefClient, public virtual CefBaseRefCounted {
 public:
  ///
  /// Return the handler for browser geolocation permission request events.
  ///
  virtual CefRefPtr<CefPermissionRequest> GetPermissionRequest() {
    return nullptr;
  }

  ///
  /// Return the handler for browser media events.
  ///
  virtual CefRefPtr<CefMediaHandler> GetMediaHandler() { return nullptr; }

  ///
  /// Returns the list of arguments NotifyJavaScriptResult.
  ///
  virtual int NotifyJavaScriptResult(CefRefPtr<CefListValue> args,
                                     const CefString& method,
                                     const CefString& object_name,
                                     CefRefPtr<CefListValue> result,
                                     int32_t routing_id,
                                     int32_t object_id) {
    return 0;
  }

  ///
  /// Returns the list of arguments NotifyJavaScriptResultFlowbuf.
  ///
  virtual int NotifyJavaScriptResultFlowbuf(CefRefPtr<CefListValue> args,
                                            const CefString& method,
                                            const CefString& object_name,
                                            int fd,
                                            CefRefPtr<CefListValue> result,
                                            int32_t routing_id,
                                            int32_t object_id) {
    return 0;
  }

  ///
  /// has javaScript object method.
  ///
  virtual bool HasJavaScriptObjectMethods(int32_t object_id,
                                          const CefString& method_name) {
    return false;
  }

  ///
  /// get javaScript object methods.
  ///
  virtual void GetJavaScriptObjectMethods(
      int32_t object_id,
      CefRefPtr<CefValue> returned_method_names) {}

  ///
  /// remove javaScript object holder.
  ///
  virtual void RemoveJavaScriptObjectHolder(int32_t holder, int32_t object_id) {
  }

  ///
  /// remove transient javaScript object holder.
  ///
  virtual void RemoveTransientJavaScriptObject() {}

  ///
  /// Return the handler for browser form events.
  ///
  virtual CefRefPtr<CefFormHandler> GetFormHandler() { return nullptr; }

  ///
  /// Called when top controls offset has been changed.
  ///
  virtual void OnTopControlsChanged(float top_controls_offset,
                                    float top_content_offset) {}

  ///
  /// Return the height of top controls.
  ///
  virtual int OnGetTopControlsHeight() { return 0; }

  ///
  /// Return the shrink renderer size of top controls.
  ///
  virtual bool DoBrowserControlsShrinkRendererSize() { return false; }

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  ///
  /// Return CefWebClientExtensionHandler.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefWebClientExtensionHandler>
  GetWebClientExtensionHandler() {
    return nullptr;
  }
#endif

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
  virtual CefOwnPtr<CefCustomMediaPlayerDelegate> OnCreateCustomMediaPlayer(
      CefOwnPtr<CefMediaPlayerListener> listener,
      const CefCustomMediaInfo& media_info) {
    return nullptr;
  }
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
  ///
  /// Return the handler for web extension api. If no handler is provided the
  /// default implementation will be used.
  ///
  virtual CefRefPtr<CefWebExtensionApiHandler> GetWebExtensionApiHandler() {
    return nullptr;
  }
#endif

#if BUILDFLAG(ARKWEB_PULL_TO_REFRESH)
  ///
  /// Notify the action of pull to refresh.
  ///
  /*--cef()--*/
  virtual bool OnPullToRefreshAction(int action) { return false; }

  ///
  /// Notify the offset of pull to refresh.
  ///
  /*--cef()--*/
  virtual void OnPullToRefreshPull(float offset_x, float offset_y) {}
#endif

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
  ///
  /// Notify the web activated by window.open.
  ///
  /*--cef()--*/
  virtual void OnActivateContent() {}
#endif
};

#endif  // OHOS_CEF_EXT_INCLUDE_ARKWEB_CLIENT_EXT_H_
