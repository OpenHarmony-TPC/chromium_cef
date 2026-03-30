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

#ifndef OHOS_CEF_EXT_INCLUDE_ARKWEB_REQUEST_EXT_H_
#define OHOS_CEF_EXT_INCLUDE_ARKWEB_REQUEST_EXT_H_
#pragma once

#include "arkweb/build/features/features.h"
#include "include/cef_request.h"
#include "cef_base_nopac.h"

#if BUILDFLAG(ARKWEB_SCHEME_HANDLER)
class ArkWebCefPostDataStream;

namespace network {
class ResourceRequestBody;
}
#endif

///
/// Extended of CefRequest
///
class ArkWebRequestExt : public virtual CefRequest,
                         public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns whether the request was made for the main frame document.
  /// Will be false for subresources or iframes
  ///
  virtual bool IsMainFrame() = 0;

#if BUILDFLAG(ARKWEB_SCHEME_HANDLER)
  ///
  /// Returns whether the request was redirect.
  ///
  virtual bool IsRedirect() = 0;

  ///
  /// Returns whether the request was triggered by user gesture.
  ///
  virtual bool HasUserGesture() = 0;

  ///
  /// Get the upload stream.
  ///
  virtual CefRefPtr<ArkWebCefPostDataStream> GetUploadStream() = 0;

  ///
  /// Set frame url.
  ///
  virtual void SetFrameUrl(const CefString& frame_url) = 0;

  ///
  /// Get frame url.
  ///
  virtual CefString GetFrameUrl() = 0;
#endif  // BUILDFLAG(ARKWEB_SCHEME_HANDLER)
};

#if BUILDFLAG(ARKWEB_SCHEME_HANDLER)
///
/// Callback for init the stream.
///
class ArkWebCefPostDataStreamInitCallback : public virtual CefBaseRefCountedNopac {
 public:
  ///
  /// Callback for init the stream.
  ///
  virtual void OnInitComplete(int result) = 0;
};

///
/// Callback for read from stream.
///
class ArkWebCefPostDataStreamReadCallback : public virtual CefBaseRefCountedNopac {
 public:
  ///
  /// Callback for read from stream.
  ///
  virtual void OnReadComplete(char* buffer, int bytes_read) = 0;
};

///
/// Callback for async read from stream.
///
class ArkWebCefPostDataStreamAsyncReadCallback
    : public virtual CefBaseRefCountedNopac {
 public:
  ///
  /// Callback for async read from stream.
  ///
  virtual void OnAsyncReadComplete(char* buffer, int bytes_read) = 0;
};

///
/// Class used to represent post data for a web request. The methods of this
/// class may be called on any thread.
///
class ArkWebCefPostDataStream : public virtual CefBaseRefCounted {
 public:
  ///
  /// Create a new CefPostDataStream object.
  ///
  static CefRefPtr<ArkWebCefPostDataStream> Create();

  ///
  /// Set ready callback.
  ///
  virtual void SetReadCallback(
      CefRefPtr<ArkWebCefPostDataStreamReadCallback> read_callback) = 0;

  ///
  /// Init the stream.
  ///
  virtual void Init(
      CefRefPtr<ArkWebCefPostDataStreamInitCallback> init_callback) = 0;

  ///
  /// Read the stream.
  ///
  virtual void Read(
      void* buffer,
      int buf_len,
      CefRefPtr<ArkWebCefPostDataStreamReadCallback> read_callback) = 0;

  ///
  /// AsyncRead the stream.
  ///
  virtual void AsyncRead(
      void* buffer,
      int buf_len,
      CefRefPtr<ArkWebCefPostDataStreamAsyncReadCallback> read_callback) = 0;

  ///
  /// Get the size of stream.
  ///
  virtual uint64_t GetSize() = 0;

  ///
  /// Get the position of stream.
  ///
  virtual uint64_t GetPosition() = 0;

  ///
  /// Get if the stream is trunked.
  ///
  virtual bool IsChunked() = 0;

  ///
  /// Get if the stream is trunked.
  ///
  virtual bool HasNullSource() = 0;

  ///
  /// Get if the stream is trunked.
  ///
  virtual bool IsEOF() = 0;

  ///
  /// Get if the stream is trunked.
  ///
  virtual bool IsInMemory() = 0;

  ///
  /// Reset();
  ///
  virtual void Reset() = 0;

  ///
  /// GetChunkedDataPipeGetter ARKWEB_NETWORK_BASE
  ///
  virtual void GetChunkedDataPipeGetter(network::ResourceRequestBody* body) = 0;
};
#endif  // BUILDFLAG(ARKWEB_SCHEME_HANDLER)

#endif  // OHOS_CEF_EXT_INCLUDE_ARKWEB_REQUEST_EXT_H_
