// Copyright (c) 2012 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file must follow a specific format in order to
// support the CEF translator tool. See the translator.README.txt file in the
// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_BROWSER_H_
#define CEF_INCLUDE_CEF_BROWSER_H_
#pragma once

#include <vector>
#include <string>
#include <set>

#include "include/cef_base.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_drag_data.h"
#include "include/cef_frame.h"
#include "include/cef_image.h"
#include "include/cef_navigation_entry.h"
#include "include/cef_registration.h"
#include "include/cef_request_context.h"

#if BUILDFLAG(IS_OHOS)
#include "include/cef_permission_request.h"
#include "include/cef_task.h"
#include "include/cef_download_item.h"
#include "include/internal/cef_string_map.h"
#endif  // BUILDFLAG(IS_OHOS)

#ifdef OHOS_DEVTOOLS
class CefDevToolsMessageHandlerDelegate;
#endif // OHOS_DEVTOOLS

class CefBrowserHost;
class CefClient;

#if BUILDFLAG(IS_OHOS)
///
/// Interface to implement to be notified of asynchronous completion via
/// CefBrowserHostBase::ExecuteJavaScript().
///
/*--cef(source=client)--*/
class CefJavaScriptResultCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion. |num_deleted| will be the
  /// number of cookies that were deleted.
  ///
  /*--cef()--*/
  virtual void OnJavaScriptExeResult(CefRefPtr<CefValue> result) = 0;
};

/* ---------- ohos webview add begin --------- */
///
/// Interface to implement to be notified of asynchronous completion via
/// CefBrowserHostBase::StoreWebArchive().
///
/*--cef(source=client)--*/
class CefStoreWebArchiveResultCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion. |result| will either be the
  /// filename under which the file was saved, or empty if saving the file
  /// failed.
  ///
  /*--cef(optional_param=result)--*/
  virtual void OnStoreWebArchiveDone(const CefString& result) = 0;
};

///
/// Interface to implement to be notified of asynchronous completion via
/// CefBrowserHostBase::SetGestureEventResult().
///
/*--cef(source=client)--*/
class CefGestureEventCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion.
  ///
  /*--cef()--*/
  virtual void ContinueTask(bool result, bool stopPropagation) = 0;
};

///
/// Interface to implement to be notified of asynchronous web message channel.
///
/*--cef(source=client)--*/
class CefWebMessageReceiver : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon |PostPortMessage|. |message| will be sent
  /// to another end of web message channel.
  ///
  /*--cef()--*/
  virtual void OnMessage(CefRefPtr<CefValue> message) = 0;

  ///
  /// The same as OnMessage, the result of the execution will be returned.
  ///
  /*--cef()--*/
  virtual bool OnMessageWithBoolResult(CefRefPtr<CefValue> message) = 0;
};

///
/// CefSetLockCallback.
///
/*--cef(source=client)--*/
class CefSetLockCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Handle.
  ///
  /*--cef()--*/
  virtual void Handle(bool key) = 0;
};

///
/// callback for RegisterScreenCaptureDelegateListener().
///
/*--cef(source=client)--*/
class CefScreenCaptureCallback : public virtual CefBaseRefCounted {
public:
  ///
  /// Method that will be called upon completion.
  ///
  /*--cef()--*/
  virtual void OnStateChange(int32_t nweb_id, const CefString& session_id, int32_t code) = 0;
};

///
/// Interface to implement to be notified of compiling javascript completion via
/// CefBrowserHostBase::PrecompileJavaScript().
///
/*--cef(source=client)--*/
class CefPrecompileCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion.
  ///
  /*--cef()--*/
  virtual void OnPrecompileFinished(int32_t result) = 0;
};

///
/// options and info of CefBrowserHostBase::PrecompileJavaScript().
///
/*--cef(source=library)--*/
class CefCacheOptions : public virtual CefBaseRefCounted {
 public:
  ///
  /// Return the response headers of javascript request.
  ///
  /*--cef(default_retval=nullptr)--*/
  virtual cef_string_map_t GetResponseHeaders() = 0;
};
/* ---------- ohos webview add end --------- */
#endif  // BUILDFLAG(IS_OHOS)

///
/// Class used to represent a browser. When used in the browser process the
/// methods of this class may be called on any thread unless otherwise indicated
/// in the comments. When used in the render process the methods of this class
/// may only be called on the main thread.
///
/*--cef(source=library)--*/
class CefBrowser : public virtual CefBaseRefCounted {
 public:
  ///
  /// True if this object is currently valid. This will return false after
  /// CefLifeSpanHandler::OnBeforeClose is called.
  ///
  /*--cef()--*/
  virtual bool IsValid() = 0;

  ///
  /// Returns the browser host object. This method can only be called in the
  /// browser process.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefBrowserHost> GetHost() = 0;

  ///
  /// Returns true if the browser can navigate backwards.
  ///
  /*--cef()--*/
  virtual bool CanGoBack() = 0;

  ///
  /// Navigate backwards.
  ///
  /*--cef()--*/
  virtual void GoBack() = 0;

  ///
  /// Returns true if the browser can navigate forwards.
  ///
  /*--cef()--*/
  virtual bool CanGoForward() = 0;

  ///
  /// Navigate forwards.
  ///
  /*--cef()--*/
  virtual void GoForward() = 0;

  ///
  /// Returns true if the browser is currently loading.
  ///
  /*--cef()--*/
  virtual bool IsLoading() = 0;

  ///
  /// Reload the current page.
  ///
  /*--cef()--*/
  virtual void Reload() = 0;

  ///
  /// Reload the current page ignoring any cached data.
  ///
  /*--cef()--*/
  virtual void ReloadIgnoreCache() = 0;

  ///
  /// Stop loading the page.
  ///
  /*--cef()--*/
  virtual void StopLoad() = 0;

  ///
  /// Returns the globally unique identifier for this browser. This value is
  /// also used as the tabId for extension APIs.
  ///
  /*--cef()--*/
  virtual int GetIdentifier() = 0;

  ///
  /// Returns true if this object is pointing to the same handle as |that|
  /// object.
  ///
  /*--cef()--*/
  virtual bool IsSame(CefRefPtr<CefBrowser> that) = 0;

  ///
  /// Returns true if the browser is a popup.
  ///
  /*--cef()--*/
  virtual bool IsPopup() = 0;

  ///
  /// Returns true if a document has been loaded in the browser.
  ///
  /*--cef()--*/
  virtual bool HasDocument() = 0;

  ///
  /// Returns the main (top-level) frame for the browser. In the browser process
  /// this will return a valid object until after
  /// CefLifeSpanHandler::OnBeforeClose is called. In the renderer process this
  /// will return NULL if the main frame is hosted in a different renderer
  /// process (e.g. for cross-origin sub-frames). The main frame object will
  /// change during cross-origin navigation or re-navigation after renderer
  /// process termination (due to crashes, etc).
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFrame> GetMainFrame() = 0;

  ///
  /// Returns the focused frame for the browser.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFrame> GetFocusedFrame() = 0;

  ///
  /// Returns the frame with the specified identifier, or NULL if not found.
  ///
  /*--cef(capi_name=get_frame_byident)--*/
  virtual CefRefPtr<CefFrame> GetFrame(int64 identifier) = 0;

  ///
  /// Returns the frame with the specified name, or NULL if not found.
  ///
  /*--cef(optional_param=name)--*/
  virtual CefRefPtr<CefFrame> GetFrame(const CefString& name) = 0;

  ///
  /// Returns the number of frames that currently exist.
  ///
  /*--cef()--*/
  virtual size_t GetFrameCount() = 0;

  ///
  /// Returns the identifiers of all existing frames.
  ///
  /*--cef(count_func=identifiers:GetFrameCount)--*/
  virtual void GetFrameIdentifiers(std::vector<int64>& identifiers) = 0;

  ///
  /// Returns the names of all existing frames.
  ///
  /*--cef()--*/
  virtual void GetFrameNames(std::vector<CefString>& names) = 0;

  ///
  /// Returns the Permission Request Delegate object.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefBrowserPermissionRequestDelegate>
  GetPermissionRequestDelegate() = 0;

  ///
  /// Returns the Geolocation Permission handler object.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefGeolocationAcess> GetGeolocationPermissions() = 0;

#if BUILDFLAG(IS_OHOS)
  ///
  /// Returns true if the browser can navigate forwards.
  ///
  /*--cef()--*/
  virtual bool CanGoBackOrForward(int num_steps) = 0;

