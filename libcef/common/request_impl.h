// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#define CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#pragma once

#include <stdint.h>

#include <memory>

#include "include/cef_request.h"

#include "base/synchronization/lock.h"
#include "cef/libcef/common/mojom/cef.mojom.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "url/gurl.h"

#if defined(OHOS_SCHEME_HANDLER)
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_data_stream.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#endif

namespace blink {
class WebURLRequest;
}  // namespace blink

namespace content {
class NavigationHandle;
}  // namespace content

namespace net {
class HttpRequestHeaders;
struct RedirectInfo;
}  // namespace net

namespace network {
class DataElement;
struct ResourceRequest;
class ResourceRequestBody;
}  // namespace network

// Implementation of CefRequest
class CefRequestImpl : public CefRequest {
 public:
  enum Changes {
    kChangedNone = 0,
    kChangedUrl = 1 << 0,
    kChangedMethod = 1 << 1,
    kChangedReferrer = 1 << 2,
    kChangedPostData = 1 << 3,
    kChangedHeaderMap = 1 << 4,
    kChangedFlags = 1 << 5,
    kChangedSiteForCookies = 1 << 6,
  };

  CefRequestImpl();

  bool IsReadOnly() override;
  CefString GetURL() override;
  void SetURL(const CefString& url) override;
  CefString GetMethod() override;
  void SetMethod(const CefString& method) override;
  void SetReferrer(const CefString& referrer_url,
                   ReferrerPolicy policy) override;
  CefString GetReferrerURL() override;
  ReferrerPolicy GetReferrerPolicy() override;
  CefRefPtr<CefPostData> GetPostData() override;
#if defined(OHOS_SCHEME_HANDLER)
  CefRefPtr<CefPostDataStream> GetUploadStream() override;
  bool IsRedirect() override;
  bool HasUserGesture() override;
  void SetFrameUrl(const CefString& frame_url);
  CefString GetFrameUrl();
#endif  // defined(OHOS_SCHEME_HANDLER)
  void SetPostData(CefRefPtr<CefPostData> postData) override;
  void GetHeaderMap(HeaderMap& headerMap) override;
  void SetHeaderMap(const HeaderMap& headerMap) override;
  CefString GetHeaderByName(const CefString& name) override;
  void SetHeaderByName(const CefString& name,
                       const CefString& value,
                       bool overwrite) override;
  void Set(const CefString& url,
           const CefString& method,
           CefRefPtr<CefPostData> postData,
           const HeaderMap& headerMap) override;
  int GetFlags() override;
  void SetFlags(int flags) override;
  CefString GetFirstPartyForCookies() override;
  void SetFirstPartyForCookies(const CefString& url) override;
  ResourceType GetResourceType() override;
  TransitionType GetTransitionType() override;
  uint64 GetIdentifier() override;
#ifdef OHOS_NETWORK_CONNINFO
  bool IsMainFrame() override;
#endif
  // Populate this object from the ResourceRequest object.
  void Set(const network::ResourceRequest* request, uint64 identifier);

  // Populate the ResourceRequest object from this object.
  // If |changed_only| is true then only the changed fields will be updated.
  void Get(network::ResourceRequest* request, bool changed_only) const;

  // Populate this object from the RedirectInfo object.
  void Set(const net::RedirectInfo& redirect_info);

  // Populate this object from the HttpRequestHeaders object.
  void Set(const net::HttpRequestHeaders& headers);

  // Populate this object from the NavigationParams object.
  // Called from throttle_handler.cc NavigationOnUIThread().
  void Set(content::NavigationHandle* navigation_handle);

  // Populate the WebURLRequest object based on the contents of |params|.
  // Called from CefBrowserImpl::LoadRequest().
  static void Get(const cef::mojom::RequestParamsPtr& params,
                  blink::WebURLRequest& request);

  // Populate the RequestParams object from this object.
  // Called from CefFrameHostImpl::LoadRequest().
  void Get(cef::mojom::RequestParamsPtr& params) const;

  void SetReadOnly(bool read_only);

  // Enable or disable tracking of changes. If |track_changes| is true the
  // status of changes will be tracked, and retrievable via GetChanges(). If
  // |backup_on_change| is true the original value will be backed up before the
  // first change. The original values can later be restored by calling
  // RevertChanges() before calling SetTrackChanges(false).
  void SetTrackChanges(bool track_changes, bool backup_on_change = false);
  void RevertChanges();
  void DiscardChanges();
  uint8_t GetChanges() const;

  static network::mojom::ReferrerPolicy NetReferrerPolicyToBlinkReferrerPolicy(
      cef_referrer_policy_t net_policy);
  static cef_referrer_policy_t BlinkReferrerPolicyToNetReferrerPolicy(
      network::mojom::ReferrerPolicy blink_policy);
#ifdef OHOS_NETWORK_CONNINFO
  void SetDestination(network::mojom::RequestDestination destination);
#endif
 private:
  // Mark values as changed. Must be called before the new values are assigned.
  void Changed(uint8_t changes);

  // Used with the Set() methods that export data to other object types. Returns
  // true if the values should be set on the export object. If |changed_only| is
  // true then only return true if the value has been changed in combination
  // with track changes.
  bool ShouldSet(uint8_t changes, bool changed_only) const;

  void Reset();

