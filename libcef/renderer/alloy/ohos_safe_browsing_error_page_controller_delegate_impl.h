// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace content {
class RenderFrame;
}  // namespace content

class AlloySafeBrowsingErrorPageControllerDelegateImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          AlloySafeBrowsingErrorPageControllerDelegateImpl> {
 public:
  explicit AlloySafeBrowsingErrorPageControllerDelegateImpl(
      content::RenderFrame* render_frame);

  // Disallow copy and assign
  AlloySafeBrowsingErrorPageControllerDelegateImpl(
      const AlloySafeBrowsingErrorPageControllerDelegateImpl&) = delete;
  AlloySafeBrowsingErrorPageControllerDelegateImpl& operator=(
      const AlloySafeBrowsingErrorPageControllerDelegateImpl&) = delete;

  ~AlloySafeBrowsingErrorPageControllerDelegateImpl() override;

  // Notifies us that a navigation error has occurred and will be committed
  void PrepareForErrorPage();

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;

 private:
  // Whether there is an error page pending to be committed.
  bool pending_error_ = false;

  // Whether the committed page is an error page.
  bool committed_error_ = false;
};