  ///
  /// Navigate backwards or forwards.
  ///
  /*--cef()--*/
  virtual void GoBackOrForward(int num_steps) = 0;

  ///
  /// DeleteHistory
  ///
  /*--cef()--*/
  virtual void DeleteHistory() = 0;

  ///
  /// display the selection control when click Free copy interface
  ///
  /*--cef()--*/
  virtual void ShowFreeCopyMenu() = 0;

  ///
  /// should show free copy menu
  ///
  /*--cef()--*/
  virtual bool ShouldShowFreeCopyMenu() = 0;

  ///
  /// select password dialog to fill
  ///
  /*--cef()--*/
  virtual void PasswordSuggestionSelected(int list_index) = 0;

  ///
  /// Update browser controls state.
  ///
  /*--cef()--*/
  virtual void UpdateBrowserControlsState(int constraints,
                                          int current,
                                          bool animate) = 0;

  ///
  /// Update browser controls height.
  ///
  /*--cef()--*/
  virtual void UpdateBrowserControlsHeight(int height, bool animate) = 0;
  ///
  /// Prefetch the resources required by the page, but will not execute js or
  /// render the page.
  ///
  /*--cef()--*/
  virtual void PrefetchPage(CefString& url,
                            CefString& additionalHttpHeaders) = 0;

  /* ---------- ohos_nweb_ex add begin --------- */
  ///
  /// Reload the current page with original url.
  ///
  /*--cef()--*/
  virtual void ReloadOriginalUrl() = 0;

  ///
  /// Can save current page as a archive.
  ///
  /*--cef()--*/
  virtual bool CanStoreWebArchive() = 0;

  ///
  /// Set user agent for current page.
  ///
  /*--cef()--*/
  virtual void SetBrowserUserAgentString(const CefString& user_agent) = 0;

  ///
  /// Is loading to different document.
  ///
  /*--cef()--*/
  virtual bool ShouldShowLoadingUI() = 0;

  ///
  /// Set force enable zoom.
  ///
  /*--cef()--*/
  virtual void SetForceEnableZoom(bool forceEnableZoom) = 0;

  ///
  /// Whether force enable zoom had been enabled.
  ///
  /*--cef()--*/
  virtual bool GetForceEnableZoom() = 0;

  ///
  /// Returns the NWeb Id.
  ///
  /*--cef()--*/
  virtual int GetNWebId() = 0;

  ///
  /// Get whether Ads block is enabled.
  ///
  /*--cef()--*/
  virtual bool IsAdsBlockEnabled() = 0;

  ///
  /// Get whether Ads block is enabled for current page.
  ///
  /*--cef()--*/
  virtual bool IsAdsBlockEnabledForCurPage() = 0;

  ///
  /// Set enable to allow automatically save password
  ///
  /*--cef()--*/
  virtual void EnableAdsBlock(bool enable) = 0;

  ///
  /// Whether automatically saving password had been enabled.
  ///
  /*--cef()--*/
  virtual bool GetSavePasswordAutomatically() = 0;

  ///
  /// Set enable to allow automatically save password
  ///
  /*--cef()--*/
  virtual void SetSavePasswordAutomatically(bool enable) = 0;

  ///
  /// save or upddate current page password
  ///
  /*--cef()--*/
  virtual void SaveOrUpdatePassword(bool is_update) = 0;

  ///
  /// Whether saving password had been enabled.
  ///
  /*--cef()--*/
  virtual bool GetSavePassword() = 0;

  ///
  /// Set enable to save password
  ///
  /*--cef()--*/
  virtual void SetSavePassword(bool enable) = 0;

  ///
  /// Enable the ability to check website security risks.
  ///
  /*--cef()--*/
  virtual void EnableSafeBrowsing(bool enable) = 0;

  ///
  /// Get whether checking website security risks is enabled.
  ///
  /*--cef()--*/
  virtual bool IsSafeBrowsingEnabled() = 0;

  ///
  /// Get security level for current page.
  ///
  /*--cef()--*/
  virtual int GetSecurityLevel() = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual int InsertBackForwardEntry(int index, const CefString& url) = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual int UpdateNavigationEntryUrl(int index, const CefString& url) = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual void ClearForwardList() = 0;

  ///
  /// Enable the ability to intelligent tracking prevention, default disabled.
  ///
  /*--cef()--*/
  virtual void EnableIntelligentTrackingPrevention(bool enable) = 0;

  ///
  /// Get whether intelligent tracking prevention is enabled.
  ///
  /*--cef()--*/
  virtual bool IsIntelligentTrackingPreventionEnabled() = 0;

  ///
  /// Set url trust list.
  ///
  /*--cef()--*/
  virtual int SetUrlTrustListWithErrMsg(
    const CefString& urlTrustList, CefString& detailErrMsg) = 0;

  ///
  /// Set tabId.
  ///
  /*--cef()--*/
  virtual void SetTabId(int tab_id) = 0;

  ///
  /// Get tabId.
  ///
  /*--cef()--*/
  virtual int GetTabId() = 0;

  ///
  /// Set back forward cache options.
  ///
  /*--cef()--*/
  virtual void SetBackForwardCacheOptions(int32_t size, int32_t timeToLive) = 0;

  ///
  /// Get window id.
  ///
  /*--cef()--*/
  virtual uint32_t GetAcceleratedWidget() = 0;

  ///
  /// Enable safe browsing detection.
  ///
  /*--cef()--*/
  virtual void EnableSafeBrowsingDetection(bool enable, bool strictMode) = 0;

  ///
  /// Determine if BeforeUnload or Unload events need to be triggered.
  ///
  /*--cef()--*/
  virtual bool NeedToFireBeforeUnloadOrUnloadEvents() = 0;

  ///
  /// Trigger the BeforeUnload event with an option to auto-cancel.
  ///
  /*--cef()--*/
  virtual void DispatchBeforeUnload() = 0;

  /* ---------- ohos_nweb_ex add end --------- */
#endif  // BUILDFLAG(IS_OHOS)
};

///
/// Callback interface for CefBrowserHost::RunFileDialog. The methods of this
/// class will be called on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefRunFileDialogCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called asynchronously after the file dialog is dismissed.
  /// |file_paths| will be a single value or a list of values depending on the
  /// dialog mode. If the selection was cancelled |file_paths| will be empty.
  ///
  /*--cef(optional_param=file_paths)--*/
  virtual void OnFileDialogDismissed(
      const std::vector<CefString>& file_paths) = 0;
};

///
/// Callback interface for CefBrowserHost::GetNavigationEntries. The methods of
/// this class will be called on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefNavigationEntryVisitor : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be executed. Do not keep a reference to |entry| outside
  /// of this callback. Return true to continue visiting entries or false to
  /// stop. |current| is true if this entry is the currently loaded navigation
  /// entry. |index| is the 0-based index of this entry and |total| is the total
  /// number of entries.
  ///
  /*--cef()--*/
  virtual bool Visit(CefRefPtr<CefNavigationEntry> entry,
                     bool current,
                     int index,
                     int total) = 0;
};

///
/// Callback interface for CefBrowserHost::PrintToPDF. The methods of this class
/// will be called on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefPdfPrintCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be executed when the PDF printing has completed. |path|
  /// is the output path. |ok| will be true if the printing completed
  /// successfully or false otherwise.
  ///
  /*--cef()--*/
  virtual void OnPdfPrintFinished(const CefString& path, bool ok) = 0;
};

///
/// Callback interface for CefBrowserHost::DownloadImage. The methods of this
/// class will be called on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefDownloadImageCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be executed when the image download has completed.
  /// |image_url| is the URL that was downloaded and |http_status_code| is the
  /// resulting HTTP status code. |image| is the resulting image, possibly at
  /// multiple scale factors, or empty if the download failed.
  ///
  /*--cef(optional_param=image)--*/
  virtual void OnDownloadImageFinished(const CefString& image_url,
                                       int http_status_code,
                                       CefRefPtr<CefImage> image) = 0;
};

///
/// Callback interface for CefBrowserHost::CreateToPDF. The methods of this
/// class will be called on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefPdfValueCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be executed when the PDF create has completed. |value|
  /// is the output pdf data stream.
  /// successfully or false otherwise.
  ///
  /*--cef()--*/
  virtual void OnReceiveValue(const char* value, const long size) = 0;
};

