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

#include "cef/ohos_cef_ext/libcef/browser/arkweb_download_url_parameters_util.h"

#include "cef/libcef/browser/download_manager_delegate.h"
#include "cef/libcef/browser/download_manager_delegate_impl.h"
#include "content/public/browser/download_request_utils.h"

namespace download_url_parameters_util {
void ParseDownloadUrlParamsIntoClass(
    const DownloadUrlParameters& input_params,
    std::unique_ptr<download::DownloadUrlParameters>& params) {
  if (params != nullptr) {
    // method
    if (!input_params.method.empty()) {
      params->set_method(input_params.method);
    }

    // postBody
    if (!input_params.postBody.empty()) {
      auto postBody = network::ResourceRequestBody::CreateFromBytes(
          input_params.postBody.c_str(),
          static_cast<size_t>(input_params.postBody.length()));
      params->set_post_body(postBody);
    }

    // headers
    std::string header_str = input_params.headers;
    size_t current_pos = 0;
    while (current_pos < header_str.size()) {
      size_t line_end = header_str.find("\r\n", current_pos);
      if (line_end == std::string::npos) {
        line_end = header_str.size();
      }
      std::string line = header_str.substr(current_pos, line_end - current_pos);

      if (line.empty()) {
        current_pos = line_end + 2;
        continue;
      }

      size_t colon_pos = line.find(':');
      if (colon_pos == std::string::npos || colon_pos == 0 ||
          colon_pos == line.size() - 1) {
        current_pos = line_end + 2;
        continue;
      }

      std::string header_name = line.substr(0, colon_pos);
      std::string header_value = line.substr(colon_pos + 1);

      base::TrimWhitespaceASCII(header_name, base::TRIM_ALL, &header_name);
      base::TrimWhitespaceASCII(header_value, base::TRIM_ALL, &header_value);

      if (!header_name.empty() && !header_value.empty() &&
          net::HttpUtil::IsValidHeaderName(header_name)) {
        params->add_request_header(header_name, header_value);
        LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set valid header: " << header_name
                   << " = " << header_value;
      } else {
        LOG(WARNING) << "[ARKWEB_DOWNLOADER] skip invalid header - name: '"
                     << header_name << "', value: '" << header_value << "'";
      }

      current_pos = line_end + 2;
    }

    // referrer
    if (!input_params.referrer.empty()) {
      params->set_referrer(GURL(input_params.referrer));
    }

    // referrerPolicy
    if (ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE <=
            input_params.referrerPolicy &&
        ReferrerPolicy::MAX >= input_params.referrerPolicy) {
      params->set_referrer_policy(
          static_cast<net::ReferrerPolicy>(input_params.referrerPolicy));
      LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set referrerPolicy = "
                 << static_cast<int>(params->referrer_policy());
    }

    // referrerEncoding
    if (!input_params.referrerEncoding.empty()) {
      params->set_referrer_encoding(input_params.referrerEncoding);
      LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set referrerEncoding = "
                 << params->referrer_encoding();
    }

    // initiator
    if (!input_params.initiator.empty()) {
      GURL initiator_gurl(input_params.initiator);
      auto initiator_org = url::Origin::Create(initiator_gurl);
      params->set_initiator(std::make_optional(initiator_org));
    }

    // preferCache
    params->set_prefer_cache(input_params.preferCache);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set preferCache = "
               << params->prefer_cache();

    // filePath
    if (!input_params.filePath.empty()) {
      params->set_file_path(base::FilePath(input_params.filePath));
      auto file_path_str = params->file_path().value();
      if (file_path_str.length() <= 2) {
        file_path_str = "**";
      } else {
        file_path_str = file_path_str.substr(0, 2) + "**";
      }
      LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set filePath = "
                 << file_path_str;
    }

    // suggestedName
    if (!input_params.suggestedName.empty()) {
      std::string utf8_name = std::string(input_params.suggestedName);
      std::u16string utf16_name = base::UTF8ToUTF16(utf8_name);
      params->set_suggested_name(utf16_name);
    }

    // offset
    if (input_params.offset >= 0) {
      params->set_offset(static_cast<int64_t>(input_params.offset));
      LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set offset = " << params->offset();
    }

    // crossOriginRedirects
    if (RedirectMode::kMinValue <= input_params.crossOriginRedirects &&
        RedirectMode::kMaxValue >= input_params.crossOriginRedirects) {
      params->set_cross_origin_redirects(
          static_cast<network::mojom::RedirectMode>(
              input_params.crossOriginRedirects));
      LOG(DEBUG) << "[ARKWEB_DOWNLOADER] crossOriginRedirects = "
                 << params->cross_origin_redirects();
    }

    // transient
    params->set_transient(input_params.transient);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set transient = "
               << params->is_transient();

    // guid
    if (!input_params.guid.empty()) {
      params->set_guid(input_params.guid);
    }

    // hasUserGesture
    params->set_has_user_gesture(input_params.hasUserGesture);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set hasUserGesture = "
               << params->has_user_gesture();
  }
}
} // namespace download_url_parameters_util