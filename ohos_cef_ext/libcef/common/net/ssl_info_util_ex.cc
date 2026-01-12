/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "cef/ohos_cef_ext/libcef/common/net/ssl_info_util_ex.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"

namespace ssl_info_util {

std::string GetIssuerDisplayName(CefRefPtr<CefSSLInfo> ssl_info) {
  if (!ssl_info || !ssl_info->GetX509Certificate() ||
      !ssl_info->GetX509Certificate()->GetIssuer()) {
    return std::string("noIssuer");
  }
  return ssl_info->GetX509Certificate()
      ->GetIssuer()
      ->GetDisplayName()
      .ToString();
}

std::string GetValidExpiry(CefRefPtr<CefSSLInfo> ssl_info) {
  if (!ssl_info || !ssl_info->GetX509Certificate()) {
    return std::string("noValidExpiry");
  }
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(
      ssl_info->GetX509Certificate()->GetValidExpiry()));
}

}  // namespace ssl_info_util
