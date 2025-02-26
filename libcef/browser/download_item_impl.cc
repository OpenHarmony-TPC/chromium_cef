// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/download_item_impl.h"

#include "libcef/common/time_util.h"
#include "url/gurl.h"
#include "components/download/public/common/download_item_impl.h"

#if defined(OHOS_EX_DOWNLOAD)
#include "cef/libcef/browser/received_slice_helper.h"
const char kNWebId[] = "nweb_id";
#endif

CefDownloadItemImpl::CefDownloadItemImpl(download::DownloadItem* value)
    : CefValueBase<CefDownloadItem, download::DownloadItem>(
          value,
          nullptr,
          kOwnerNoDelete,
          true,
          new CefValueControllerNonThreadSafe()) {
  // Indicate that this object owns the controller.
  SetOwnsController();
}

CefDownloadItemImpl::CefDownloadItemImpl(download::DownloadItem* value,
                                         int nweb_id)
    : CefValueBase<CefDownloadItem, download::DownloadItem>(
          value,
          nullptr,
          kOwnerNoDelete,
          true,
          new CefValueControllerNonThreadSafe()) {
  // Indicate that this object owns the controller.
  SetOwnsController();
#if defined(OHOS_EX_DOWNLOAD)
  download::DownloadItemImpl* item_impl =
      static_cast<download::DownloadItemImpl*>(mutable_value());
  item_impl->SetUserData(
      kNWebId,
      std::make_unique<download::DownloadItemImpl::NWebIdData>(nweb_id));
#endif  //  OHOS_EX_DOWNLOAD
}

bool CefDownloadItemImpl::IsValid() {
  return !detached();
}

bool CefDownloadItemImpl::IsInProgress() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::IN_PROGRESS;
}

bool CefDownloadItemImpl::IsComplete() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::COMPLETE;
}

bool CefDownloadItemImpl::IsCanceled() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::CANCELLED;
}

int64 CefDownloadItemImpl::GetCurrentSpeed() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().CurrentSpeed();
}

int CefDownloadItemImpl::GetPercentComplete() {
  CEF_VALUE_VERIFY_RETURN(false, -1);
  return const_value().PercentComplete();
}

int64 CefDownloadItemImpl::GetTotalBytes() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetTotalBytes();
}

int64 CefDownloadItemImpl::GetReceivedBytes() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetReceivedBytes();
}

CefBaseTime CefDownloadItemImpl::GetStartTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefBaseTime());
  return const_value().GetStartTime();
}

CefBaseTime CefDownloadItemImpl::GetEndTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefBaseTime());
  return const_value().GetEndTime();
}

CefString CefDownloadItemImpl::GetFullPath() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetFullPath().value();
}

uint32 CefDownloadItemImpl::GetId() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetId();
}

CefString CefDownloadItemImpl::GetURL() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetURL().spec();
}

CefString CefDownloadItemImpl::GetOriginalUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetOriginalUrl().spec();
}

CefString CefDownloadItemImpl::GetSuggestedFileName() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetSuggestedFilename();
}

CefString CefDownloadItemImpl::GetContentDisposition() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetContentDisposition();
}

CefRefPtr<CefValue> CefDownloadItemImpl::GetOriginContentDisposition() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  CefRefPtr<CefValue> data = CefValue::Create();
  data->SetStdString(const_value().GetContentDisposition());
  return data;
}

CefString CefDownloadItemImpl::GetMimeType() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetMimeType();
}

CefString CefDownloadItemImpl::GetOriginalMimeType() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetOriginalMimeType();
}

CefString CefDownloadItemImpl::GetGuid() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetGuid();
}

// additional methods
int CefDownloadItemImpl::GetState() {
  CEF_VALUE_VERIFY_RETURN(false, -1);
  int state = static_cast<int>(const_value().GetState());
  return state;
}

bool CefDownloadItemImpl::IsPaused() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  bool paused = const_value().IsPaused();
  return paused;
}

CefString CefDownloadItemImpl::GetMethod() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
#if defined(OHOS_EX_DOWNLOAD)
  const download::DownloadItemImpl& item_impl =
      static_cast<const download::DownloadItemImpl&>(const_value());
  std::string request_method = item_impl.GetRequestMethod();
  return request_method;
#else
  return CefString();
#endif  //  OHOS_EX_DOWNLOAD
}

int CefDownloadItemImpl::GetLastErrorCode() {
  CEF_VALUE_VERIFY_RETURN(false, -1);
#if defined(OHOS_EX_DOWNLOAD)
  download::DownloadInterruptReason reason = const_value().GetLastReason();
  return static_cast<int>(reason);
#else
  return -1;
#endif  //  OHOS_EX_DOWNLOAD
}

bool CefDownloadItemImpl::IsPending() {
  CEF_VALUE_VERIFY_RETURN(false, false);
#if defined(OHOS_EX_DOWNLOAD)
  const download::DownloadItemImpl& item_impl =
      static_cast<const download::DownloadItemImpl&>(const_value());
  bool pending = item_impl.IsBeforeInProgress();
  return pending;
#else
  return false;
#endif  //  OHOS_EX_DOWNLOAD
}

CefString CefDownloadItemImpl::GetLastModifiedTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetLastModifiedTime();
}

CefString CefDownloadItemImpl::GetETag() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetETag();
}

CefString CefDownloadItemImpl::GetReceivedSlices() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
#if defined(OHOS_EX_DOWNLOAD)
  const download::DownloadItemImpl& item_impl =
      static_cast<const download::DownloadItemImpl&>(const_value());
  return received_slice_helper::SerializeToString(
      item_impl.GetReceivedSlices());
#else
  return CefString();
#endif  //  OHOS_EX_DOWNLOAD
}

int CefDownloadItemImpl::GetNWebId() {
  CEF_VALUE_VERIFY_RETURN(false, -1);
#if defined(OHOS_EX_DOWNLOAD)
  const download::DownloadItemImpl& item_impl =
      static_cast<const download::DownloadItemImpl&>(const_value());
  void* data_raw_ptr = item_impl.GetUserData(kNWebId);
  if (data_raw_ptr) {
    download::DownloadItemImpl::NWebIdData* nweb_id_data_ptr =
        (download::DownloadItemImpl::NWebIdData*)data_raw_ptr;
    if (nweb_id_data_ptr) {
      return nweb_id_data_ptr->nweb_id_;
    }
  }
  return 0;
#else
  return -1;
#endif  //  OHOS_EX_DOWNLOAD
}
