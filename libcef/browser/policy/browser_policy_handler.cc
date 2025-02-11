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

#include "browser_policy_handler.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "cef/libcef/browser/chrome/chrome_browser_context.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/common/app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_loader_ohos.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"

namespace policy {

namespace {
constexpr int32 kApiMinSdkVerison = 50001013;

Profile* GetActiveProfile() {
  auto request_context = static_cast<CefRequestContextImpl*>(
      CefAppManager::Get()->GetGlobalRequestContext().get());
  if (!request_context || !request_context->GetBrowserContext()) {
    LOG(ERROR) << "GetActiveProfile failed";
    return nullptr;
  }
  return request_context->GetBrowserContext()->AsProfile();
}
}  // namespace

bool ShouldUseBrowserPolicy(bool& should_use_browser_policy) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOhosAppApiVersion)) {
    LOG(ERROR) << "kOhosAppApiVersion not exist";
    return false;
  }
  std::string version_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOhosAppApiVersion);
  if (version_string.empty()) {
    return false;
  }
  int version = std::stoi(version_string);
  LOG(INFO) << "ShouldUseBrowserPolicy version: " << version;
  should_use_browser_policy = version >= kApiMinSdkVerison;
  return true;
}

// static
BrowserPolicyHandler* BrowserPolicyHandler::GetInstance() {
  static base::NoDestructor<BrowserPolicyHandler> instance;
  return instance.get();
}

void BrowserPolicyHandler::MaybeInitFromPersistentPrefs() {
  if (current_version_ == kInvalidPolicyVersion && GetActiveProfile()) {
    auto prefs = GetActiveProfile()->GetPrefs();
    if (!prefs) {
      LOG(ERROR) << "BrowserPolicyHandler MaybeInitFromPersistentPrefs "
                    "GetPrefs failed";
      return;
    }
    current_version_ =
        prefs->GetInteger(prefs::kBrowserPolicyVersion);
  }
}

void BrowserPolicyHandler::SetPolicyAndNotify(const std::string& policy,
                                              int version) {
  bool use_browser_policy = false;
  std::ignore = ShouldUseBrowserPolicy(use_browser_policy);
  if (!use_browser_policy) {
    LOG(ERROR) << "current api version does not support enterprise ";
    return;
  }

  if (!SetPolicy(policy, version)) {
    return;
  }
  for (Observer& observer : observers_) {
    observer.OnPolicyChanged();
  }
}

bool BrowserPolicyHandler::GetPolicy(std::string& policy) {
  if (policy_.empty()) {
    return false;
  }
  LOG(INFO) << "BrowserPolicyHandler policy read";

  policy = policy_;
  return true;
}

bool BrowserPolicyHandler::SetPolicy(const std::string& policy, int version) {
  MaybeInitFromPersistentPrefs();
  if (current_version_ == kInvalidPolicyVersion) {
    LOG(ERROR) << "BrowserPolicyHandler set policy failed: init failed";
    return false;
  }

  if (version <= current_version_) {
    LOG(WARNING) << "BrowserPolicyHandler set policy failed: version not newer";
    return false;
  }

  if (!GetActiveProfile()) {
    LOG(ERROR) << "BrowserPolicyHandler SetPolicy GetActiveProfile failed";
    return false;
  }
  auto prefs = GetActiveProfile()->GetPrefs();
  if (!prefs) {
    LOG(ERROR) << "BrowserPolicyHandler SetPolicy GetPrefs failed";
    return false;
  }

  current_version_ = version;
  policy_ = policy;

  prefs->SetInteger(prefs::kBrowserPolicyVersion, version);

  LOG(INFO) << "BrowserPolicyHandler new policy set";
  return true;
}

void BrowserPolicyHandler::AddObserver(Observer* observer) {
  if (observers_.HasObserver(observer)) {
    return;
  }

  LOG(INFO) << "BrowserPolicyHandler add new observer";
  observers_.AddObserver(observer);
}

void BrowserPolicyHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace policy
