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

#ifndef CEF_LIBCEF_COMMON_NET_SSL_INFO_UTIL_EX_H_
#define CEF_LIBCEF_COMMON_NET_SSL_INFO_UTIL_EX_H_
#pragma once

#include "cef/include/cef_ssl_info.h"
#include "cef/include/internal/cef_ptr.h"

namespace ssl_info_util {

std::string GetIssuerDisplayName(CefRefPtr<CefSSLInfo> ssl_info);
std::string GetValidExpiry(CefRefPtr<CefSSLInfo> ssl_info);

}  // namespace ssl_info_util

#endif  // CEF_LIBCEF_COMMON_NET_SSL_INFO_UTIL_EX_H_