///
/// Class used to represent the browser process aspects of a browser. The
/// methods of this class can only be called in the browser process. They may be
/// called on any thread in that process unless otherwise indicated in the
/// comments.
///
/*--cef(source=library)--*/
class CefBrowserHost : public virtual CefBaseRefCounted {
 public:
  typedef cef_drag_operations_mask_t DragOperationsMask;
  typedef cef_file_dialog_mode_t FileDialogMode;
  typedef cef_mouse_button_type_t MouseButtonType;
  typedef cef_paint_element_type_t PaintElementType;

  ///
  /// Create a new browser using the window parameters specified by
  /// |windowInfo|. All values will be copied internally and the actual window
  /// (if any) will be created on the UI thread. If |request_context| is empty
  /// the global request context will be used. This method can be called on any
  /// browser process thread and will not block. The optional |extra_info|
  /// parameter provides an opportunity to specify extra information specific to
  /// the created browser that will be passed to
  /// CefRenderProcessHandler::OnBrowserCreated() in the render process.
  ///
  /*--cef(optional_param=client,optional_param=url,
          optional_param=request_context,optional_param=extra_info)--*/
  static bool CreateBrowser(const CefWindowInfo& windowInfo,
                            CefRefPtr<CefClient> client,
                            const CefString& url,
                            const CefBrowserSettings& settings,
                            CefRefPtr<CefDictionaryValue> extra_info,
                            CefRefPtr<CefRequestContext> request_context);

  ///
  /// Create a new browser using the window parameters specified by
  /// |windowInfo|. If |request_context| is empty the global request context
  /// will be used. This method can only be called on the browser process UI
  /// thread. The optional |extra_info| parameter provides an opportunity to
  /// specify extra information specific to the created browser that will be
  /// passed to CefRenderProcessHandler::OnBrowserCreated() in the render
  /// process.
  ///
  /*--cef(optional_param=client,optional_param=url,
          optional_param=request_context,optional_param=extra_info)--*/
  static CefRefPtr<CefBrowser> CreateBrowserSync(
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue> extra_info,
      CefRefPtr<CefRequestContext> request_context);

  ///
  /// Returns the hosted browser object.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefBrowser> GetBrowser() = 0;

  ///
  /// Request that the browser close. The JavaScript 'onbeforeunload' event will
  /// be fired. If |force_close| is false the event handler, if any, will be
  /// allowed to prompt the user and the user can optionally cancel the close.
  /// If |force_close| is true the prompt will not be displayed and the close
  /// will proceed. Results in a call to CefLifeSpanHandler::DoClose() if the
  /// event handler allows the close or if |force_close| is true. See
  /// CefLifeSpanHandler::DoClose() documentation for additional usage
  /// information.
  ///
  /*--cef()--*/
  virtual void CloseBrowser(bool force_close) = 0;

  ///
  /// Helper for closing a browser. Call this method from the top-level window
  /// close handler (if any). Internally this calls CloseBrowser(false) if the
  /// close has not yet been initiated. This method returns false while the
  /// close is pending and true after the close has completed. See
  /// CloseBrowser() and CefLifeSpanHandler::DoClose() documentation for
  /// additional usage information. This method must be called on the browser
  /// process UI thread.
  ///
  /*--cef()--*/
  virtual bool TryCloseBrowser() = 0;

  ///
  /// Set whether the browser is focused.
  ///
  /*--cef()--*/
  virtual void SetFocus(bool focus) = 0;

  ///
  /// Retrieve the window handle (if any) for this browser. If this browser is
  /// wrapped in a CefBrowserView this method should be called on the browser
  /// process UI thread and it will return the handle for the top-level native
  /// window.
  ///
  /*--cef()--*/
  virtual CefWindowHandle GetWindowHandle() = 0;

  ///
  /// Retrieve the window handle (if any) of the browser that opened this
  /// browser. Will return NULL for non-popup browsers or if this browser is
  /// wrapped in a CefBrowserView. This method can be used in combination with
  /// custom handling of modal windows.
  ///
  /*--cef()--*/
  virtual CefWindowHandle GetOpenerWindowHandle() = 0;

  ///
  /// Returns true if this browser is wrapped in a CefBrowserView.
  ///
  /*--cef()--*/
  virtual bool HasView() = 0;

  ///
  /// Returns the client for this browser.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefClient> GetClient() = 0;

  ///
  /// Returns the request context for this browser.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefRequestContext> GetRequestContext() = 0;

  ///
  /// Get the current zoom level. The default zoom level is 0.0. This method can
  /// only be called on the UI thread.
  ///
  /*--cef()--*/
  virtual double GetZoomLevel() = 0;

  ///
  /// Change the zoom level to the specified value. Specify 0.0 to reset the
  /// zoom level. If called on the UI thread the change will be applied
  /// immediately. Otherwise, the change will be applied asynchronously on the
  /// UI thread.
  ///
  /*--cef()--*/
  virtual void SetZoomLevel(double zoomLevel) = 0;

  ///
  /// Call to run a file chooser dialog. Only a single file chooser dialog may
  /// be pending at any given time. |mode| represents the type of dialog to
  /// display. |title| to the title to be used for the dialog and may be empty
  /// to show the default title ("Open" or "Save" depending on the mode).
  /// |default_file_path| is the path with optional directory and/or file name
  /// component that will be initially selected in the dialog. |accept_filters|
  /// are used to restrict the selectable file types and may any combination of
  /// (a) valid lower-cased MIME types (e.g. "text/*" or "image/*"), (b)
  /// individual file extensions (e.g.
  /// ".txt" or ".png"), or (c) combined description and file extension
  /// delimited using "|" and ";" (e.g. "Image Types|.png;.gif;.jpg").
  /// |callback| will be executed after the dialog is dismissed or immediately
  /// if another dialog is already pending. The dialog will be initiated
  /// asynchronously on the UI thread.
  ///
  /*--cef(optional_param=title,optional_param=default_file_path,
          optional_param=accept_filters)--*/
  virtual void RunFileDialog(FileDialogMode mode,
                             const CefString& title,
                             const CefString& default_file_path,
                             const std::vector<CefString>& accept_filters,
                             CefRefPtr<CefRunFileDialogCallback> callback) = 0;

  ///
  /// Download the file at |url| using CefDownloadHandler.
  ///
  /*--cef(optional_param=post_body)--*/
  virtual void StartDownload(const CefString& url) = 0;

  ///
  /// Download |image_url| and execute |callback| on completion with the images
  /// received from the renderer. If |is_favicon| is true then cookies are not
  /// sent and not accepted during download. Images with density independent
  /// pixel (DIP) sizes larger than |max_image_size| are filtered out from the
  /// image results. Versions of the image at different scale factors may be
  /// downloaded up to the maximum scale factor supported by the system. If
  /// there are no image results <= |max_image_size| then the smallest image is
  /// resized to |max_image_size| and is the only result. A |max_image_size| of
  /// 0 means unlimited. If |bypass_cache| is true then |image_url| is requested
  /// from the server even if it is present in the browser cache.
  ///
  /*--cef()--*/
  virtual void DownloadImage(const CefString& image_url,
                             bool is_favicon,
                             uint32 max_image_size,
                             bool bypass_cache,
                             CefRefPtr<CefDownloadImageCallback> callback) = 0;

  ///
  /// Print the current browser contents.
  ///
  /*--cef()--*/
  virtual void Print() = 0;

