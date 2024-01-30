/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SAFE_BROWSING_TAB_HELPER_H_
#define CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SAFE_BROWSING_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace ohos_safe_browsing {

using SBCheckCallback = base::OnceCallback<void(const GURL&, int hw_code)>;

class OhosSafeBrowsingTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OhosSafeBrowsingTabHelper>,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  OhosSafeBrowsingTabHelper(const OhosSafeBrowsingTabHelper&) = delete;
  OhosSafeBrowsingTabHelper& operator=(const OhosSafeBrowsingTabHelper&) = delete;
  ~OhosSafeBrowsingTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationStopped() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  struct SBCheck;
  using SBCheckList = std::list<std::unique_ptr<SBCheck>>;

  friend class content::WebContentsUserData<OhosSafeBrowsingTabHelper>;

  explicit OhosSafeBrowsingTabHelper(content::WebContents* web_contents,
                                 bool incognito_mode,
                                 CefBrowserHostBase* browser);

  void OnSimpleLoaderComplete(SBCheckList::iterator it,
                              const GURL& url,
                              std::string converted_url,
                              std::unique_ptr<std::string> response_body);
  
  void QuerySafeBrowsingResults(
      content::NavigationHandle* navigation_handle);

  raw_ptr<network::NetworkConnectionTracker> connection_tracker_;
  SBCheckList checks_running_;
  std::unique_ptr<OhosSbClient> sb_client_;
  CefBrowserHostBase* browser_{nullptr};

  base::WeakPtrFactory<OhosSafeBrowsingTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ohos_safe_browsing

#endif  // CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SAFE_BROWSING_TAB_HELPER_H_
