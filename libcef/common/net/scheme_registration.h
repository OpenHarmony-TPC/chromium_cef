// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#define CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#pragma once

#include <string>
#include <vector>

#include "arkweb/build/features/features.h"

namespace scheme {

// Returns true if the specified |scheme| is handled internally.
bool IsInternalHandledScheme(const std::string& scheme);

// Returns true if the specified |scheme| is a registered standard scheme.
bool IsStandardScheme(const std::string& scheme);

// Returns true if the specified |scheme| is a registered CORS enabled scheme.
bool IsCorsEnabledScheme(const std::string& scheme);

#if BUILDFLAG(ARKWEB_CUSTOM_SCHEME_CODECACHE)
// Add schemes that support code cache.
void AddSchemesSupportCodeCache(const std::string& scheme);
#endif
}  // namespace scheme

#endif  // CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