  ///
  /// Print the current browser contents to the PDF file specified by |path| and
  /// execute |callback| on completion. The caller is responsible for deleting
  /// |path| when done. For PDF printing to work on Linux you must implement the
  /// CefPrintHandler::GetPdfPaperSize method.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void PrintToPDF(const CefString& path,
                          const CefPdfPrintSettings& settings,
                          CefRefPtr<CefPdfPrintCallback> callback) = 0;

  ///
  /// Search for |searchText|. |forward| indicates whether to search forward or
  /// backward within the page. |matchCase| indicates whether the search should
  /// be case-sensitive. |findNext| indicates whether this is the first request
  /// or a follow-up. The search will be restarted if |searchText| or
  /// |matchCase| change. The search will be stopped if |searchText| is empty.
  /// The CefFindHandler instance, if any, returned via
  /// CefClient::GetFindHandler will be called to report find results.
  ///
  /*--cef()--*/
  virtual void Find(const CefString& searchText,
                    bool forward,
                    bool matchCase,
                    bool findNext,
                    bool newSession) = 0;

  ///
  /// Cancel all searches that are currently going on.
  ///
  /*--cef()--*/
  virtual void StopFinding(bool clearSelection) = 0;

  ///
  /// Open developer tools (DevTools) in its own browser. The DevTools browser
  /// will remain associated with this browser. If the DevTools browser is
  /// already open then it will be focused, in which case the |windowInfo|,
  /// |client| and |settings| parameters will be ignored. If
  /// |inspect_element_at| is non-empty then the element at the specified (x,y)
  /// location will be inspected. The |windowInfo| parameter will be ignored if
  /// this browser is wrapped in a CefBrowserView.
  ///
  /*--cef(optional_param=windowInfo,optional_param=client,
          optional_param=settings,optional_param=inspect_element_at)--*/
  virtual void ShowDevTools(const CefWindowInfo& windowInfo,
                            CefRefPtr<CefClient> client,
                            const CefBrowserSettings& settings,
                            const CefPoint& inspect_element_at) = 0;

#ifdef OHOS_DEVTOOLS
  ///
  /// Opend DevTools with frontend_browser.
  ///
  /*--cef()--*/
  virtual void ShowDevToolsWith(
      CefRefPtr<CefBrowserHost> frontend_browser,
      CefRefPtr<CefDevToolsMessageHandlerDelegate> delegate,
      const CefPoint& inspect_element_at) = 0;
