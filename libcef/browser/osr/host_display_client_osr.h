// Copyright 2019 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/viz/host/host_display_client.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "ui/gfx/native_widget_types.h"

class CefLayeredWindowUpdaterOSR;
class CefRenderWidgetHostViewOSR;

class CefHostDisplayClientOSR : public viz::HostDisplayClient {
 public:
  CefHostDisplayClientOSR(CefRenderWidgetHostViewOSR* const view,
                          gfx::AcceleratedWidget widget);

  CefHostDisplayClientOSR(const CefHostDisplayClientOSR&) = delete;
  CefHostDisplayClientOSR& operator=(const CefHostDisplayClientOSR&) = delete;

  ~CefHostDisplayClientOSR() override;

  void SetActive(bool active);
  const void* GetPixelMemory() const;
  gfx::Size GetPixelSize() const;

#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
  void NotifyFirstRealSwapBuffer() override;
  void AddFirstRealSwapBufferListener(base::RepeatingCallback<void()> func);
#endif  // BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)

 private:
  // mojom::DisplayClient implementation.
  void UseProxyOutputDevice(UseProxyOutputDeviceCallback callback) override;

  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<viz::mojom::LayeredWindowUpdater> receiver)
      override;

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  void DidCompleteSwapWithNewSizeOHOS(const gfx::Size& size) override;
  CefRefPtr<AlloyBrowserHostImpl> browser_impl_;
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

  const raw_ptr<CefRenderWidgetHostViewOSR> view_;
  std::unique_ptr<CefLayeredWindowUpdaterOSR> layered_window_updater_;
  bool active_ = false;
#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
  base::RepeatingCallback<void()> first_real_swap_buffer_listener_;
#endif  // BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
};

#endif  // CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
