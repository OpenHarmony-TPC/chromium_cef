// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_CEF_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_CEF_H_

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/net/base/net_helpers.h"

#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
#include "cef/include/cef_download_handler.h"

namespace net_service {

#define NETHELPERSCEF_EXPORT __attribute__((visibility("default")))

class NETHELPERSCEF_EXPORT NetHelpersCef {
 public:
  static void SetDownloadHandler(
      CefRefPtr<CefDownloadHandler> download_handler);
  static CefRefPtr<CefDownloadHandler> GetDownloadHandler();

  static CefRefPtr<CefDownloadHandler> global_download_handler;
};

}  // namespace net_service

#endif

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_NET_HELPERS_CEF_H_
