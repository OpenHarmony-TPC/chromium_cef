/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
#pragma once
 
#include "cef/libcef/browser/osr/browser_platform_delegate_osr.h"
#include "cef/ohos_cef_ext/libcef/browser/osr/browser_platform_delegate_osr_ext.h"

class CefBrowserPlatformDelegateOsrUtils
{
public:
    CefBrowserPlatformDelegateOsr* cefBrowserPlatformDelegateOsr;
    CefBrowserPlatformDelegateOsrUtils(CefBrowserPlatformDelegateOsr* osr) { cefBrowserPlatformDelegateOsr = osr; }
    ~CefBrowserPlatformDelegateOsrUtils() = default;

    void InitializeAndUpdateRenderView();
    void RedistributeKeyEventIfUrlEmpty(const CefKeyEvent& event);
    void AdjustMouseClickCoordinates(CefRenderWidgetHostViewOSR* view, CefMouseEvent& mouseEvent);
    void CancelTouchpadFlingOnMouseClick(CefRenderWidgetHostViewOSR* view, const CefMouseEvent& event);
    void AdjustMouseMoveCoordinates(CefRenderWidgetHostViewOSR* view, CefMouseEvent& mouseEvent);
    void CancelTouchpadFlingMouseWheel(CefRenderWidgetHostViewOSR* view, const CefMouseEvent& event);
    void AdjustAndSendTouchEvent(CefRenderWidgetHostViewOSR* view, const CefTouchEvent& event);
    void SetFocusAndUpdateStatus(bool setFocus, CefRenderWidgetHostViewOSR* view);
    CefImageImpl* CreateDragImage(const gfx::ImageSkia& image);
    void RestoreTextHandlesAfterDrag();
    void UpdateNativeEmbedMode(CefRenderWidgetHostViewOSR* view);
#if BUILDFLAG(ARKWEB_VSYNC_SCHEDULE)
    void UpdateBypassVsyncCondition(CefRenderWidgetHostViewOSR* view);
#endif
};
 
#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_UTILS_H_
