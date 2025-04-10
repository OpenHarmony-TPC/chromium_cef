
// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_

#include <string>
#include <vector>

#include "include/cef_client.h"
#include "include/cef_drag_data.h"
#include "include/internal/cef_types.h"
#include "include/views/cef_browser_view.h"

#include "base/functional/callback_forward.h"
#include "extensions/common/mojom/view_type.mojom-forward.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

#ifdef OHOS_HTML_SELECT
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#endif  // #endif OHOS_HTML_SELECT

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/gfx/geometry/rect_f.h"
#endif

class GURL;

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebInputEvent;
class WebTouchEvent;

namespace mojom {
class WindowFeatures;
}
}  // namespace blink

namespace content {
struct AXEventNotificationDetails;
struct AXLocationChangeNotificationDetails;
#if BUILDFLAG(IS_OHOS)
class BrowserAccessibilityManager;
#endif
struct DropData;
struct NativeWebKeyboardEvent;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class WebContents;
class WebContentsView;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionHost;
}  // namespace extensions

namespace gfx {
class ImageSkia;
class Point;
class Rect;
class Size;
class Vector2d;
}  // namespace gfx

namespace views {
class Widget;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

struct CefBrowserCreateParams;
class CefBrowserHostBase;
class CefJavaScriptDialogRunner;
class CefMenuRunner;

// Provides platform-specific implementations of browser functionality. All
// methods are called on the browser process UI thread unless otherwise
// indicated.
class CefBrowserPlatformDelegate {
 public:
  CefBrowserPlatformDelegate(const CefBrowserPlatformDelegate&) = delete;
  CefBrowserPlatformDelegate& operator=(const CefBrowserPlatformDelegate&) =
      delete;

  // Create a new CefBrowserPlatformDelegate instance. May be called on multiple
  // threads.
  static std::unique_ptr<CefBrowserPlatformDelegate> Create(
      const CefBrowserCreateParams& create_params);

  // Called from AlloyBrowserHostImpl::Create.
  // Wait for the call to WebContentsCreated(owned=true) before taking ownership
  // of the resulting WebContents object.
  virtual content::WebContents* CreateWebContents(
      CefBrowserCreateParams& create_params,
      bool& own_web_contents);

  // Called to create the view objects for a new WebContents. Will only be
  // called a single time per instance. May be called on multiple threads. Only
  // used with windowless rendering.
  virtual void CreateViewForWebContents(
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view);

  // Called after the WebContents for a browser has been created. |owned| will
  // be true if |web_contents| was created via CreateWebContents() and we should
  // take ownership. This will also be called for popup WebContents created
  // indirectly by Chromium. Will only be called a single time per instance.
  virtual void WebContentsCreated(content::WebContents* web_contents,
                                  bool owned);

  // Called when Chromium is ready to hand over ownership of a popup
  // WebContents. WebContentsCreated(owned=false) will be called first for
  // |new_contents|. Will only be called a single time per instance. See also
  // the WebContentsDelegate documentation.
  virtual void AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked);

  // Called when the WebContents is destroyed. This will be called before
  // BrowserDestroyed(). Will only be called a single time per instance.
  virtual void WebContentsDestroyed(content::WebContents* web_contents);