#endif // OHOS_DEVTOOLS

  ///
  /// Explicitly close the associated DevTools browser, if any.
  ///
  /*--cef()--*/
  virtual void CloseDevTools() = 0;

  ///
  /// Returns true if this browser currently has an associated DevTools browser.
  /// Must be called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual bool HasDevTools() = 0;

  ///
  /// Send a method call message over the DevTools protocol. |message| must be a
  /// UTF8-encoded JSON dictionary that contains "id" (int), "method" (string)
  /// and "params" (dictionary, optional) values. See the DevTools protocol
  /// documentation at https://chromedevtools.github.io/devtools-protocol/ for
  /// details of supported methods and the expected "params" dictionary
  /// contents. |message| will be copied if necessary. This method will return
  /// true if called on the UI thread and the message was successfully submitted
  /// for validation, otherwise false. Validation will be applied asynchronously
  /// and any messages that fail due to formatting errors or missing parameters
  /// may be discarded without notification. Prefer ExecuteDevToolsMethod if a
  /// more structured approach to message formatting is desired.
  ///
  /// Every valid method call will result in an asynchronous method result or
  /// error message that references the sent message "id". Event messages are
  /// received while notifications are enabled (for example, between method
  /// calls for "Page.enable" and "Page.disable"). All received messages will be
  /// delivered to the observer(s) registered with AddDevToolsMessageObserver.
  /// See CefDevToolsMessageObserver::OnDevToolsMessage documentation for
  /// details of received message contents.
  ///
  /// Usage of the SendDevToolsMessage, ExecuteDevToolsMethod and
  /// AddDevToolsMessageObserver methods does not require an active DevTools
  /// front-end or remote-debugging session. Other active DevTools sessions will
  /// continue to function independently. However, any modification of global
  /// browser state by one session may not be reflected in the UI of other
  /// sessions.
  ///
  /// Communication with the DevTools front-end (when displayed) can be logged
  /// for development purposes by passing the
  /// `--devtools-protocol-log-file=<path>` command-line flag.
  ///
  /*--cef()--*/
  virtual bool SendDevToolsMessage(const void* message,
                                   size_t message_size) = 0;

  ///
  /// Execute a method call over the DevTools protocol. This is a more
  /// structured version of SendDevToolsMessage. |message_id| is an incremental
  /// number that uniquely identifies the message (pass 0 to have the next
  /// number assigned automatically based on previous values). |method| is the
  /// method name. |params| are the method parameters, which may be empty. See
  /// the DevTools protocol documentation (linked above) for details of
  /// supported methods and the expected |params| dictionary contents. This
  /// method will return the assigned message ID if called on the UI thread and
  /// the message was successfully submitted for validation, otherwise 0. See
  /// the SendDevToolsMessage documentation for additional usage information.
  ///
  /*--cef(optional_param=params)--*/
  virtual int ExecuteDevToolsMethod(int message_id,
                                    const CefString& method,
                                    CefRefPtr<CefDictionaryValue> params) = 0;

  ///
  /// Add an observer for DevTools protocol messages (method results and
  /// events). The observer will remain registered until the returned
  /// Registration object is destroyed. See the SendDevToolsMessage
  /// documentation for additional usage information.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefRegistration> AddDevToolsMessageObserver(
      CefRefPtr<CefDevToolsMessageObserver> observer) = 0;

  ///
  /// Retrieve a snapshot of current navigation entries as values sent to the
  /// specified visitor. If |current_only| is true only the current navigation
  /// entry will be sent, otherwise all navigation entries will be sent.
  ///
  /*--cef()--*/
  virtual void GetNavigationEntries(
      CefRefPtr<CefNavigationEntryVisitor> visitor,
      bool current_only) = 0;

  ///
  /// If a misspelled word is currently selected in an editable node calling
  /// this method will replace it with the specified |word|.
  ///
  /*--cef()--*/
  virtual void ReplaceMisspelling(const CefString& word) = 0;

  ///
  /// Add the specified |word| to the spelling dictionary.
  ///
  /*--cef()--*/
  virtual void AddWordToDictionary(const CefString& word) = 0;

  ///
  /// Returns true if window rendering is disabled.
  ///
  /*--cef()--*/
  virtual bool IsWindowRenderingDisabled() = 0;

  ///
  /// Notify the browser that the widget has been resized. The browser will
  /// first call CefRenderHandler::GetViewRect to get the new size and then call
  /// CefRenderHandler::OnPaint asynchronously with the updated regions. This
  /// method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void WasResized() = 0;

  ///
  /// Notify the browser that it has been hidden or shown. Layouting and
  /// CefRenderHandler::OnPaint notification will stop when the browser is
  /// hidden. This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void WasHidden(bool hidden) = 0;

  ///
  /// Notify the browser that it has been occluded or unoccluded. Layouting and
  /// CefRenderHandler::OnPaint notification will stop when the browser is
  /// occluded. This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void WasOccluded(bool occluded) = 0;

  ///
  /// Running and do something when the window show
  ///
  /*--cef()--*/
  virtual void OnWindowShow() = 0;

  ///
  /// Running and do something when the window hide
  ///
  /*--cef()--*/
  virtual void OnWindowHide() = 0;

  ///
  /// Running and do something when the render visible
  ///
  /*--cef()--*/
  virtual void OnOnlineRenderToForeground() = 0;

  ///
  /// Send touch event list to the browser for a windowless browser.
  ///
  /*--cef()--*/
  virtual void SendTouchEventList(const std::vector<CefTouchEvent>& event_list) = 0;

  ///
  /// Send a notification to the browser that the screen info has changed. The
  /// browser will then call CefRenderHandler::GetScreenInfo to update the
  /// screen information with the new values. This simulates moving the webview
  /// window from one display to another, or changing the properties of the
  /// current display. This method is only used when window rendering is
  /// disabled.
  ///
  /*--cef()--*/
  virtual void NotifyScreenInfoChanged() = 0;

  ///
  /// Invalidate the view. The browser will call CefRenderHandler::OnPaint
  /// asynchronously. This method is only used when window rendering is
  /// disabled.
  ///
  /*--cef()--*/
  virtual void Invalidate(PaintElementType type) = 0;

  ///
  /// Issue a BeginFrame request to Chromium.  Only valid when
  /// CefWindowInfo::external_begin_frame_enabled is set to true.
  ///
  /*--cef()--*/
  virtual void SendExternalBeginFrame() = 0;

  ///
  /// Send a key event to the browser.
  ///
  /*--cef()--*/
  virtual void SendKeyEvent(const CefKeyEvent& event) = 0;

  ///
  /// Send a mouse click event to the browser. The |x| and |y| coordinates are
  /// relative to the upper-left corner of the view.
  ///
  /*--cef()--*/
  virtual void SendMouseClickEvent(const CefMouseEvent& event,
                                   MouseButtonType type,
                                   bool mouseUp,
                                   int clickCount) = 0;

  ///
  /// Send a mouse move event to the browser. The |x| and |y| coordinates are
  /// relative to the upper-left corner of the view.
  ///
  /*--cef()--*/
  virtual void SendMouseMoveEvent(const CefMouseEvent& event,
                                  bool mouseLeave) = 0;

  ///
  /// Send a mouse wheel event to the browser. The |x| and |y| coordinates are
  /// relative to the upper-left corner of the view. The |deltaX| and |deltaY|
  /// values represent the movement delta in the X and Y directions
  /// respectively. In order to scroll inside select popups with window
  /// rendering disabled CefRenderHandler::GetScreenPoint should be implemented
  /// properly.
  ///
  /*--cef()--*/
  virtual void SendMouseWheelEvent(const CefMouseEvent& event,
                                   int deltaX,
                                   int deltaY) = 0;

  ///
  /// Send a touch event to the browser for a windowless browser.
  ///
  /*--cef()--*/
  virtual void SendTouchEvent(const CefTouchEvent& event) = 0;

  ///
  /// Send a capture lost event to the browser.
  ///
  /*--cef()--*/
  virtual void SendCaptureLostEvent() = 0;

  ///
  /// Notify the browser that the window hosting it is about to be moved or
  /// resized. This method is only used on Windows and Linux.
  ///
  /*--cef()--*/
  virtual void NotifyMoveOrResizeStarted() = 0;

  ///
  /// Returns the maximum rate in frames per second (fps) that
  /// CefRenderHandler::OnPaint will be called for a windowless browser. The
  /// actual fps may be lower if the browser cannot generate frames at the
  /// requested rate. The minimum value is 1 and the maximum value is 60
  /// (default 30). This method can only be called on the UI thread.
  ///
  /*--cef()--*/
  virtual int GetWindowlessFrameRate() = 0;

  ///
  /// Set the maximum rate in frames per second (fps) that CefRenderHandler::
  /// OnPaint will be called for a windowless browser. The actual fps may be
  /// lower if the browser cannot generate frames at the requested rate. The
  /// minimum value is 1 and the maximum value is 60 (default 30). Can also be
  /// set at browser creation via CefBrowserSettings.windowless_frame_rate.
  ///
  /*--cef()--*/
  virtual void SetWindowlessFrameRate(int frame_rate) = 0;

  ///
  /// Begins a new composition or updates the existing composition. Blink has a
  /// special node (a composition node) that allows the input method to change
  /// text without affecting other DOM nodes. |text| is the optional text that
  /// will be inserted into the composition node. |underlines| is an optional
  /// set of ranges that will be underlined in the resulting text.
  /// |replacement_range| is an optional range of the existing text that will be
  /// replaced. |selection_range| is an optional range of the resulting text
  /// that will be selected after insertion or replacement. The
  /// |replacement_range| value is only used on OS X.
  ///
  /// This method may be called multiple times as the composition changes. When
  /// the client is done making changes the composition should either be
  /// canceled or completed. To cancel the composition call
  /// ImeCancelComposition. To complete the composition call either
  /// ImeCommitText or ImeFinishComposingText. Completion is usually signaled
  /// when:
  ///
  /// 1. The client receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR
  ///    flag (on Windows), or;
  /// 2. The client receives a "commit" signal of GtkIMContext (on Linux), or;
  /// 3. insertText of NSTextInput is called (on Mac).
  ///
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef(optional_param=text, optional_param=underlines)--*/
  virtual void ImeSetComposition(
      const CefString& text,
      const std::vector<CefCompositionUnderline>& underlines,
      const CefRange& replacement_range,
      const CefRange& selection_range) = 0;

  ///
  /// Completes the existing composition by optionally inserting the specified
  /// |text| into the composition node. |replacement_range| is an optional range
  /// of the existing text that will be replaced. |relative_cursor_pos| is where
  /// the cursor will be positioned relative to the current cursor position. See
  /// comments on ImeSetComposition for usage. The |replacement_range| and
  /// |relative_cursor_pos| values are only used on OS X.
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef(optional_param=text)--*/
  virtual void ImeCommitText(const CefString& text,
                             const CefRange& replacement_range,
                             int relative_cursor_pos) = 0;

  ///
  /// Completes the existing composition by applying the current composition
  /// node contents. If |keep_selection| is false the current selection, if any,
  /// will be discarded. See comments on ImeSetComposition for usage. This
  /// method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void ImeFinishComposingText(bool keep_selection) = 0;

  ///
  /// Cancels the existing composition and discards the composition node
  /// contents without applying them. See comments on ImeSetComposition for
  /// usage.
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void ImeCancelComposition() = 0;

  ///
  /// Call this method when the user drags the mouse into the web view (before
  /// calling DragTargetDragOver/DragTargetLeave/DragTargetDrop).
  /// |drag_data| should not contain file contents as this type of data is not
  /// allowed to be dragged into the web view. File contents can be removed
  /// using CefDragData::ResetFileContents (for example, if |drag_data| comes
  /// from CefRenderHandler::StartDragging). This method is only used when
  /// window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                                   const CefMouseEvent& event,
                                   DragOperationsMask allowed_ops) = 0;

  ///
  /// Call this method each time the mouse is moved across the web view during
  /// a drag operation (after calling DragTargetDragEnter and before calling
  /// DragTargetDragLeave/DragTargetDrop).
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragTargetDragOver(const CefMouseEvent& event,
                                  DragOperationsMask allowed_ops) = 0;

  ///
  /// Call this method when the user drags the mouse out of the web view (after
  /// calling DragTargetDragEnter).
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragTargetDragLeave() = 0;

  ///
  /// Call this method when the user completes the drag operation by dropping
  /// the object onto the web view (after calling DragTargetDragEnter).
  /// The object being dropped is |drag_data|, given as an argument to
  /// the previous DragTargetDragEnter call.
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragTargetDrop(const CefMouseEvent& event) = 0;

  ///
  /// Call this method when the drag operation started by a
  /// CefRenderHandler::StartDragging call has ended either in a drop or
  /// by being cancelled. |x| and |y| are mouse coordinates relative to the
  /// upper-left corner of the view. If the web view is both the drag source
  /// and the drag target then all DragTarget* methods should be called before
  /// DragSource* mthods.
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragSourceEndedAt(int x, int y, DragOperationsMask op) = 0;

  ///
  /// Call this method when the drag operation started by a
  /// CefRenderHandler::StartDragging call has completed. This method may be
  /// called immediately without first calling DragSourceEndedAt to cancel a
  /// drag operation. If the web view is both the drag source and the drag
  /// target then all DragTarget* methods should be called before DragSource*
  /// mthods.
  /// This method is only used when window rendering is disabled.
  ///
  /*--cef()--*/
  virtual void DragSourceSystemDragEnded() = 0;

  ///
  /// Returns the current visible navigation entry for this browser. This method
  /// can only be called on the UI thread.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefNavigationEntry> GetVisibleNavigationEntry() = 0;

  ///
  /// Set accessibility state for all frames. |accessibility_state| may be
  /// default, enabled or disabled. If |accessibility_state| is STATE_DEFAULT
  /// then accessibility will be disabled by default and the state may be
  /// further controlled with the "force-renderer-accessibility" and
  /// "disable-renderer-accessibility" command-line switches. If
  /// |accessibility_state| is STATE_ENABLED then accessibility will be enabled.
  /// If |accessibility_state| is STATE_DISABLED then accessibility will be
  /// completely disabled.
  ///
  /// For windowed browsers accessibility will be enabled in Complete mode
  /// (which corresponds to kAccessibilityModeComplete in Chromium). In this
  /// mode all platform accessibility objects will be created and managed by
  /// Chromium's internal implementation. The client needs only to detect the
  /// screen reader and call this method appropriately. For example, on macOS
  /// the client can handle the @"AXEnhancedUserInterface" accessibility
  /// attribute to detect VoiceOver state changes and on Windows the client can
  /// handle WM_GETOBJECT with OBJID_CLIENT to detect accessibility readers.
  ///
  /// For windowless browsers accessibility will be enabled in TreeOnly mode
  /// (which corresponds to kAccessibilityModeWebContentsOnly in Chromium). In
  /// this mode renderer accessibility is enabled, the full tree is computed,
  /// and events are passed to CefAccessibiltyHandler, but platform
  /// accessibility objects are not created. The client may implement platform
  /// accessibility objects using CefAccessibiltyHandler callbacks if desired.
  ///
  /*--cef()--*/
  virtual void SetAccessibilityState(cef_state_t accessibility_state) = 0;

  ///
  /// Enable notifications of auto resize via CefDisplayHandler::OnAutoResize.
  /// Notifications are disabled by default. |min_size| and |max_size| define
  /// the range of allowed sizes.
  ///
  /*--cef()--*/
  virtual void SetAutoResizeEnabled(bool enabled,
                                    const CefSize& min_size,
                                    const CefSize& max_size) = 0;

  ///
  /// Returns the extension hosted in this browser or NULL if no extension is
  /// hosted. See CefRequestContext::LoadExtension for details.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefExtension> GetExtension() = 0;

  ///
  /// Returns true if this browser is hosting an extension background script.
  /// Background hosts do not have a window and are not displayable. See
  /// CefRequestContext::LoadExtension for details.
  ///
  /*--cef()--*/
  virtual bool IsBackgroundHost() = 0;

  ///
  /// Set whether the browser's audio is muted.
  ///
  /*--cef()--*/
  virtual void SetAudioMuted(bool mute) = 0;

  ///
  /// Returns true if the browser's audio is muted.  This method can only be
  /// called on the UI thread.
  ///
  /*--cef()--*/
  virtual bool IsAudioMuted() = 0;

