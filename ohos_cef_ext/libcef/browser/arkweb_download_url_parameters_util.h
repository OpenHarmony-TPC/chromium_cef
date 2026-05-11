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

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_URL_PARAMETERS_UTIL_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_URL_PARAMETERS_UTIL_H_
#pragma once

#include "ohos_nweb/src/capi/browser_service/nweb_extension_downloader_types.h"
#include "components/download/public/common/download_utils.h"

namespace download_url_parameters_util {
void ParseDownloadUrlParamsIntoClass(
    const DownloadUrlParameters& input_params,
    std::unique_ptr<download::DownloadUrlParameters>& params);
} // namespace download_url_parameters_util

#endif // CEF_LIBCEF_BROWSER_DOWNLOAD_URL_PARAMETERS_UTIL_H_