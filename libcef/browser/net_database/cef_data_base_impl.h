// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_DATA_BASE_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_DATA_BASE_IMPL_H_

#include "include/cef_data_base.h"
#include "libcef/browser/browser_context.h"

class CefDataBaseImpl : public CefDataBase {
 public:
  enum CefPermissionType { GEOLOCATION_TYPE };
  CefDataBaseImpl() = default;

  CefDataBaseImpl(const CefDataBaseImpl&) = delete;
  CefDataBaseImpl& operator=(const CefDataBaseImpl&) = delete;

  bool ExistHttpAuthCredentials() override;

  void DeleteHttpAuthCredentials() override;

  void SaveHttpAuthCredentials(const CefString& host,
                               const CefString& realm,
                               const CefString& username,
                               const char* password) override;

  void GetHttpAuthCredentials(
      const CefString& host,
      const CefString& realm,
      std::vector<CefString>& username_password) override;

  bool ExistPermissionByOrigin(const CefString& origin, int type) override;

  bool GetPermissionResultByOrigin(const CefString& origin,
                                   int type,
                                   bool& result) override;

  void SetPermissionByOrigin(const CefString& origin,
                             int type,
                             bool result) override;

  void ClearPermissionByOrigin(const CefString& origin, int type) override;

  void ClearAllPermission(int type) override;

  void GetOriginsByPermission(int type,
                              std::vector<CefString>& origins) override;

 private:
  IMPLEMENT_REFCOUNTING(CefDataBaseImpl);
};
#endif  // CEF_LIBCEF_BROWSER_NET_DATA_BASE_IMPL_H_