#if BUILDFLAG(IS_OHOS)
  ///
  /// GetRootBrowserAccessibilityManager
  ///
  /*--cef()--*/
  virtual void GetRootBrowserAccessibilityManager(void** manager) = 0;

  ///
  /// Execute a string of JavaScript code, return result by callback
  ///
  /*--cef()--*/
  virtual void ExecuteJavaScript(
      const std::string& code,
      CefRefPtr<CefJavaScriptResultCallback> callback,
      bool extention) = 0;
  ///
  /// Execute a string of JavaScript code, return result by callback
  ///
  /*--cef()--*/
  virtual void ExecuteJavaScriptExt(
      const int fd,
      const uint64 scriptLength,
      CefRefPtr<CefJavaScriptResultCallback> callback,
      bool extention) = 0;

  ///
  /// Set native window from ohos rs
  ///
  /*--cef()--*/
  virtual void SetNativeWindow(cef_native_window_t window) = 0;

  ///
  /// Set web debugging access
  ///
  /*--cef()--*/
  virtual void SetWebDebuggingAccess(bool isEnableDebug) = 0;

  ///
  /// Get web debugging access
  ///
  /*--cef()--*/
  virtual bool GetWebDebuggingAccess() = 0;

  ///
  /// GetImageForContextNode
  ///
  /*--cef()--*/
  virtual void GetImageForContextNode(int command_id) = 0;

  ///
  /// GetImageFromCache
  ///
  /*--cef()--*/
  virtual void GetImageFromCache(const CefString& url, int command_id) = 0;


  ///
  /// GetImageFromCacheEx
  ///
  /*--cef()--*/
  virtual void GetImageFromCacheEx(const CefString& url) = 0;

  ///
  /// ExitFullScreen
  ///
  /*--cef()--*/
  virtual void ExitFullScreen() = 0;

  ///
  /// SetFocusOnWeb
  ///
  /*--cef()--*/
  virtual void SetFocusOnWeb() = 0;

  ///
  /// UpdateLocale
  ///
  /*--cef()--*/
  virtual void UpdateLocale(const CefString& locale) = 0;

  ///
  /// Returns the original url of the request.
  ///
  /*--cef()--*/
  virtual CefString GetOriginalUrl() = 0;

  ///
  /// Set network status
  ///
  /*--cef()--*/
  virtual void PutNetworkAvailable(bool available) = 0;

  ///
  /// Remove web cache
  ///
  /*--cef()--*/
  virtual void RemoveCache(bool include_disk_files) = 0;

  ///
  /// Post task to ui thread.
  ///
  /*--cef()--*/
  virtual void PostTaskToUIThread(CefRefPtr<CefTask> task) = 0;

  ///
  /// Set the virtual pixel ratio
  ///
  /*--cef()--*/
  virtual void SetVirtualPixelRatio(float ratio) = 0;

  ///
  /// Get the virtual pixel ratio
  ///
  /*--cef()--*/
  virtual float GetVirtualPixelRatio() = 0;

  ///
  /// Recompute the WebPreferences based on the current state of the
  /// CefSettings, we will also call SetWebPreferences and send the updated
  /// WebPreferences to all RenderViews by WebContents.
  ///
  /*--cef()--*/
  virtual void SetWebPreferences(
      const CefBrowserSettings& browser_settings) = 0;

  ///
  /// PutUserAgent
  ///
  /*--cef()--*/
  virtual void PutUserAgent(const CefString& ua) = 0;

  ///
  /// DefaultUserAgent
  ///
  /*--cef()--*/
  virtual CefString DefaultUserAgent() = 0;

  ///
  /// SetBackgroundColor
  ///
  /*--cef()--*/
  virtual void SetBackgroundColor(int color) = 0;

  ///
  /// UpdateEasyListRules
  ///
  /*--cef()--*/
  virtual void UpdateAdblockEasyListRules(long adBlockEasyListVersion) = 0;

  ///
  /// RegisterArkJSfunction
  ///
  /*--cef()--*/
  virtual void RegisterArkJSfunction(const CefString& object_name,
                                     const std::vector<CefString>& method_list,
                                     const std::vector<CefString>& async_method_list,
                                     const int32_t object_id,
                                     const CefString& permission) = 0;

  ///
  /// UnregisterArkJSfunction
  ///
  /*--cef(optional_param=method_list)--*/
  virtual void UnregisterArkJSfunction(
      const CefString& object_name,
      const std::vector<CefString>& method_list) = 0;

  ///
  /// CallH5Function
  ///
  /*--cef()--*/
  virtual void CallH5Function(int32_t routing_id,
                              int32_t h5_object_id,
                              const CefString& h5_method_name,
                              const std::vector<CefRefPtr<CefValue>>& args) = 0;

  ///
  /// Saves the current view as a web archive.
  ///
  /*--cef()--*/
  virtual void StoreWebArchive(
      const CefString& base_name,
      bool auto_name,
      CefRefPtr<CefStoreWebArchiveResultCallback> callback) = 0;

  ///
  /// Notify the browser that the widget has been resized because of virtual
  /// keyboard.
  ///
  /*--cef()--*/
  virtual void WasKeyboardResized() = 0;

  ///
  /// Set if lower the frame rate.
  ///
  /*--cef()--*/
  virtual void SetEnableLowerFrameRate(bool enabled) = 0;
  /* ---------- ohos webview add end --------- */

  ///
  /// Gets the title for the current page.
  ///
  /*--cef()--*/
  virtual CefString Title() = 0;

  ///
  /// Create a message channel, which include two message ports.
  ///
  /*--cef()--*/
  virtual void CreateWebMessagePorts(std::vector<CefString>& ports) = 0;

  ///
  /// Posts a MessageEvent to the main frame.
  ///
  /*--cef()--*/
  virtual void PostWebMessage(CefString& message,
                              std::vector<CefString>& ports,
                              CefString& targetUri) = 0;

  ///
  /// Close the web message port.
  ///
  /*--cef()--*/
  virtual void ClosePort(CefString& port_handle) = 0;

  ///
  /// Destroy all web message ports.
  ///
  /*--cef()--*/
  virtual void DestroyAllWebMessagePorts() = 0;

  ///
  /// Post a message to the port.
  ///
  /*--cef()--*/
  virtual void PostPortMessage(const CefString& port_handle,
                               CefRefPtr<CefValue> message) = 0;

  ///
  /// Set the callback of the port.
  ///
  /*--cef()--*/
  virtual void SetPortMessageCallback(
      const CefString& port_handle,
      CefRefPtr<CefWebMessageReceiver> callback) = 0;

  ///
  /// Gets the latest hitdata
  ///
  /*--cef()--*/
  virtual void GetHitData(int& type, CefString& extra_data) = 0;

  ///
  /// Set the inital page scale
  ///
  /*--cef()--*/
  virtual void SetInitialScale(float scale) = 0;

  ///
  /// Gets the progress for the current page.
  ///
  /*--cef()--*/
  virtual int PageLoadProgress() = 0;

  ///
  /// Gets the progress for the current page.
  ///
  /*--cef()--*/
  virtual float Scale() = 0;

  ///
  /// Loads the given data into this WebView, using baseUrl as the base URL for
  /// the content. The base URL is used both to resolve relative URLs and when
  /// applying JavaScript's same origin policy. The historyUrl is used for the
  /// history entry.
  /// optional_param=baseUrl, optional_param=data, optional_param=mimeType,
  /// optional_param=encoding, optional_param=historyUrl
  ///
  /*--cef(optional_param=baseUrl, optional_param=data, optional_param=mimeType,
          optional_param=encoding, optional_param=historyUrl)--*/
  virtual void LoadWithDataAndBaseUrl(const CefString& baseUrl,
                                      const CefString& data,
                                      const CefString& mimeType,
                                      const CefString& encoding,
                                      const CefString& historyUrl) = 0;

  ///
  /// Loads the given data into this WebView
  /// optional_param=data, optional_param=mimeType, optional_param=encoding,
  ///
  /*--cef(optional_param=data, optional_param=mimeType,
          optional_param=encoding)--*/
  virtual void LoadWithData(const CefString& data,
                            const CefString& mimeType,
                            const CefString& encoding) = 0;

  ///
  /// add visited url.
  ///
  /*--cef(optional_param=urls)--*/
  virtual void AddVisitedLinks(const std::vector<CefString>& urls) {}

  ///
  /// Resume download after interrupted.
  ///
  /*--cef(optional_param=etag,optional_param=last_modified,optional_param=received_slices_string)--*/
  virtual void ResumeDownload(const CefString& url,
                              const CefString& full_path,
                              int64 received_bytes,
                              int64 total_bytes,
                              const CefString& etag,
                              const CefString& mime_type,
                              const CefString& last_modified,
                              const CefString& received_slices_string) = 0;
  ///
  ///  Set the audio resume interval of the broswer.
  ///
  /*--cef()--*/
  virtual void SetAudioResumeInterval(int resumeInterval) = 0;

  ///
  ///  Set whether the browser's audio is exclusive.
  ///
  /*--cef()--*/
  virtual void SetAudioExclusive(bool audioExclusive) = 0;

  ///
  /// Close fullScreen video.
  ///
  /*--cef()--*/
  virtual void CloseMedia() = 0;

  ///
  /// Stop all audio and video playback on the web page.
  ///
  /*--cef()--*/
  virtual void StopMedia() = 0;

  ///
  /// Restart playback of all audio and video on the web page.
  ///
  /*--cef()--*/
  virtual void ResumeMedia() = 0;

  ///
  /// Pause all audio and video playback on the web page.
  ///
  /*--cef()--*/
  virtual void PauseMedia() = 0;

  ///
  /// View the playback status of all audio and video on the web page.
  ///
  /*--cef()--*/
  virtual int GetMediaPlaybackState() = 0;

  ///
  /// Scroll page up or down
  ///
  /*--cef()--*/
  virtual void ScrollPageUpDown(bool is_up,
                                bool is_half,
                                float view_height) = 0;

  ///
  /// Get web history state
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefBinaryValue> GetWebState() = 0;

  ///
  /// Restore web history state
  ///
  /*--cef()--*/
  virtual bool RestoreWebState(const CefRefPtr<CefBinaryValue> state) = 0;

  ///
  /// Scroll to the position.
  ///
  /*--cef()--*/
  virtual void ScrollTo(float x, float y) = 0;

  ///
  /// Scroll by the delta distance.
  ///
  /*--cef()--*/
  virtual void ScrollBy(float delta_x, float delta_y) = 0;

  ///
  /// Slide Scroll by the speed.
  ///
  /*--cef()--*/
  virtual void SlideScroll(float vx, float vy) = 0;

  ///
  /// Set whether webview can access files
  ///
  /*--cef()--*/
  virtual void SetFileAccess(bool falg) = 0;

  ///
  /// Set whether webview can access network
  ///
  /*--cef()--*/
  virtual void SetBlockNetwork(bool falg) = 0;

  ///
  /// Set the cache mode of webview
  ///
  /*--cef()--*/
  virtual void SetCacheMode(int falg) = 0;

  ///
  /// Set should frame submission before draw
  ///
  /*--cef()--*/
  virtual void SetShouldFrameSubmissionBeforeDraw(bool should) = 0;

  ///
  /// Set zoom with the dela facetor
  ///
  /*--cef()--*/
  virtual void ZoomBy(float delta, float width, float height) = 0;

  ///
  /// Set the window id of the UI framework
  ///
  /*--cef()--*/
  virtual void SetWindowId(int window_id, int nweb_id) = 0;

  ///
  /// Set the token of the UI framework
  ///
  /*--cef()--*/
  virtual void SetToken(void* token) = 0;

  ///
  /// Set the property values for width, height, and keyboard height
  ///
  /*--cef()--*/
  virtual void SetVirtualKeyBoardArg(int32_t width,
                                     int32_t height,
                                     double keyboard) = 0;

  ///
  /// Set the virtual keyboard to override the web status
  ///
  /*--cef()--*/
  virtual bool ShouldVirtualKeyboardOverlay() = 0;

  ///
  /// JavaScriptOnDocumentStart
  ///
  /*--cef()--*/
  virtual void JavaScriptOnDocumentStart(
      const CefString& script,
      const std::vector<CefString>& script_rules) = 0;

  ///
  /// RemoveJavaScriptOnDocumentStart
  ///
  /*--cef()--*/
  virtual void RemoveJavaScriptOnDocumentStart() = 0;

  ///
  /// JavaScriptOnDocumentEnd
  ///
  /*--cef()--*/
  virtual void JavaScriptOnDocumentEnd(
      const CefString& script,
      const std::vector<CefString>& script_rules) = 0;

  ///
  /// RemoveJavaScriptOnDocumentEnd
  ///
  /*--cef()--*/
  virtual void RemoveJavaScriptOnDocumentEnd() = 0;

  ///
  /// Set the draw rect
  ///
  /*--cef()--*/
  virtual void SetDrawRect(int x, int y, int width, int height) = 0;

  ///
  /// Set the draw mode
  ///
  /*--cef()--*/
  virtual void SetDrawMode(int mode) = 0;

  ///
  /// Create the Web print document adapter of the UI framework
  ///
  /*--cef()--*/
  virtual void CreateWebPrintDocumentAdapter(
      const CefString& jobName,
      void** webPrintDocumentAdapter) = 0;

  ///
  /// Set the over-scroll mode of web
  ///
  /*--cef()--*/
  virtual void SetOverscrollMode(int mode) = 0;


  ///
  /// Change the zoom factor for browser zoom.
  /// If called on the UI thread the change will be applied immediately.
  /// Otherwise, the change will be applied asynchronously on the UI thread.
  ///
  /*--cef()--*/
  virtual void SetBrowserZoomLevel(double zoomFactor) = 0;

  ///
  /// Discard a webview window
  ///
  /*--cef()--*/
  virtual bool Discard() = 0;

  ///
  /// Restore the discarded webview window
  ///
  /*--cef()--*/
  virtual bool Restore() = 0;

  ///
  /// Get the top controls offset.
  ///
  /*--cef()--*/
  virtual int GetTopControlsOffset() = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual int GetShrinkViewportHeight() = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual int InsertBackForwardEntry(int index, const CefString& url) = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual int UpdateNavigationEntryUrl(int index, const CefString& url) = 0;

  ///
  /// Get the shrink viewport height.
  ///
  /*--cef()--*/
  virtual void ClearForwardList() = 0;

  ///
  /// Set background print enable.
  ///
  /*--cef()--*/
  virtual void SetPrintBackground(bool enable) = 0;

  ///
  /// Get whether print background.
  ///
  /*--cef()--*/
  virtual bool GetPrintBackground() = 0;

  ///
  /// set Scrollable
  ///
  /*--cef()--*/
  virtual void SetScrollable(bool enable, int scrollType) = 0;

  ///
  /// Get the last javascript proxy calling frame url.
  ///
  /*--cef()--*/
  virtual CefString GetLastJavascriptProxyCallingFrameUrl() = 0;

  ///
  /// Start current camera.
  ///
  /*--cef()--*/
  virtual void StartCamera() = 0;

  ///
  ///  Stop current camera.
  ///
  /*--cef()--*/
  virtual void StopCamera() = 0;

  ///
  ///  Close current camera.
  ///
  /*--cef()--*/
  virtual void CloseCamera() = 0;

  ///
  ///  Set NWebID.
  ///
  /*--cef()--*/
  virtual void SetNWebId(int nWebId) = 0;

  ///
  /// get pendingSizeStatus.
  ///
  /*--cef()--*/
  virtual bool GetPendingSizeStatus() = 0;

  ///
  /// Get CefDownloadItem by download_item_id.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDownloadItem> GetDownloadItem(uint32 item_id) = 0;

  ///
  /// SetWakeLockHandler.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void SetWakeLockHandler(int32_t windowId, CefRefPtr<CefSetLockCallback> callback) = 0;

  ///
  /// Notify browser host needs reoload when the render process terminated.
  ///
  /*--cef()--*/
  virtual void SetNeedsReload(bool needs_reload) = 0;

  ///
  /// Return true if needs reload page, or false if needs not reload.
  ///
  /*--cef()--*/
  virtual bool NeedsReload() = 0;

  ///
  ///  precompile javascript and generate code cache.
  ///
  /*--cef()--*/
  virtual void PrecompileJavaScript(
      const std::string& url,
      const std::string& script,
      CefRefPtr<CefCacheOptions> cacheOptions,
      CefRefPtr<CefPrecompileCallback> callback) = 0;

  ///
  ///  update DrawRect.
  ///
  /*--cef()--*/
  virtual void UpdateDrawRect() = 0;

  ///
  /// SendTouchpadFlingEvent
  ///
  /*--cef()--*/
  virtual void SendTouchpadFlingEvent(const CefMouseEvent& event, double vx, double vy) = 0;

  ///
  /// Set the fit content mode
  ///
  /*--cef()--*/
  virtual void SetFitContentMode(int mode) = 0;

  ///
  /// Terminate render process
  ///
  /*--cef()--*/
  virtual bool TerminateRenderProcess() = 0;

  ///
  /// RegisterNativeJSProxy
  ///
  /*--cef()--*/
  virtual void RegisterNativeJSProxy(const CefString& object_name,
                                     const std::vector<CefString>& method_list,
                                     const int32_t object_id,
                                     bool is_async,
                                     const CefString& permission) = 0;

  ///
  /// Called when text is selected.
  ///
  /*--cef()--*/
  virtual void OnTextSelected(bool flag) = 0;

  ///
  /// Get page scale factor.
  ///
  /*--cef()--*/
  virtual float GetPageScaleFactor() = 0;

  ///
  /// WebPageSnapshot, return result by callback
  ///
  /*--cef()--*/
  virtual bool WebPageSnapshot(const char* id,
                               int width,
                               int height,
                               cef_web_snapshot_callback_t callback) = 0;

  ///
  /// Advance focus for IME to the browser.
  ///
  /*--cef()--*/
  virtual void AdvanceFocusForIME(int focusType) = 0;

  ///
  /// Called when image analyzer overlay is destroyed.
  ///
  /*--cef()--*/
  virtual void OnDestroyImageAnalyzerOverlay() = 0;

  ///
  /// Called when the folding status of the phone screen changes.
  ///
  /*--cef()--*/
  virtual void OnFoldStatusChanged(uint32_t foldStatus) = 0;

  ///
  /// SetNativeEmbedMode.
  ///
  /*--cef()--*/
  virtual void SetNativeEmbedMode(bool flag) = 0;

