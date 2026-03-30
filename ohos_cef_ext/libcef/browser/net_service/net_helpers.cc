// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net_service/net_helpers.h"

namespace net_service {

#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
CefRefPtr<CefDownloadHandler> NetHelpersCef::global_download_handler = nullptr;
void NetHelpersCef::SetDownloadHandler(
    CefRefPtr<CefDownloadHandler> download_handler) {
    NetHelpersCef::global_download_handler = download_handler;
  }
  CefRefPtr<CefDownloadHandler> NetHelpersCef::GetDownloadHandler() {
  return NetHelpersCef::global_download_handler;
}
#endif

}  // namespace net_service
