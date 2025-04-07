// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_INCLUDE_CEF_VERSION_H_
#define CEF_INCLUDE_CEF_VERSION_H_

#define CEF_VERSION "126.0.0-2024092502.37+g6266d48+chromium-126.0.6478.36"
#define CEF_VERSION_MAJOR 126
#define CEF_VERSION_MINOR 0
#define CEF_VERSION_PATCH 0
#define CEF_COMMIT_NUMBER 37
#define CEF_COMMIT_HASH "6266d48a61e0819c87c4423ed9e6fd2901115875"
#define COPYRIGHT_YEAR 2024

#define CHROME_VERSION_MAJOR 126
#define CHROME_VERSION_MINOR 0
#define CHROME_VERSION_BUILD 6478
#define CHROME_VERSION_PATCH 36

#define DO_MAKE_STRING(p) #p
#define MAKE_STRING(p) DO_MAKE_STRING(p)

#ifndef APSTUDIO_HIDDEN_SYMBOLS

#include "include/internal/cef_export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns CEF version information for the libcef library. The |entry|
// parameter describes which version component will be returned:
// 0 - CEF_VERSION_MAJOR
// 1 - CEF_VERSION_MINOR
// 2 - CEF_VERSION_PATCH
// 3 - CEF_COMMIT_NUMBER
// 4 - CHROME_VERSION_MAJOR
// 5 - CHROME_VERSION_MINOR
// 6 - CHROME_VERSION_BUILD
// 7 - CHROME_VERSION_PATCH
///
CEF_EXPORT int cef_version_info(int entry);

#ifdef __cplusplus
}
#endif

#endif  // APSTUDIO_HIDDEN_SYMBOLS

#endif  // CEF_INCLUDE_CEF_VERSION_H_