#if defined(OHOS_GET_SCROLL_OFFSET)
  ///
  /// Get the current scroll offset of the webpage.
  ///
  /*--cef()--*/
  virtual void GetScrollOffset(float* offset_x, float* offset_y) = 0;

  ///
  /// Get the current overscroll offset of the webpage.
  ///
  /*--cef()--*/
  virtual void GetOverScrollOffset(float* offset_x, float* offset_y) = 0;
#endif

  ///
  /// Scroll to the position with anime.
  ///
  /*--cef()--*/
  virtual void ScrollToWithAnime(float x, float y, int32_t duration) = 0;

  ///
  /// Scroll by the delta with anime.
  ///
  /*--cef()--*/
  virtual void ScrollByWithAnime(float delta_x, float delta_y, int32_t duration) = 0;
#endif  // BUILDFLAG(IS_OHOS)

  ///
  /// OnSafeInsetsChange
  ///
  /*--cef()--*/
  virtual void OnSafeInsetsChange(int left, int top, int right, int bottom) = 0;

  ///
  /// Notify for next touch move event.
  ///
  /*--cef()--*/
  virtual void NotifyForNextTouchEvent() = 0;

  ///
  /// Set grant file access dirs.
  ///
  /*--cef()--*/
  virtual void SetGrantFileAccessDirs(const std::vector<CefString>& dir_list) = 0;