  GURL url_;
  std::string method_;
  GURL referrer_url_;
  ReferrerPolicy referrer_policy_;
  CefRefPtr<CefPostData> postdata_;
  HeaderMap headermap_;
  ResourceType resource_type_;
  TransitionType transition_type_;
  uint64 identifier_;
#ifdef OHOS_NETWORK_CONNINFO
  network::mojom::RequestDestination destination_;
#endif
#if defined(OHOS_SCHEME_HANDLER)
  bool is_redirect_{false};
  bool has_user_gesture_{false};
  CefRefPtr<CefPostDataStream> postdata_stream_;
  GURL frame_url_;
#endif
  // The below members are used by CefURLRequest.
  int flags_;
  net::SiteForCookies site_for_cookies_;

  // Stores backup of values for use with track changes.
  struct Backup {
    // Bitmask of values that have been backed up.
    uint8_t backups_ = kChangedNone;

    GURL url_;
    std::string method_;
    GURL referrer_url_;
    ReferrerPolicy referrer_policy_;
    CefRefPtr<CefPostData> postdata_;
    std::unique_ptr<HeaderMap> headermap_;
    int flags_;
    net::SiteForCookies site_for_cookies_;
  };
  std::unique_ptr<Backup> backup_;

  // True if this object is read-only.
  bool read_only_ = false;

  // True if this object should track changes.
  bool track_changes_ = false;

  // True if original values should be backed up when |track_changes_| is true.
  bool backup_on_change_ = false;

  // Bitmask of |Changes| values which indicate which fields have changed.
  uint8_t changes_ = kChangedNone;

  mutable base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefRequestImpl);
};

#if defined(OHOS_SCHEME_HANDLER)
class CefPostDataStreamImpl : public CefPostDataStream {
 public:
  CefPostDataStreamImpl();
  ~CefPostDataStreamImpl();

  void SetReadCallback(
      CefRefPtr<CefPostDataStreamReadCallback> read_callback) override;

  void Init(CefRefPtr<CefPostDataStreamInitCallback> init_callback) override;
  void Read(void* buffer,
            int buf_len,
            CefRefPtr<CefPostDataStreamReadCallback> read_callback) override;
  uint64_t GetSize() override;
  uint64_t GetPosition() override;
  bool IsChunked() override;
  bool HasNullSource() override;
  bool IsEOF() override;
  bool IsInMemory() override;
  void Set(network::ResourceRequestBody* body);
  void Reset() override;

  void GetChunkedDataPipeGetter(network::ResourceRequestBody* body);

 private:
  void OnStreamInitialized(int rv);
  void OnStreamRead(scoped_refptr<net::WrappedIOBuffer> buffer,
                    base::WaitableEvent* event,
                    int rv);
  void ReadAsync(void* buffer,
            int buf_len,
            CefRefPtr<CefPostDataStreamReadCallback> read_callback);
  void OnStreamReadAsync(scoped_refptr<net::WrappedIOBuffer> buffer,
                         CefRefPtr<CefPostDataStreamReadCallback> read_callback,
                         int rv);
  void ReadOnTaskRunner(void* buffer,
            int buf_len,
            base::WaitableEvent* event);

  CefRefPtr<CefPostDataStreamReadCallback> read_callback_;
  CefRefPtr<CefPostDataStreamInitCallback> init_callback_;

  std::unique_ptr<net::UploadDataStream> upload_stream_;
  bool initialated_{false};
  bool is_data_pipe_{false};
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  int last_read_rv_ = -2;
  mutable base::Lock lock_;
  IMPLEMENT_REFCOUNTING(CefPostDataStreamImpl);
};
#endif  // defined(OHOS_SCHEME_HANDLER)
// Implementation of CefPostData
class CefPostDataImpl : public CefPostData {
 public:
  CefPostDataImpl();

  bool IsReadOnly() override;
  bool HasExcludedElements() override;
  size_t GetElementCount() override;
  void GetElements(ElementVector& elements) override;
  bool RemoveElement(CefRefPtr<CefPostDataElement> element) override;
  bool AddElement(CefRefPtr<CefPostDataElement> element) override;
  void RemoveElements() override;

  void Set(const network::ResourceRequestBody& body);
  scoped_refptr<network::ResourceRequestBody> GetBody() const;

  void SetReadOnly(bool read_only);

  void SetTrackChanges(bool track_changes);
  bool HasChanges() const;

 private:
  void Changed();

  ElementVector elements_;

  // True if this object is read-only.
  bool read_only_;

  // True if this object has excluded elements.
  bool has_excluded_elements_;

  // True if this object should track changes.
  bool track_changes_;

  // True if this object has changes.
  bool has_changes_;

  mutable base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefPostDataImpl);
};

// Implementation of CefPostDataElement
class CefPostDataElementImpl : public CefPostDataElement {
 public:
  CefPostDataElementImpl();
  ~CefPostDataElementImpl() override;

  bool IsReadOnly() override;
  void SetToEmpty() override;
  void SetToFile(const CefString& fileName) override;
  void SetToBytes(size_t size, const void* bytes) override;
  Type GetType() override;
  CefString GetFile() override;
  size_t GetBytesCount() override;
  size_t GetBytes(size_t size, void* bytes) override;

  void* GetBytes() { return data_.bytes.bytes; }

  void Set(const network::DataElement& element);
  void Get(network::ResourceRequestBody& body) const;

  void SetReadOnly(bool read_only);

  void SetTrackChanges(bool track_changes);
  bool HasChanges() const;

 private:
  void Changed();
  void Cleanup();

  Type type_;
  union {
    struct {
      void* bytes;
      size_t size;
    } bytes;
    cef_string_t filename;
  } data_;

  // True if this object is read-only.
  bool read_only_;

  // True if this object should track changes.
  bool track_changes_;

  // True if this object has changes.
  bool has_changes_;

  mutable base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefPostDataElementImpl);
};

#endif  // CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