  // See WebContentsDelegate documentation.
  virtual bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_main_frame_navigation);

  // Called after the RenderViewHost is created.
  virtual void RenderViewCreated(content::RenderViewHost* render_view_host);

  // See WebContentsObserver documentation.
  virtual void RenderViewReady();

  // Called after the owning AlloyBrowserHostImpl is created. Will only be
  // called a single time per instance. Do not send any client notifications
  // from this method.
  virtual void BrowserCreated(CefBrowserHostBase* browser);

  // Called from AlloyBrowserHostImpl::Create.
  virtual void CreateExtensionHost(const extensions::Extension* extension,
                                   const GURL& url,
                                   extensions::mojom::ViewType host_type);

  // Returns the current extension host.
  virtual extensions::ExtensionHost* GetExtensionHost() const;

  // Send any notifications related to browser creation. Called after
  // BrowserCreated().
  virtual void NotifyBrowserCreated();

  // Send any notifications related to browser destruction. Called before
  // BrowserDestroyed().
  virtual void NotifyBrowserDestroyed();

  // Called before the owning AlloyBrowserHostImpl is destroyed. Will only be
  // called a single time per instance. All references to the
  // AlloyBrowserHostImpl and WebContents should be cleared when this method is
  // called. Do not send any client notifications from this method.
  virtual void BrowserDestroyed(CefBrowserHostBase* browser);

  // Create the window that hosts the browser. Will only be called a single time
  // per instance. Only used with windowed rendering.
  virtual bool CreateHostWindow();

  // Sends a message to close the window that hosts the browser. On native
  // platforms this will be done via the OS. DestroyBrowser will be called after
  // the native window has closed. Only used with windowed rendering.
  virtual void CloseHostWindow();

  // Return the OS handle for the window that hosts the browser. For windowed
  // rendering this will return the most immediate parent window handle. For
  // windowless rendering this will return the parent window handle specified by
  // the client, which may be NULL. May be called on multiple threads.
  virtual CefWindowHandle GetHostWindowHandle() const;

  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  virtual views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with views-
  // based browsers.
  virtual CefRefPtr<CefBrowserView> GetBrowserView() const;

  // Returns the WebContentsModalDialogHost associated with this browser.
  virtual web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      const;

  // Called after the WebContents have been created for a new popup browser
  // parented to this browser but before the AlloyBrowserHostImpl is created for
  // the popup. |is_devtools| will be true if the popup will host DevTools. This
  // method will be called before WebContentsCreated() is called on
  // |new_platform_delegate|. Do not make the new browser visible in this
  // callback.
  virtual void PopupWebContentsCreated(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* new_web_contents,
      CefBrowserPlatformDelegate* new_platform_delegate,
      bool is_devtools);

  // Called after the AlloyBrowserHostImpl is created for a new popup browser
  // parented to this browser. |is_devtools| will be true if the popup will host
  // DevTools. This method will be called immediately after
  // CefLifeSpanHandler::OnAfterCreated() for the popup browser. It is safe to
  // make the new browser visible in this callback (for example, add the browser
  // to a window and show it).
  virtual void PopupBrowserCreated(CefBrowserHostBase* new_browser,
                                   bool is_devtools);

  // Returns the background color for the browser. The alpha component will be
  // either SK_AlphaTRANSPARENT or SK_AlphaOPAQUE (e.g. fully transparent or
  // fully opaque). SK_AlphaOPAQUE will always be returned for windowed
  // browsers. SK_ColorTRANSPARENT may be returned for windowless browsers to
  // enable transparency.
  virtual SkColor GetBackgroundColor() const;

  // Notify the window that it was resized.
  virtual void WasResized();

  // Send input events.
  virtual void SendKeyEvent(const CefKeyEvent& event);
  virtual void SendMouseClickEvent(const CefMouseEvent& event,
                                   CefBrowserHost::MouseButtonType type,
                                   bool mouseUp,
                                   int clickCount);
  virtual void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave);
  virtual void SendTouchpadFlingEvent(const CefMouseEvent& event, double vx, double vy);
  virtual void SendMouseWheelEvent(const CefMouseEvent& event,
                                   int deltaX,
                                   int deltaY);
  virtual void SendTouchEvent(const CefTouchEvent& event);

  // Send focus event. The browser's WebContents may be NULL when this method is
  // called.
  virtual void SetFocus(bool setFocus);

  // Send capture lost event.
  virtual void SendCaptureLostEvent();

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  // The window hosting the browser is about to be moved or resized. Only used
  // on Windows and Linux.
  virtual void NotifyMoveOrResizeStarted();

  // Resize the host window to the given dimensions. Only used with windowed
  // rendering on Windows and Linux.
  virtual void SizeTo(int width, int height);
