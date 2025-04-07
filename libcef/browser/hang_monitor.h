/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
#define CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
#pragma once

#include "base/functional/callback.h"
#include "cef/include/cef_unresponsive_process_callback.h"

namespace content {
class RenderWidgetHost;
}

class CefBrowserHostBase;

namespace hang_monitor {

// Called from WebContentsDelegate::RendererUnresponsive.
// Returns false for default handling.
bool RendererUnresponsive(CefBrowserHostBase* browser,
                          content::RenderWidgetHost* render_widget_host,
                          base::RepeatingClosure hang_monitor_restarter);

// Called from WebContentsDelegate::RendererResponsive.
// Returns false for default handling.
bool RendererResponsive(CefBrowserHostBase* browser,
                        content::RenderWidgetHost* render_widget_host);

// Detach an existing callback object.
void Detach(CefRefPtr<CefUnresponsiveProcessCallback> callback);

}  // namespace hang_monitor

#endif  // CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
