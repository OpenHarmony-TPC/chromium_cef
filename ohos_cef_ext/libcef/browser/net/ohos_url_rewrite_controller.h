// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENABLE_REWRITE_URL_UTIL_H_
#define ENABLE_REWRITE_URL_UTIL_H_

class OhosUrlRewriteController {
 public:
  static bool IsRewriteUrlEnabled();
  static void EnableRewriteUrl(bool enable);

 private:
    static bool is_rewrite_url_enabled_;
};

#endif // ENABLE_REWRITE_URL_UTIL_H_