// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/ohos_safe_browsing_error_page_controller_delegate_impl.h"

#include "content/public/renderer/render_frame.h"

AlloySafeBrowsingErrorPageControllerDelegateImpl::
    AlloySafeBrowsingErrorPageControllerDelegateImpl(
        content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<
          AlloySafeBrowsingErrorPageControllerDelegateImpl>(render_frame) {}

AlloySafeBrowsingErrorPageControllerDelegateImpl::
    ~AlloySafeBrowsingErrorPageControllerDelegateImpl() = default;

void AlloySafeBrowsingErrorPageControllerDelegateImpl::PrepareForErrorPage() {
  pending_error_ = true;
}

void AlloySafeBrowsingErrorPageControllerDelegateImpl::OnDestruct() {
  delete this;
}

void AlloySafeBrowsingErrorPageControllerDelegateImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  committed_error_ = pending_error_;
  pending_error_ = false;
}

void AlloySafeBrowsingErrorPageControllerDelegateImpl::DidFinishLoad() {
  if (committed_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame());
  }
}
