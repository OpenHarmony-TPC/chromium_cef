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

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_

#include <memory>
#include <string>

#include "extensions/browser/extension_event_histogram_value.h"
#include "ui/base/page_transition_types.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}

class GURL;

namespace extensions {
namespace cef {

namespace web_navigation_api_helpers {

// Returns the frame ID as it will be passed to the extension:
// 0 if the navigation happens in the main frame, or the frame routing ID
// otherwise.
int GetFrameId(content::RenderFrameHost* frame_host);

// Create and dispatch the various events of the webNavigation API.
std::unique_ptr<Event> CreateOnBeforeNavigateEvent(
    content::NavigationHandle* navigation_handle);

void DispatchOnCommitted(events::HistogramValue histogram_value,
                         const std::string& event_name,
                         content::NavigationHandle* navigation_handle);

void DispatchOnDOMContentLoaded(content::WebContents* web_contents,
                                content::RenderFrameHost* frame_host,
                                const GURL& url);

void DispatchOnCompleted(content::WebContents* web_contents,
                         content::RenderFrameHost* frame_host,
                         const GURL& url);

void DispatchOnCreatedNavigationTarget(
    int source_tab_id,
    int source_render_process_id,
    int source_extension_frame_id,
    content::BrowserContext* browser_context,
    content::WebContents* target_web_contents,
    const GURL& target_url);

void DispatchOnErrorOccurred(content::WebContents* web_contents,
                             content::RenderFrameHost* frame_host,
                             const GURL& url,
                             int error_code);
void DispatchOnErrorOccurred(content::NavigationHandle* navigation_handle);

void DispatchOnTabReplaced(
    content::WebContents* old_web_contents,
    content::BrowserContext* browser_context,
    content::WebContents* new_web_contents);

}  // namespace web_navigation_api_helpers

}  // cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_
