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

#ifndef CEF_INCLUDE_CEF_RENDER_HANDLER_H_
#define CEF_INCLUDE_CEF_RENDER_HANDLER_H_
#pragma once

#include <vector>

#include "include/cef_accessibility_handler.h"
#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_drag_data.h"

///
/// Implement this interface to handle events when window rendering is disabled.
/// The methods of this class will be called on the UI thread.
///
/*--cef(source=client)--*/
class CefRenderHandler : public virtual CefBaseRefCounted {
 public:
  typedef cef_drag_operations_mask_t DragOperation;
  typedef cef_drag_operations_mask_t DragOperationsMask;
  typedef cef_paint_element_type_t PaintElementType;
  typedef std::vector<CefRect> RectList;
  typedef cef_text_input_mode_t TextInputMode;
  typedef cef_text_input_type_t TextInputType;
#ifdef BUILDFLAG(IS_OHOS)
  typedef cef_native_embed_data_t CefNativeEmbedData;
  typedef cef_embed_touch_event_t CefEmbedTouchEvent;
  typedef cef_embed_life_change_t CefEmbedLifeStatus;
  typedef cef_embed_touch_type_t CefEmbedTouchType;
  typedef std::map<CefString, CefString> AttributesMap;
  typedef cef_text_input_action_t TextInputAction;
  typedef cef_text_input_flags_t TextInputFlags;
  typedef cef_text_input_info_t TextInputInfo;
#endif

  ///
  /// Return the handler for accessibility notifications. If no handler is
  /// provided the default implementation will be used.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefAccessibilityHandler> GetAccessibilityHandler() {
    return nullptr;
  }

  ///
  /// Called to retrieve the root window rectangle in screen DIP coordinates.
  /// Return true if the rectangle was provided. If this method returns false
  /// the rectangle from GetViewRect will be used.
  ///
  /*--cef()--*/
  virtual bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    return false;
  }

  ///
  /// Called to retrieve the view rectangle in screen DIP coordinates. This
  /// method must always provide a non-empty rectangle.
  ///
  /*--cef()--*/
  virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) = 0;

  ///
  /// Called to retrieve the translation from view DIP coordinates to screen
  /// coordinates. Windows/Linux should provide screen device (pixel)
  /// coordinates and MacOS should provide screen DIP coordinates. Return true
  /// if the requested coordinates were provided.
  ///
  /*--cef()--*/
  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) {
    return false;
  }

  ///
  /// Called to allow the client to fill in the CefScreenInfo object with
  /// appropriate values. Return true if the |screen_info| structure has been
  /// modified.
  ///
  /// If the screen info rectangle is left empty the rectangle from GetViewRect
  /// will be used. If the rectangle is still empty or invalid popups may not be
  /// drawn correctly.
  ///
  /*--cef()--*/
  virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                             CefScreenInfo& screen_info) {
    return false;
  }

  ///
  /// Called when the browser wants to show or hide the popup widget. The popup
  /// should be shown if |show| is true and hidden if |show| is false.
  ///
  /*--cef()--*/
  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {}

  ///
  /// Called when the browser wants to move or resize the popup widget. |rect|
  /// contains the new location and size in view coordinates.
  ///
  /*--cef()--*/
  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) {
  }

  ///
  /// Called when an element should be painted. Pixel values passed to this
  /// method are scaled relative to view coordinates based on the value of
  /// CefScreenInfo.device_scale_factor returned from GetScreenInfo. |type|
  /// indicates whether the element is the view or the popup widget. |buffer|
  /// contains the pixel data for the whole image. |dirtyRects| contains the set
  /// of rectangles in pixel coordinates that need to be repainted. |buffer|
  /// will be |width|*|height|*4 bytes in size and represents a BGRA image with
  /// an upper-left origin. This method is only called when
  /// CefWindowInfo::shared_texture_enabled is set to false.
  ///
  /*--cef()--*/
  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width,
                       int height) = 0;

  ///
  /// Called when an element has been rendered to the shared texture handle.
  /// |type| indicates whether the element is the view or the popup widget.
  /// |dirtyRects| contains the set of rectangles in pixel coordinates that need
  /// to be repainted. |shared_handle| is the handle for a D3D11 Texture2D that
  /// can be accessed via ID3D11Device using the OpenSharedResource method. This
  /// method is only called when CefWindowInfo::shared_texture_enabled is set to
  /// true, and is currently only supported on Windows.
  ///
  /*--cef()--*/
  virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                  PaintElementType type,
                                  const RectList& dirtyRects,
                                  void* shared_handle) {}

  ///
  /// Called to retrieve the size of the touch handle for the specified
  /// |orientation|.
  ///
  /*--cef()--*/
  virtual void GetTouchHandleSize(CefRefPtr<CefBrowser> browser,
                                  cef_horizontal_alignment_t orientation,
                                  CefSize& size) {}

  ///
  /// Called when touch handle state is updated. The client is responsible for
  /// rendering the touch handles.
  ///
  /*--cef()--*/
  virtual void OnTouchHandleStateChanged(CefRefPtr<CefBrowser> browser,
                                         const CefTouchHandleState& state) {}

  ///
  /// Called when the user starts dragging content in the web view. Contextual
  /// information about the dragged content is supplied by |drag_data|.
  /// (|x|, |y|) is the drag start location in screen coordinates.
  /// OS APIs that run a system message loop may be used within the
  /// StartDragging call.
  ///
  /// Return false to abort the drag operation. Don't call any of
  /// CefBrowserHost::DragSource*Ended* methods after returning false.
  ///
  /// Return true to handle the drag operation. Call
  /// CefBrowserHost::DragSourceEndedAt and DragSourceSystemDragEnded either
  /// synchronously or asynchronously to inform the web view that the drag
  /// operation has ended.
  ///
  /*--cef()--*/
  virtual bool StartDragging(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefDragData> drag_data,
                             DragOperationsMask allowed_ops,
                             int x,
                             int y) {
    return false;
  }

  ///
  /// Called when the web view wants to update the mouse cursor during a
  /// drag & drop operation. |operation| describes the allowed operation
  /// (none, move, copy, link).
  ///
  /*--cef()--*/
  virtual void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                                DragOperation operation) {}

  ///
  /// Called when the scroll offset has changed.
  ///
  /*--cef()--*/
  virtual void OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser,
                                     double x,
                                     double y) {}

  ///
  /// Called when the IME composition range has changed. |selected_range| is the
  /// range of characters that have been selected. |character_bounds| is the
  /// bounds of each character in view coordinates.
  ///
  /*--cef()--*/
  virtual void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                            const CefRange& selected_range,
                                            const RectList& character_bounds) {}

  ///
  /// Called when text selection has changed for the specified |browser|.
  /// |selected_text| is the currently selected text and |selected_range| is
  /// the character range.
  ///
  /*--cef(optional_param=selected_text,optional_param=selected_range)--*/
  virtual void OnTextSelectionChanged(CefRefPtr<CefBrowser> browser,
                                      const CefString& selected_text,
                                      const CefRange& selected_range) {}

  ///
  /// Called when an on-screen keyboard should be shown or hidden for the
  /// specified |browser|. |input_mode| specifies what kind of keyboard
  /// should be opened. If |input_mode| is CEF_TEXT_INPUT_MODE_NONE, any
  /// existing keyboard for this browser should be hidden.|show_keyboard|
  /// specifies whether to display the keyboard.
  ///
  /*--cef()--*/
  virtual void OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser,
                                          TextInputInfo text_input_info,
                                          bool is_need_reset_listener,
                                          const AttributesMap& attributes) {}

  ///
  /// Called when touch selection is updated. The client is responsible for
  /// rendering the touch handles.
  ///
  /*--cef()--*/
  virtual void OnTouchSelectionChanged(
      const CefTouchHandleState& insert_handle,
      const CefTouchHandleState& start_selection_handle,
      const CefTouchHandleState& end_selection_handle,
      bool need_report) {}

  ///
  /// Called when the RootLayer has changed.
  ///
  /*--cef()--*/
  virtual void OnRootLayerChanged(CefRefPtr<CefBrowser> browser,
                                  int height,
                                  int width) {}