#endif

  // Convert from view DIP coordinates to screen coordinates. If
  // |want_dip_coords| is true return DIP instead of device (pixel) coordinates
  // on Windows/Linux.
  virtual gfx::Point GetScreenPoint(const gfx::Point& view,
                                    bool want_dip_coords) const;

  // Open the specified text in the default text editor.
  virtual void ViewText(const std::string& text);

  // Forward the keyboard event to the application or frame window to allow
  // processing of shortcut keys.
  virtual bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event);

  // See WebContentsDelegate documentation.
  virtual bool PreHandleGestureEvent(content::WebContents* source,
                                     const blink::WebGestureEvent& event);

  // See WebContentsDelegate documentation.
  virtual bool IsNeverComposited(content::WebContents* web_contents);

  // Invoke platform specific handling for the external protocol.
  static void HandleExternalProtocol(const GURL& url);

  // Returns the OS event handle, if any, associated with |event|.
  virtual CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const;

  // Create the platform-specific JavaScript dialog runner.
  virtual std::unique_ptr<CefJavaScriptDialogRunner>
  CreateJavaScriptDialogRunner();

  // Create the platform-specific menu runner.
  virtual std::unique_ptr<CefMenuRunner> CreateMenuRunner();

  // Optionally modify the bounding box for the Chrome Find bar.
  virtual void UpdateFindBarBoundingBox(gfx::Rect* bounds) const;

  // Returns true if this delegate implements windowless rendering. May be
  // called on multiple threads.
  virtual bool IsWindowless() const;

  // Returns true if this delegate implements views-hosted browser handling. May
  // be called on multiple threads.
  virtual bool IsViewsHosted() const;

  // Returns true if this delegate implements a browser with external
  // (client-provided) parent window. May be called on multiple threads.
  virtual bool HasExternalParent() const;

  // Notify the browser that it was hidden. Only used with windowless rendering.
  virtual void WasHidden(bool hidden);

  // Returns true if the browser is currently hidden. Only used with windowless
  // rendering.
  virtual bool IsHidden() const;

#if BUILDFLAG(IS_OHOS)
  // Notify the browser that it was occluded. Only used with windowless
  // rendering.
  virtual void WasOccluded(bool occluded);

  virtual void OnWindowShow();

  virtual void OnWindowHide();

  virtual void OnOnlineRenderToForeground();

  // Send touch event list to the browser for a windowless browser.
  virtual void SendTouchEventList(const std::vector<CefTouchEvent>& event_list);

  virtual void NotifyForNextTouchEvent();

  virtual void SetNativeEmbedMode(bool flag) {};

  virtual void MaximizeResize() {}
#endif

  // Notify the browser that screen information has changed. Only used with
  // windowless rendering.
  virtual void NotifyScreenInfoChanged();
#if BUILDFLAG(IS_OHOS)
  virtual void NotifyScreenInfoChangedV2();
#endif

  // Invalidate the view. Only used with windowless rendering.
  virtual void Invalidate(cef_paint_element_type_t type);

  // Send the external begin frame message. Only used with windowless rendering.
  virtual void SendExternalBeginFrame();

  // Set the windowless frame rate. Only used with windowless rendering.
  virtual void SetWindowlessFrameRate(int frame_rate);

  // IME-related callbacks. See documentation in CefBrowser and
  // CefRenderHandler. Only used with windowless rendering.
  virtual void ImeSetComposition(
      const CefString& text,
      const std::vector<CefCompositionUnderline>& underlines,
      const CefRange& replacement_range,
      const CefRange& selection_range);
  virtual void ImeCommitText(const CefString& text,
                             const CefRange& replacement_range,
                             int relative_cursor_pos);
  virtual void ImeFinishComposingText(bool keep_selection);
  virtual void ImeCancelComposition();

  // Drag/drop-related callbacks. See documentation in CefRenderHandler. Only
  // used with windowless rendering.
  virtual void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                                   const CefMouseEvent& event,
                                   cef_drag_operations_mask_t allowed_ops);
  virtual void DragTargetDragOver(const CefMouseEvent& event,
                                  cef_drag_operations_mask_t allowed_ops);
  virtual void DragTargetDragLeave();
  virtual void DragTargetDrop(const CefMouseEvent& event);
  virtual void StartDragging(
      const content::DropData& drop_data,
      blink::DragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const blink::mojom::DragEventSourceInfo& event_info,
      content::RenderWidgetHostImpl* source_rwh);
  virtual void UpdateDragCursor(ui::mojom::DragOperation operation);
  virtual void DragSourceEndedAt(int x, int y, cef_drag_operations_mask_t op);
  virtual void DragSourceSystemDragEnded();
  virtual void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& eventData);
  virtual void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& locData);
  virtual gfx::Point GetDialogPosition(const gfx::Size& size);
  virtual gfx::Size GetMaximumDialogSize();

  // See CefBrowserHost documentation.
  virtual void SetAutoResizeEnabled(bool enabled,
                                    const CefSize& min_size,
                                    const CefSize& max_size);
  virtual void SetAccessibilityState(cef_state_t accessibility_state);
#if BUILDFLAG(IS_OHOS)
  virtual content::BrowserAccessibilityManager*
  GetRootBrowserAccessibilityManager() {
    return nullptr;
  }
