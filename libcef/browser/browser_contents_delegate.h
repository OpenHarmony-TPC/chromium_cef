// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
#pragma once

#include <memory>

#include "libcef/browser/frame_host_impl.h"

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_OHOS)
#include "libcef/browser/icon_helper.h"
#endif

class CefBrowser;
class CefBrowserInfo;
class CefBrowserPlatformDelegate;
class CefClient;

// Flags that represent which states have changed.
enum class CefBrowserContentsState : uint8_t {
  kNone = 0,
  kNavigation = (1 << 0),
  kDocument = (1 << 1),
  kFullscreen = (1 << 2),
  kFocusedFrame = (1 << 3),
};

constexpr inline CefBrowserContentsState operator&(
    CefBrowserContentsState lhs,
    CefBrowserContentsState rhs) {
  return static_cast<CefBrowserContentsState>(static_cast<int>(lhs) &
                                              static_cast<int>(rhs));
}

constexpr inline CefBrowserContentsState operator|(
    CefBrowserContentsState lhs,
    CefBrowserContentsState rhs) {
  return static_cast<CefBrowserContentsState>(static_cast<int>(lhs) |
                                              static_cast<int>(rhs));
}

// Tracks state and executes client callbacks based on WebContents callbacks.
// Includes functionality that is shared by the alloy and chrome runtimes.
// Only accessed on the UI thread.
class CefBrowserContentsDelegate : public content::WebContentsDelegate,
                                   public content::WebContentsObserver,
                                   public content::NotificationObserver {
 public:
  using State = CefBrowserContentsState;

  // Interface to implement for observers that wish to be informed of changes
  // to the delegate. All methods will be called on the UI thread.
  class Observer : public base::CheckedObserver {
   public:
    // Called after state has changed and before the associated CefClient
    // callback is executed.
    virtual void OnStateChanged(State state_changed) = 0;

    // Called when the associated WebContents is destroyed.
    virtual void OnWebContentsDestroyed(content::WebContents* web_contents) = 0;

   protected:
    ~Observer() override {}
  };

  explicit CefBrowserContentsDelegate(
      scoped_refptr<CefBrowserInfo> browser_info);

  CefBrowserContentsDelegate(const CefBrowserContentsDelegate&) = delete;
  CefBrowserContentsDelegate& operator=(const CefBrowserContentsDelegate&) =
      delete;

  void ObserveWebContents(content::WebContents* new_contents);

  // Manage observer objects. The observer must either outlive this object or
  // be removed before destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // WebContentsDelegate methods:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
#if defined(OHOS_ARKWEB_EXTENSIONS)
  void WebExtensionUpdateTab(
      int32_t tab_id,
      const NWebExtensionTabUpdateProperties* update_properties) override;
#endif
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

#if BUILDFLAG(IS_OHOS)
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostMouseLock() override;
  void UnlockMouse();

  // Shows the repost form confirmation dialog box.
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
#endif

  // WebContentsObserver methods:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderWidgetCreated(
      content::RenderWidgetHost* render_widget_host) override;
  void RenderViewReady() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnFrameFocused(content::RenderFrameHost* render_frame_host) override;
  void PrimaryMainDocumentElementAvailable() override;
  void LoadProgressChanged(double progress) override;
  void DidStopLoading() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;
  void WebContentsDestroyed() override;
#ifdef OHOS_NETWORK_LOAD
  void DidStartLoading() override;
#endif

#if defined(OHOS_WPT)
  void DidStartNavigation(content::NavigationHandle* navigation) override;
#endif  // defined(OHOS_WPT)

#if defined(OHOS_FAVICON)
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
#endif  // defined(OHOS_FAVICON)

#if defined(OHOS_NAVIGATION)
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
#endif  // defined(OHOS_NAVIGATION)

#if defined(OHOS_DISPLAY_CUTOUT)
  void ViewportFitChanged(blink::mojom::ViewportFit value) override;
