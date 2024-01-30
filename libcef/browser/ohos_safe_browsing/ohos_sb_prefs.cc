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

#include "libcef/browser/ohos_safe_browsing/ohos_sb_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace ohos_safe_browsing {

const char kMaliciousAllowList[] = "oh.safe_browsing.malicious_allowlist";

const char kIncognitoMaliciousAllowList[] = "oh.safe_browsing.incognito_malicious_allowlist";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kMaliciousAllowList);
  registry->RegisterListPref(kIncognitoMaliciousAllowList);
}

}  // namespace ohos_safe_browsing
