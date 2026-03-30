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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
#pragma once
 
#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/osr/browser_platform_delegate_osr.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/browser_platform_delegate_osr_ext.h"

class CefBrowserPlatformDelegateOsrUtils
{
public:
    raw_ptr<CefBrowserPlatformDelegateOsr> cefBrowserPlatformDelegateOsr;
    CefBrowserPlatformDelegateOsrUtils(CefBrowserPlatformDelegateOsr* osr) { cefBrowserPlatformDelegateOsr = osr; }
    ~CefBrowserPlatformDelegateOsrUtils() = default;

    void InitializeAndUpdateRenderView();
    void RedistributeKeyEventIfUrlEmpty(const CefKeyEvent& event);
    void CancelTouchpadFlingOnMouseClick(CefRenderWidgetHostViewOSR* view, const CefMouseEvent& event);
    void AdjustMouseEventCoordinates(CefRenderWidgetHostViewOSR* view,
                                     const CefMouseEvent& mouseEventevent,
                                     blink::WebMouseEvent& event);
    void CancelTouchpadFlingMouseWheel(CefRenderWidgetHostViewOSR* view, const CefMouseEvent& event);
    void AdjustAndSendTouchEvent(CefRenderWidgetHostViewOSR* view, const CefTouchEvent& event);
    void SetFocusAndUpdateStatus(bool setFocus, CefRenderWidgetHostViewOSR* view);
    CefImageImpl* CreateDragImage(const gfx::ImageSkia& image);
    void RestoreTextHandlesAfterDrag();
#if BUILDFLAG(ARKWEB_SAME_LAYER)
    void UpdateNativeEmbedMode(CefRenderWidgetHostViewOSR* view);
    void SetEnableCustomVideoPlayer(CefRenderWidgetHostViewOSR* view);
#endif
#if BUILDFLAG(ARKWEB_VSYNC_SCHEDULE)
    void UpdateBypassVsyncCondition(CefRenderWidgetHostViewOSR* view);
#endif

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
    void EvictFrameBackBuffersWhenNWebWasHidden();
    void SetIsOfflineWebComponent();
#endif
};
 
#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
