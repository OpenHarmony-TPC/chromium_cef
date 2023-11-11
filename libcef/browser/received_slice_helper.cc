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

#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "components/download/public/common/download_item.h"
#include "include/capi/cef_base_capi.h"
#include "libcef/browser/received_slice_helper.h"

namespace received_slice_helper {

std::vector<download::DownloadItem::ReceivedSlice> FromString(
    const std::string& input_str) {
  LOG(INFO) << "parse received slice from " << input_str;
  std::vector<download::DownloadItem::ReceivedSlice> received_slices;
  if (input_str.length() == 0) {
    return received_slices;
  }

  std::vector<std::string> slices = base::SplitString(
      input_str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  LOG(INFO) << "received slice size: " << slices.size();
  for (auto& slice : slices) {
    if (slice.empty()) {
      continue;
    }
    std::vector<std::string> slice_pairs = base::SplitString(
        slice, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    int64_t offset = 0;
    int64_t received_bytes = 0;
    int finished = 0;
    if (base::StringToInt64(slice_pairs[0], &offset) &&
        base::StringToInt64(slice_pairs[1], &received_bytes) &&
        base::StringToInt(slice_pairs[2], &finished)) {
      auto slice = download::DownloadItem::ReceivedSlice(offset, received_bytes,
                                                         !!finished);
      received_slices.push_back(slice);
    } else {
      received_slices.clear();
      return received_slices;
    }
  }
  return received_slices;
}

std::string SerializeToString(
    std::vector<download::DownloadItem::ReceivedSlice> slices) {
  std::string slices_str;
  size_t slice_index = 0;
  for (auto slice : slices) {
    slices_str += base::NumberToString(slice.offset);
    slices_str += ",";
    slices_str += base::NumberToString(slice.received_bytes);
    slices_str += ",";
    slices_str += base::NumberToString(slice.finished ? 1 : 0);
    slice_index++;
    if (slice_index != slices.size()) {
      slices_str += ";";
    }
  }
  return slices_str;
}
}  //  namespace received_slice_helper
