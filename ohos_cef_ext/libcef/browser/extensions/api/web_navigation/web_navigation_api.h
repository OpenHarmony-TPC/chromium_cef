/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "url/gurl.h"

namespace extensions {
namespace cef {

// Tab contents observer that forwards navigation events to the event router.
class WebNavigationTabObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebNavigationTabObserver> {
 public:
  WebNavigationTabObserver(const WebNavigationTabObserver&) = delete;
  WebNavigationTabObserver& operator=(const WebNavigationTabObserver&) = delete;

  ~WebNavigationTabObserver() override;

  // Returns the object for the given |web_contents|.
  static WebNavigationTabObserver* Get(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // This method dispatches the already created onBeforeNavigate event.
  void DispatchCachedOnBeforeNavigate();

 private:
  explicit WebNavigationTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebNavigationTabObserver>;

  void HandleCommit(content::NavigationHandle* navigation_handle);
  void HandleError(content::NavigationHandle* navigation_handle);

  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(content::RenderFrameHost* frame_host,
                                     const GURL& url);

  // Called when a RenderFrameHost goes into pending deletion. Stop tracking it
  // and its children.
  void RenderFrameHostPendingDeletion(content::RenderFrameHost*);

  // The latest onBeforeNavigate event this frame has generated. It is stored
  // as it might not be sent immediately, but delayed until the tab is added to
  // the tab strip and is ready to dispatch events.
  std::unique_ptr<Event> pending_on_before_navigate_event_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Tracks new tab navigations and routes them as events to the extension system.
class WebNavigationEventRouter {
 public:
  explicit WebNavigationEventRouter(Profile* profile);

  WebNavigationEventRouter(const WebNavigationEventRouter&) = delete;
  WebNavigationEventRouter& operator=(const WebNavigationEventRouter&) = delete;

  ~WebNavigationEventRouter();

  // Router level handler for the creation of WebContents. Stores information
  // about the newly created WebContents. This information is later used when
  // the WebContents for the tab is added to the tabstrip and we receive the
  // TabStripModelChanged insertion.
  void RecordNewWebContents(content::WebContents* source_web_contents,
                            int source_render_process_id,
                            int source_render_frame_id,
                            GURL target_url,
                            content::WebContents* target_web_contents);

 private:

  // The profile that owns us via ExtensionService.
  raw_ptr<Profile> profile_;
};

// API function that returns the state of a given frame.
class WebNavigationGetFrameFunction : public ExtensionFunction {
  ~WebNavigationGetFrameFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getFrame", WEBNAVIGATION_GETFRAME)
};

// API function that returns the states of all frames in a given tab.
class WebNavigationGetAllFramesFunction : public ExtensionFunction {
  ~WebNavigationGetAllFramesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getAllFrames",
                             WEBNAVIGATION_GETALLFRAMES)
};

class WebNavigationAPI : public BrowserContextKeyedAPI,
                         public extensions::EventRouter::Observer {
 public:
  explicit WebNavigationAPI(content::BrowserContext* context);

  WebNavigationAPI(const WebNavigationAPI&) = delete;
  WebNavigationAPI& operator=(const WebNavigationAPI&) = delete;

  ~WebNavigationAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<WebNavigationAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebNavigationAPI>;
  friend class WebNavigationTabObserver;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "WebNavigationAPI";
  }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<WebNavigationEventRouter> web_navigation_event_router_;
};

}  // cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
