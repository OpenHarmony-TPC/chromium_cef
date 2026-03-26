#ifndef CEF_INCLUDE_CEF_DOWNLOAD_ITEM_EXT_H_
#define CEF_INCLUDE_CEF_DOWNLOAD_ITEM_EXT_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_values.h"

class CefDownloadItemExt : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the original mime type.
  ///
  /*--cef()--*/
  virtual CefString GetOriginalMimeType() = 0;

  ///
  /// Returns the guid.
  ///
  /*--cef()--*/
  virtual CefString GetGuid() = 0;

  ///
  /// Returns the download state,
  /// IN_PROGRESS,COMPLETE,CANCELED,INTERRUPTED,PENDING,PAUSED.
  ///
  /*--cef()--*/
  virtual int GetState() = 0;

  ///
  /// Returns the download if paused.
  ///
  /*--cef()--*/
  virtual bool IsPaused() = 0;

  ///
  /// Returns the download request method.
  ///
  /*--cef()--*/
  virtual CefString GetMethod() = 0;

  ///
  /// Returns the download last error code.
  ///
  /*--cef()--*/
  virtual int GetLastErrorCode() = 0;

  ///
  /// Returns if the download is pending.
  ///
  /*--cef()--*/
  virtual bool IsPending() = 0;

  ///
  /// Returns the download last modified time.
  ///
  /*--cef()--*/
  virtual CefString GetLastModifiedTime() = 0;

  ///
  /// Returns the download etag.
  ///
  /*--cef()--*/
  virtual CefString GetETag() = 0;

  ///
  /// Returns the download received slices.
  ///
  /*--cef()--*/
  virtual CefString GetReceivedSlices() = 0;

  ///
  /// Get nweb id.
  ///
  /*--cef()--*/
  virtual int GetNWebId() = 0;

  ///
  /// Returns the origin content disposition.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefValue> GetOriginContentDisposition() = 0;

  ///
  /// Returns can resume.
  ///
  /*--cef()--*/
  virtual bool CanResume() = 0;

  ///
  /// Returns referrer url.
  ///
  /*--cef()--*/
  virtual CefString GetReferrerUrl() = 0;

  ///
  /// Returns request initiator.
  ///
  /*--cef()--*/
  virtual CefString GetRequestInitiator() = 0;

  ///
  /// Returns referrer url.
  ///
  /*--cef()--*/
  virtual bool IsTransient() = 0;

  ///
  /// Returns the download resource,
  ///
  /*--cef()--*/
  virtual int GetDownloadSource() = 0;

  ///
  /// Returns the target disposition.
  ///
  /*--cef()--*/
  virtual int GetTargetDisposition() = 0;

  ///
  /// Returns the extension id.
  ///
  /*--cef()--*/
  virtual CefString GetByExtensionId() = 0;

  ///
  /// Returns the extension name.
  ///
  /*--cef()--*/
  virtual CefString GetByExtensionName() = 0;

  ///
  /// Returns the conflict action.
  ///
  /*--cef()--*/
  virtual int GetConflictAction() = 0;

  ///
  /// Returns url chain.
  ///
  /*--cef()--*/
  virtual std::vector<CefString> GetUrlChain() {return {};}
};
#endif
