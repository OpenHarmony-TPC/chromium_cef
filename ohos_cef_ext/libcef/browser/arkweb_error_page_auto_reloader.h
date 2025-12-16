/*
 * Copyright (c) 2025-2025 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_ARKWEB_ERR_PAGE_AUTO_RELOADER_H_
#define CEF_LIBCEF_BROWSER_ARKWEB_ERR_PAGE_AUTO_RELOADER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
#include "arkweb/chromium_ext/content/public/browser/error_page_reload_reason.h"
#endif

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

namespace OHOS::NWeb {

// This class implements support for automatic reload attempts with backoff
// whenever a WebContents' main frame lands on common network error pages. This
// excludes errors that aren't connectivity related since a reload doesn't
// generally fix them (e.g. SSL errors or when the client blocked the request).
// To use this behavior as a Content embedder, simply call the static
// `MaybeCreateNavigationThrottle()` method from within your implementation of
// ContentBrowserClient::CreateThrottlesForNavigation.
class NetErrorAutoReloader
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NetErrorAutoReloader>,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetErrorAutoReloader(const NetErrorAutoReloader&) = delete;
  NetErrorAutoReloader& operator=(const NetErrorAutoReloader&) = delete;
  ~NetErrorAutoReloader() override;

  // Maybe installs a throttle for the given navigation, lazily initializing the
  // appropriate WebContents' NetErrorAutoReloader instance if necessary. For
  // embedders wanting to use NetErrorAutoReload's behavior, it's sufficient to
  // call this from ContentBrowserClient::CreateThrottlesForNavigation for each
  // navigation processed.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // override content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void NavigationStopped() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // override network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  friend class content::WebContentsUserData<NetErrorAutoReloader>;

  explicit NetErrorAutoReloader(content::WebContents* web_contents);

  void SetInitialConnectionType(network::mojom::ConnectionType type);
  bool IsWebContentsVisible();
  void Reset();
  void PauseAutoReloadTimerIfRunning();
  void ResumeAutoReloadIfPaused();
  void ScheduleNextAutoReload();
  void ReloadMainFrame();
  std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottle(
      content::NavigationHandle* handle);
  bool ShouldSuppressErrorPage(content::NavigationHandle* handle);

  struct ErrorPageInfo {
    ErrorPageInfo(const GURL& url, net::Error error);
    ~ErrorPageInfo();

    GURL url;
    net::Error error;
#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
    content::ErrorPageReloadReason reason =
        content::ErrorPageReloadReason::INVALID;
#endif
  };

  raw_ptr<network::NetworkConnectionTracker> connection_tracker_;
  bool is_online_ = true;
  std::set<raw_ptr<content::NavigationHandle, SetExperimental>>
      pending_navigations_;
  std::optional<base::OneShotTimer> reload_timer_;
  std::optional<ErrorPageInfo> current_reloadable_error_page_info_;
  bool is_auto_reload_in_progress_ = false;
  base::WeakPtrFactory<NetErrorAutoReloader> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace OHOS::NWeb

#endif  // CEF_LIBCEF_BROWSER_ARKWEB_ERR_PAGE_AUTO_RELOADER_H_
