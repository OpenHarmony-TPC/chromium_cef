// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/permission/alloy_access_request.h"

AlloyAccessRequest::AlloyAccessRequest(const CefString& origin,
                                       int resources,
                                       cef_permission_callback_t callback)
    : origin_(origin), resources_(resources), callback_(std::move(callback)) {}

AlloyAccessRequest::~AlloyAccessRequest() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(false);
  }
}

CefString AlloyAccessRequest::Origin() {
  return origin_;
}

int AlloyAccessRequest::ResourceAcessId() {
  return resources_;
}

void AlloyAccessRequest::ReportRequestResult(bool allowed) {
  if (!callback_.is_null()) {
    std::move(callback_).Run(allowed);
  }
}