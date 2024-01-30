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