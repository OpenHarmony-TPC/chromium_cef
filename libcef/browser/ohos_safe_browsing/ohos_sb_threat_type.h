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

#ifndef CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_THREAT_TYPE_H_
#define CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_THREAT_TYPE_H_

namespace ohos_safe_browsing {

// The type of block page
enum OHSBThreatType {
  THREAT_DEFAULT = -1,
  THREAT_ILLEGAL = 0,
  THREAT_FRAUD = 1,
  THREAT_RISK = 2,
  THREAT_WARNING = 3,
  THREAT_URL_TRUST_LIST = 4,
};

enum OHSBPolicyType {
  POLICY_NO_PROMPT = 0,
  POLICY_DANGER_LABEL = 1,
  POLICY_POPUP_AND_DANGER = 2,
  POLICY_FORBIDDEN_PROHIBIT_ACCESS = 3,
  POLICY_CHILD_MODE_PROHIBIT_ACCESS = 4,
  POLICY_CUSTOMIZE_JUMP = 5,
  POLICY_HALF_POPUP = 10,
};

}  // namespace ohos_safe_browsing

#endif  // CEF_LIBCEF_BROWSER_OHOS_SAFE_BROWSING_OHOS_SB_THREAT_TYPE_H_
