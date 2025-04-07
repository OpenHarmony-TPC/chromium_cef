// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_INCLUDE_CEF_FRAME_EXT_H_
#define CEF_INCLUDE_CEF_FRAME_EXT_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_dom.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_request.h"
#include "include/cef_stream.h"
#include "include/cef_string_visitor.h"
class CefBrowser;
class CefURLRequest;
class CefURLRequestClient;
class CefV8Context;

#if BUILDFLAG(IS_OHOS)
///
/// Interface to implement to be notified of asynchronous completion via
/// CefFrameHostImpl::GetImages.
///
/*--cef(source=client)--*/
class CefGetImagesCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion. |num_deleted| will be the
  /// number of cookies that were deleted.
  ///
  /*--cef()--*/
  virtual void GetImages(bool response) = 0;
};
#endif  // BUILDFLAG(IS_OHOS)

class ArkwebFrameExt : public virtual CefBaseRefCounted {
 public:
  ///
  /// Loads the given URL with additional HTTP headers, specified as a map
  /// from name to value. Note that if this map contains any of the headers that
  /// are set by default by this WebView, such as those controlling caching,
  /// accept types or the User-Agent, their values may be overridden by this
  /// WebView's defaults.
  ///
  /*--cef(optional_param=url, optional_param=additionalHttpHeaders)--*/
  virtual void LoadHeaderUrl(const CefString& url,
                             const CefString& additionalHttpHeaders) = 0;

  ///
  /// web has image or not
  ///
  /*--cef()--*/
  virtual void GetImages(CefRefPtr<CefGetImagesCallback> callback) = 0;

  ///
  /// PostUrl
  ///
  /*--cef(optional_param=post_data)--*/
  virtual void PostURL(const CefString& url,
                       const std::vector<char>& post_data) = 0;

  ///
  /// LoadURLWithUserGesture
  ///
  /*--cef()--*/
  virtual void LoadURLWithUserGesture(const CefString& url,
                                      bool user_gesture) = 0;

#if BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
  virtual void SetJsOnlineProperty(bool network_up) = 0;
#endif

#if BUILDFLAG(ARKWEB_SYNC_RENDER)

  ///
  /// UpdateDrawRect
  ///
  /*--cef()--*/
  virtual void UpdateDrawRect() = 0;
#endif
};
#endif  // CEF_INCLUDE_CEF_FRAME_H_
