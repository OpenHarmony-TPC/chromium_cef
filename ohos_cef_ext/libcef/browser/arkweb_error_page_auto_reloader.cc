/*
 * Copyright (c) 2025-2025 Huawei Device Co., Ltd.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cef/ohos_cef_ext/libcef/browser/arkweb_error_page_auto_reloader.h"

#include <algorithm>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/ohos_nweb_ex/overrides/net/proxy_resolution/fallback_proxy_utils.h"
#include "base/command_line.h"
#include "content/public/browser/browser_context.h"
#endif

namespace OHOS::NWeb {

namespace {
#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
content::ErrorPageReloadReason SetErrorPageReloadReason(
    content::NavigationHandle* handle,
    int net_error) {
  if (handle->NeedsReloadWithFallbackProxy()) {
    return content::ErrorPageReloadReason::FALLBACK_PROXY;
  }
  return content::ErrorPageReloadReason::INVALID;
}
#endif

bool ShouldAutoReload(content::NavigationHandle* handle) {
  DCHECK(handle->HasCommitted());
#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNwebEx)) {
    return false;
  }

  if (handle->NeedsReloadWithFallbackProxy()) {
    return true;
  }
#endif

  return false;
}

// Helper to block a navigation that would result in re-committing the same
// error page a tab is already displaying.
class IgnoreDuplicateErrorThrottle : public content::NavigationThrottle {
 public:
  using ShouldSuppressCallback =
      base::OnceCallback<bool(content::NavigationHandle*)>;

  IgnoreDuplicateErrorThrottle(content::NavigationHandle* handle,
                               ShouldSuppressCallback should_suppress)
      : content::NavigationThrottle(handle),
        should_suppress_(std::move(should_suppress)) {
    DCHECK(should_suppress_);
  }
  IgnoreDuplicateErrorThrottle(const IgnoreDuplicateErrorThrottle&) = delete;
  IgnoreDuplicateErrorThrottle& operator=(const IgnoreDuplicateErrorThrottle&) =
      delete;
  ~IgnoreDuplicateErrorThrottle() override = default;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    DCHECK(should_suppress_);
    if (std::move(should_suppress_).Run(navigation_handle()))
      return content::NavigationThrottle::ThrottleAction::CANCEL;
    return content::NavigationThrottle::ThrottleAction::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "IgnoreDuplicateErrorThrottle";
  }

 private:
  ShouldSuppressCallback should_suppress_;
};

}  // namespace

NetErrorAutoReloader::ErrorPageInfo::ErrorPageInfo(const GURL& url,
                                                   net::Error error)
    : url(url), error(error) {}

NetErrorAutoReloader::ErrorPageInfo::~ErrorPageInfo() = default;

NetErrorAutoReloader::NetErrorAutoReloader(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NetErrorAutoReloader>(*web_contents),
      connection_tracker_(content::GetNetworkConnectionTracker()) {
  connection_tracker_->AddNetworkConnectionObserver(this);

  network::mojom::ConnectionType connection_type;
  if (connection_tracker_->GetConnectionType(
          &connection_type,
          base::BindOnce(&NetErrorAutoReloader::SetInitialConnectionType,
                         weak_ptr_factory_.GetWeakPtr()))) {
    SetInitialConnectionType(connection_type);
  }
}

NetErrorAutoReloader::~NetErrorAutoReloader() {
  if (connection_tracker_)
    connection_tracker_->RemoveNetworkConnectionObserver(this);
}

// static
std::unique_ptr<content::NavigationThrottle>
NetErrorAutoReloader::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return nullptr;

  // Note that `CreateForWebContents` is a no-op if `contents` already has a
  // NetErrorAutoReloader. See WebContentsUserData.
  content::WebContents* contents = handle->GetWebContents();
  CreateForWebContents(contents);
  return FromWebContents(contents)->MaybeCreateThrottle(handle);
}

void NetErrorAutoReloader::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return;

  // Suppress automatic reload as long as any navigations are pending.
  PauseAutoReloadTimerIfRunning();
  pending_navigations_.insert(handle);
}

void NetErrorAutoReloader::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return;

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  if (handle->GetCurrentReloadReason() !=
      content::ErrorPageReloadReason::INVALID) {
    ReportFallProxyReloadErrorCodeInfo(
        handle->GetURL().spec(), (int)handle->GetCurrentReloadReason(),
        handle->GetOriginalNetErrorCode(), handle->GetNetErrorCode());
  }
#endif

  pending_navigations_.erase(handle);
  if (!handle->HasCommitted()) {
    // This navigation was cancelled and not committed. If there are still other
    // pending navigations, or we aren't sitting on a error page which allows
    // auto-reload, there's nothing to do.
    if (!pending_navigations_.empty() || !current_reloadable_error_page_info_)
      return;

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNwebEx) &&
        current_reloadable_error_page_info_ &&
        current_reloadable_error_page_info_->reason >
            content::ErrorPageReloadReason::INVALID &&
        handle->HasBeenReloadedForThisReason(
            current_reloadable_error_page_info_->reason)) {
      return;
    }
#endif

    // The last pending navigation was just cancelled and we're sitting on an
    // error page which allows auto-reload. Schedule the next auto-reload
    // attempt.
    is_auto_reload_in_progress_ = false;
    ScheduleNextAutoReload();
    return;
  }

  if (!ShouldAutoReload(handle)) {
    // We've committed something that doesn't support auto-reload. Reset
    // all auto-reload state so nothing interesting happens until another
    // error page navigation is committed.
    Reset();
    return;
  }

  // This heuristic isn't perfect but it should be good enough: if the new
  // commit is not a reload, or if it's an error page with an error code
  // different from what we had previously committed, we treat it as a new
  // error and thus reset our tracking state.
  net::Error net_error = handle->GetNetErrorCode();
  if (handle->GetReloadType() == content::ReloadType::NONE ||
      !current_reloadable_error_page_info_ ||
      net_error != current_reloadable_error_page_info_->error) {
    Reset();
    current_reloadable_error_page_info_ =
        ErrorPageInfo(handle->GetURL(), net_error);
  }

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  current_reloadable_error_page_info_->reason =
      SetErrorPageReloadReason(handle, net_error);
#endif

  // We only schedule a reload if there are no other pending navigations.
  // If there are and they end up getting terminated without a commit, we
  // will schedule the next auto-reload at that time.
  if (pending_navigations_.empty())
    ScheduleNextAutoReload();
}

void NetErrorAutoReloader::NavigationStopped() {
  // Stopping navigation or loading cancels all pending auto-reload behavior
  // until the next time a new error page is committed. Note that a stop during
  // navigation will also result in a DidFinishNavigation with a failed
  // navigation and an error code of ERR_ABORTED. However stops can also occur
  // after an error page commits but before it finishes loading, and we want to
  // catch those cases too.
  Reset();
}

void NetErrorAutoReloader::OnVisibilityChanged(content::Visibility visibility) {
  if (!IsWebContentsVisible()) {
    PauseAutoReloadTimerIfRunning();
  } else if (pending_navigations_.empty()) {
    ResumeAutoReloadIfPaused();
  }
}

void NetErrorAutoReloader::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  is_online_ = (type != network::mojom::ConnectionType::CONNECTION_NONE);
  if (!is_online_) {
    PauseAutoReloadTimerIfRunning();
  } else if (pending_navigations_.empty()) {
    ResumeAutoReloadIfPaused();
  }
}

void NetErrorAutoReloader::SetInitialConnectionType(
    network::mojom::ConnectionType type) {
  if (connection_tracker_)
    OnConnectionChanged(type);
}

bool NetErrorAutoReloader::IsWebContentsVisible() {
  return web_contents()->GetVisibility() != content::Visibility::HIDDEN;
}

void NetErrorAutoReloader::Reset() {
  reload_timer_.reset();
  is_auto_reload_in_progress_ = false;
  current_reloadable_error_page_info_.reset();
}

void NetErrorAutoReloader::PauseAutoReloadTimerIfRunning() {
  reload_timer_.reset();
}

void NetErrorAutoReloader::ResumeAutoReloadIfPaused() {
  if (current_reloadable_error_page_info_ && !reload_timer_)
    ScheduleNextAutoReload();
}

void NetErrorAutoReloader::ScheduleNextAutoReload() {
  DCHECK(current_reloadable_error_page_info_);
  if (!is_online_ || !IsWebContentsVisible())
    return;

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
           ::switches::kEnableNwebEx)) {
    return;
  }

  if (current_reloadable_error_page_info_->reason !=
      content::ErrorPageReloadReason::FALLBACK_PROXY) {
    return;
  }
#endif

  // Note that Unretained is safe here because base::OneShotTimer will never
  // run its callback once destructed.
  reload_timer_.emplace();

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  reload_timer_->Start(
      FROM_HERE, base::Seconds(1),
      base::BindOnce(&NetErrorAutoReloader::ReloadMainFrame,
                      base::Unretained(this)));
#endif
}

void NetErrorAutoReloader::ReloadMainFrame() {
  DCHECK(current_reloadable_error_page_info_);
  if (!is_online_ || !IsWebContentsVisible())
    return;

  is_auto_reload_in_progress_ = true;

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  int error_no = current_reloadable_error_page_info_->error;
  if (current_reloadable_error_page_info_) {
    LOG(INFO) << "[ErrReload] Net error auto reloader --url: "
              << url::LogUtils::ConvertUrlWithMask(
                     current_reloadable_error_page_info_->url.spec())
              << " error code: " << error_no << ", reason "
              << (int)current_reloadable_error_page_info_->reason;
  }
  web_contents()->GetController().ReloadWithNetError(
      content::ReloadType::NORMAL, true,
      current_reloadable_error_page_info_->reason);
#else
  web_contents()->GetPrimaryMainFrame()->Reload();
#endif
}

std::unique_ptr<content::NavigationThrottle>
NetErrorAutoReloader::MaybeCreateThrottle(content::NavigationHandle* handle) {
  DCHECK(handle->IsInPrimaryMainFrame());
  if (!current_reloadable_error_page_info_ ||
      current_reloadable_error_page_info_->url != handle->GetURL() ||
      !is_auto_reload_in_progress_) {
    return nullptr;
  }

  return std::make_unique<IgnoreDuplicateErrorThrottle>(
      handle, base::BindOnce(&NetErrorAutoReloader::ShouldSuppressErrorPage,
                             base::Unretained(this)));
}

bool NetErrorAutoReloader::ShouldSuppressErrorPage(
    content::NavigationHandle* handle) {
  // We already verified these conditions when the throttle was created, but now
  // that the throttle is about to fail its navigation, we double-check in case
  // another navigation has committed in the interim.
  if (!current_reloadable_error_page_info_ ||
      current_reloadable_error_page_info_->url != handle->GetURL() ||
      current_reloadable_error_page_info_->error != handle->GetNetErrorCode()) {
    return false;
  }

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNwebEx) &&
      current_reloadable_error_page_info_->reason !=
      content::ErrorPageReloadReason::INVALID) {
    return false;
  }
#endif

  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NetErrorAutoReloader);

}  // namespace OHOS::NWeb