#if BUILDFLAG(IS_OHOS)
  ///
  /// Called when text selection has changed for the specified |browser|.
  /// |text| is the currently text and |selected_range| is
  /// the character range.
  ///
  /*--cef(optional_param=text,optional_param=selected_range)--*/
  virtual void OnSelectionChanged(CefRefPtr<CefBrowser> browser,
                                  const CefString& text,
                                  const CefRange& selected_range) {}

  ///
  /// Called when the cursor position has changed.
  ///
  /*--cef()--*/
  virtual void OnCursorUpdate(CefRefPtr<CefBrowser> browser,
                              const CefRect& rect) {}

  ///
  /// Called when swap buffer completed with new size.
  ///
  /*--cef()--*/
  virtual void OnCompleteSwapWithNewSize() {}

  ///
  /// Called when resize not work.
  ///
  /*--cef()--*/
  virtual void OnResizeNotWork() {}

  ///
  /// Called when over scroll.
  ///
  /*--cef()--*/
  virtual void OnOverscroll(CefRefPtr<CefBrowser> browser,
                            const float x,
                            const float y) {}
  ///
  /// Called when editable status has been changed.
  ///
  /*--cef()--*/
  virtual void OnEditableChanged(CefRefPtr<CefBrowser> browser,
                                 bool is_editable_node) {}

  ///
  /// Called when over scroll.
  ///
  /*--cef()--*/
  virtual void OnOverScrollFlingVelocity(CefRefPtr<CefBrowser> browser,
                                         const float x,
                                         const float y,
                                         bool is_fling) {}

  ///
  /// Called when over scroll fling end.
  ///
  /*--cef()--*/
  virtual void OnOverScrollFlingEnd(CefRefPtr<CefBrowser> browser) {}

  ///
  /// Called when scroll begin or end.
  ///
  /*--cef()--*/
  virtual void OnScrollState(CefRefPtr<CefBrowser> browser,
                             bool scroll_state) {}

  ///
  /// Called when scroll.
  ///
  /*--cef()--*/
  virtual bool FilterScrollEvent(CefRefPtr<CefBrowser> browser,
                                 const float x,
                                 const float y,
                                 const float fling_x,
                                 const float fling_y) {
    return false;
  }
  ///
  /// Called when embed touch.
  ///
  /*--cef()--*/
  virtual void OnNativeEmbedGestureEvent(CefRefPtr<CefBrowser> browser,
                                   const CefEmbedTouchEvent& event,
                                   CefRefPtr<CefGestureEventCallback> callback) {}
  ///
  /// Called when embed touch.
  ///
  /*--cef()--*/
  virtual void OnNativeEmbedLifecycleChange(CefRefPtr<CefBrowser> browser,
                                      const CefNativeEmbedData& info) {}
  
  ///
  /// Called when embed visibility.
  ///
  /*--cef()--*/
  virtual void OnNativeEmbedVisibilityChange(const std::string& embed_id,
                                      bool visibility) {}

  ///
  /// Called when select all is clicked.
  ///
  /*--cef()--*/
  virtual void NotifySelectAllClicked(bool select_all) {}

  ///
  /// Called when scroll begin or end.
  ///
  /*--cef()--*/
  virtual void ReleaseResizeHold(CefRefPtr<CefBrowser> browser) {}

  ///
  /// Called when text input state has changed for the specified |browser|.
  ///
  /*--cef()--*/
  virtual void OnUpdateTextInputStateCalled(CefRefPtr<CefBrowser> browser,
                                            const CefString& text,
                                            const CefRange& selected_range,
                                            const CefRange& compositon_range) {}

  ///
  /// Called when selecting word.
  ///
  /*--cef()--*/
  virtual void GetWordSelection(CefRefPtr<CefBrowser> browser,
                                const CefString& text,
                                int8_t offset,
                                CefPoint& select) {}

  ///
  /// Called when creating overlay.
  ///
  /*--cef()--*/
  virtual void CreateOverlay(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefImage> cef_image,
                             const CefRect& cef_image_rect,
                             const CefPoint& cef_touch_point) {}

  ///
  /// Called when overlay state is changed.
  ///
  /*--cef()--*/
  virtual void OnOverlayStateChanged(CefRefPtr<CefBrowser> browser,
                                     const CefRect& cef_image_rect) {}

  ///
  /// Called to retrieve the visible view rectangle in screen DIP coordinates. This
  /// method must always provide a non-empty rectangle.
  ///
  /*--cef()--*/
  virtual void GetVisibleViewportRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {}

  ///
  /// SendDynamicFrameLossEvent
  ///
  /*--cef()--*/
  virtual void SendDynamicFrameLossEvent(CefRefPtr<CefBrowser> browser,
                                         const CefString& sceneId,
                                         bool isStart) {}

  ///
  /// OnResizeScrollableViewport.
  ///
  /*--cef()--*/
  virtual void OnResizeScrollableViewport(CefRefPtr<CefBrowser> browser) {}

  ///
  /// SetFillContent
  ///
  /*--cef()--*/
  virtual void SetFillContent(const std::string& content) {}

  ///
  /// SetGestureEventResult
  ///
  /*--cef()--*/
  virtual void SetGestureEventResult(const bool result) {}

  ///
  /// Called when you need to start vibrator.
  ///
  /*--cef()--*/
  virtual void StartVibraFeedback(const std::string& vibratorType) {}

  ///
  /// Get Device pixel size.
  ///
  /*--cef()--*/
  virtual void GetDevicePixelSize(CefRefPtr<CefBrowser> browser, CefSize& size) {}

  ///
  /// Called when an accessibility event occurs.
  ///
  /*--cef()--*/
  virtual void OnAccessibilityEvent(int64_t accessibilityId, int32_t eventType) {}

  ///
  /// Get the visible area relative to the web.
  ///
  /*--cef()--*/
  virtual void GetVisibleRectToWeb(int& visibleX, int& visibleY, int& visibleWidth, int& visibleHeight) {}

  ///
  /// Called when scroll begin or end.
  ///
  /*--cef()--*/
  virtual void OnScrollStart(CefRefPtr<CefBrowser> browser,
                                 const float x,
                                 const float y) {}

  ///
  /// restore renderfit
  ///
  /*--cef()--*/
  virtual void RestoreRenderFit() {}

  ///
  /// Get Screen Offset
  ///
  /*--cef()--*/
  virtual void GetScreenOffset(CefRefPtr<CefBrowser> browser, double& x, double& y) {}
#endif  // BUILDFLAG(IS_OHOS)
};

#endif  // CEF_INCLUDE_CEF_RENDER_HANDLER_H_
