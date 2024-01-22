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