#endif

  // NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Accessors for state information. Changes will be signaled to
  // Observer::OnStateChanged.
  bool is_loading() const { return is_loading_; }
#if !BUILDFLAG(IS_OHOS)
  bool can_go_back() const { return can_go_back_; }
  bool can_go_forward() const { return can_go_forward_; }
#endif
  bool has_document() const { return has_document_; }
  bool is_fullscreen() const { return is_fullscreen_; }
  CefRefPtr<CefFrameHostImpl> focused_frame() const { return focused_frame_; }

  // Helpers for executing client callbacks.
  // TODO(cef): Make this private if/when possible.
  bool OnSetFocus(cef_focus_source_t source);

#if BUILDFLAG(IS_OHOS)
  void InitIconHelper();
#endif

 private:
  CefRefPtr<CefClient> client() const;
  CefRefPtr<CefBrowser> browser() const;
  CefBrowserPlatformDelegate* platform_delegate() const;

  // Helpers for executing client callbacks.
  void OnAddressChange(const GURL& url);
  void OnLoadStart(CefRefPtr<CefFrame> frame,
                   ui::PageTransition transition_type);
  void OnLoadEnd(CefRefPtr<CefFrame> frame,
                 const GURL& url,
                 int http_status_code);
  void OnLoadError(CefRefPtr<CefFrame> frame, const GURL& url, int error_code);
  void OnTitleChange(const std::u16string& title);
  void OnFullscreenModeChange(bool fullscreen
#if defined(OHOS_MEDIA)
                              ,
                              const CefSize& video_natural_size
#endif  // defined(OHOS_MEDIA)
  );

  void OnStateChanged(State state_changed);

#if BUILDFLAG(IS_OHOS)
  void OnLoadError(CefRefPtr<CefRequest> request,
                   bool is_in_main_frame,
                   bool has_user_gesture,
                   int error_code);
  void OnOldPageNoLongerRendered(const GURL& url, bool success);
  void OnRefreshAccessedHistory(CefRefPtr<CefFrame> frame,
                                const GURL& url,
                                bool isReload);

  // Returns true if the mouse is locked.
  bool IsMouseLocked() const;

  // Returns true if the mouse was locked and no notification should be
  // displayed to the user.
  bool IsMouseLockedSilently() const;

  void SetTabWithExclusiveAccess(content::WebContents* tab);

  bool HandleUserKeyEvent(const content::NativeWebKeyboardEvent& event);
#endif

  scoped_refptr<CefBrowserInfo> browser_info_;

  bool is_loading_ = false;

#if !BUILDFLAG(IS_OHOS)
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
#endif

  bool has_document_ = false;
  bool is_fullscreen_ = false;

  // The currently focused frame, or nullptr if the main frame is focused.
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // True if currently in the OnSetFocus callback.
  bool is_in_onsetfocus_ = false;

  // Observers that want to be notified of changes to this object.
  base::ObserverList<Observer> observers_;

  // Used for managing notification subscriptions.
  std::unique_ptr<content::NotificationRegistrar> registrar_;

  // True if the focus is currently on an editable field on the page.
  bool focus_on_editable_field_ = false;

#if BUILDFLAG(IS_OHOS)
  // Store web site icon.
  CefRefPtr<IconHelper> icon_helper_;

  enum MouseLockState {
    MOUSELOCK_UNLOCKED,
    // Mouse has been locked.
    MOUSELOCK_LOCKED,
    // Mouse has been locked silently, with no notification to user.
    MOUSELOCK_LOCKED_SILENTLY
  };

  MouseLockState mouse_lock_state_;

  // Timestamp when the user last successfully escaped from a lock request.
  base::TimeTicks last_user_escape_time_;

  content::WebContents* tab_with_exclusive_access_ = nullptr;
#endif

#ifdef OHOS_NETWORK_LOAD
  bool observe_need_report_title_ = false;
#endif
#if BUILDFLAG(IS_OHOS)
  base::WeakPtrFactory<CefBrowserContentsDelegate> weak_factory_{this};
#endif
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