#if defined(OHOS_ARKWEB_EXTENSIONS)
  ///
  /// Receiving the tab updated notification.
  ///
  /*--cef()--*/
  virtual void WebExtensionTabUpdated(
      int tab_id,
      const std::vector<CefString>& changed_property_names,
      const CefString& url) = 0;
#endif

  ///
  /// ScrollFocusedEditableNodeIntoView.
  ///
  /*--cef()--*/
  virtual void ScrollFocusedEditableNodeIntoView() = 0;

  ///
  /// Set the callback of the autofill event.
  ///
  /*--cef()--*/
  virtual void SetAutofillCallback(CefRefPtr<CefWebMessageReceiver> callback) = 0;

  ///
  /// Fill autofill data.
  ///
  /*--cef()--*/
  virtual void FillAutofillData(CefRefPtr<CefValue> message) = 0;

  ///
  /// Process autofill cancel content.
  ///
  /*--cef()--*/
  virtual void ProcessAutofillCancel(const std::string& fillContent) = 0;

  ///
  /// request autofill from IMF event.
  ///
  /*--cef()--*/
  virtual void AutoFillWithIMFEvent(bool is_username,
                                    bool is_other_account,
                                    bool is_new_password,
                                    const std::string& content) = 0;

  ///
  /// Create pdf data stream.
  ///
  /*--cef()--*/
  virtual void CreateToPDF(const CefPdfPrintSettings& settings,
                           CefRefPtr<CefPdfValueCallback> callback) = 0;

  ///
  /// Set zoom with the dela facetor
  ///
  /*--cef()--*/
  virtual void ScaleGestureChangeV2(int type, float scale, float originScale, float width, float height) = 0;

  ///
  /// set video assistant enable.
  ///
  /*--cef()--*/
  virtual void EnableVideoAssistant(bool enable) = 0;

  ///
  /// execute video assistant function.
  ///
  /*--cef()--*/
  virtual void ExecuteVideoAssistantFunction(const CefString& cmdId) = 0;

  ///
  ///  Close current screen capture.
  ///
  /*--cef()--*/
  virtual void StopScreenCapture(int32_t nweb_id, const CefString& session_id) = 0;

  ///
  ///  Register screen capture listener.
  ///
  /*--cef()--*/
  virtual void RegisterScreenCaptureDelegateListener(
      CefRefPtr<CefScreenCaptureCallback> listener) = 0;

#ifdef OHOS_EX_REFRESH_IFRAME
  ///
  /// Get whether it is the iframe.
  ///
  /*--cef()--*/
  virtual bool IsIframe() = 0;

  ///
  /// fresh focused frame for context menu.
  ///
  /*--cef()--*/
  virtual void ReloadFocusedFrame() = 0;
#endif

  ///
  /// Set whether use optimized HTML parser budget to reduce FCP time.
  ///
  /*--cef()--*/
  virtual void SetOptimizeParserBudgetEnabled(bool enable) = 0;

#if defined(OHOS_JSPROXY)
  ///
  /// JavaScriptOnHeadReady
  ///
  /*--cef()--*/
  virtual void JavaScriptOnHeadReady(
      const CefString& script,
      const std::vector<CefString>& script_rules) = 0;

  ///
  /// RemoveJavaScriptOnHeadReady
  ///
  /*--cef()--*/
  virtual void RemoveJavaScriptOnHeadReady() = 0;
#endif

};
#endif  // CEF_INCLUDE_CEF_BROWSER_H_
