// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tests/shared/browser/resource_util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "include/base/cef_logging.h"
#include "include/cef_path_util.h"
#include "include/internal/cef_types.h"

namespace client {

bool GetResourceDir(std::string& dir) {
  CefString ceftests_files;
  if (CefGetPath(PK_DIR_RESOURCES, ceftests_files) && !ceftests_files.empty()) {
    dir = std::string(ceftests_files.ToString()) + "/ceftests_files";
    return true;
  } else {
    return false;
  }
}

}  // namespace client