#endif
  virtual bool IsPrintPreviewSupported() const;
  virtual void Find(const CefString& searchText,
                    bool forward,
                    bool matchCase,
                    bool findNext
#if BUILDFLAG(IS_OHOS)
                    , bool newSession
#endif
  );
  virtual void StopFinding(bool clearSelection);

#if defined(OHOS_COMPOSITE_RENDER)
  virtual void SetShouldFrameSubmissionBeforeDraw(bool should) {}
  virtual void WasKeyboardResized() {}
  virtual void SetDrawRect(int x, int y, int width, int height) {}
  virtual void SetDrawMode(int mode) {}
  virtual bool GetPendingSizeStatus() { return false; }
  virtual void SetFitContentMode(int mode) {}
  virtual void SetNativeInnerWeb(bool isInnerWeb) {}
#endif  // defined(OHOS_COMPOSITE_RENDER)

#ifdef OHOS_HTML_SELECT
  virtual void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection);
#endif  // #ifdef OHOS_HTML_SELECT

#ifdef OHOS_ARKWEB_ADBLOCK
  virtual void OnAdsBlocked(
      const std::string& main_frame_url,
      const std::map<std::string, int32_t>& subresource_blocked,
      bool is_site_first_report);
#endif

#if defined(OHOS_EX_PASSWORD)
  virtual void ShowPasswordDialog(bool is_update, const std::string& url);
#endif  // OHOS_EX_PASSWORD

#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
  virtual void OnShowAutofillPopup(
      const gfx::RectF& element_bounds,
      bool is_rtl,
      const std::vector<autofill::Suggestion>& suggestions,
      bool is_password_popup_type);
  virtual void OnHideAutofillPopup();
#endif

#if defined(OHOS_PRINT)
  virtual void SetToken(void* token) {}
  virtual void CreateWebPrintDocumentAdapter(const CefString& jobName,
                                             void** webPrintDocumentAdapter) {}
  virtual void SetPrintBackground(bool enable) {}
  virtual bool GetPrintBackground() { return false; }
#endif  // defined(OHOS_PRINT)

#if defined(OHOS_INPUT_EVENTS)
  virtual void SetVirtualKeyBoardArg(int32_t width,
                                     int32_t height,
                                     double keyboard) {}
  virtual bool ShouldVirtualKeyboardOverlay() { return false; }

  virtual void OnNativeEmbedLifecycleChange(
      const CefRenderHandler::CefNativeEmbedData& info) {}
  virtual void OnNativeEmbedVisibilityChange(
      const std::string& embed_id, bool visibility) {}
  virtual void SetScrollable(bool enable) {}
  virtual void AdvanceFocusForIME(int focusType) {}
  virtual void ScrollBy(float delta_x, float delta_y) {}
#endif

#ifdef OHOS_EX_TOPCONTROLS
  virtual int GetTopControlsOffset();
  virtual int GetShrinkViewportHeight();
#endif

#ifdef OHOS_AI
  virtual void CreateOverlay(const gfx::ImageSkia& image,
                             const gfx::Rect& image_rect,
                             const gfx::Point& touch_point);
  virtual void OnTextSelected(bool flag);
  virtual void OnDestroyImageAnalyzerOverlay();
  virtual float GetPageScaleFactor();
  virtual void OnFoldStatusChanged(uint32_t foldstatus);
#endif

#ifdef OHOS_DISPLAY_CUTOUT
  virtual void OnSafeInsetsChange(int left, int top, int right, int bottom);
#endif

#if defined(OHOS_SOFTWARE_COMPOSITOR)
  virtual bool WebPageSnapshot(const char* id,
                               int width,
                               int height,
                               cef_web_snapshot_callback_t callback);
#endif

#if defined(OHOS_INPUT_EVENTS)
  virtual void ScrollFocusedEditableNodeIntoView();
#endif
  virtual void ScaleGestureChangeV2(int type, float scale, float originScale, float centerX, float centerY);

#if defined(OHOS_DISPATCH_BEFORE_UNLOAD)
  virtual void OnBeforeUnloadFired(bool proceed);
#endif // OHOS_DISPATCH_BEFORE_UNLOAD

 protected:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<CefBrowserPlatformDelegate>;

  CefBrowserPlatformDelegate();
  virtual ~CefBrowserPlatformDelegate();

  static int TranslateWebEventModifiers(uint32 cef_modifiers);

  // Not owned by this object.
  content::WebContents* web_contents_ = nullptr;
  CefBrowserHostBase* browser_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
