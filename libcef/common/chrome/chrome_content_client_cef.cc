// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/common/chrome/chrome_content_client_cef.h"

#include "cef/libcef/common/app_manager.h"
#include "chrome/common/media/cdm_registration.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "cef/libcef/common/cdm_host_file_path.h"
#endif
#endif
 
#if BUILDFLAG(ENABLE_WISEPLAY)
#include "base/logging.h"
#include "content/public/common/cdm_info.h"
#include "media/base/video_codecs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/wiseplay/cdm/wiseplay_cdm_common.h"
using Robustness = content::CdmInfo::Robustness;
#endif

#if BUILDFLAG(IS_OHOS)
#include "cef/libcef/common/ohos_cef_media_drm_bridge_client.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_WISEPLAY)
void AddSoftwareSecureWisePlay(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;
  LOG(INFO) << "WiseplayDRM | " << __FUNCTION__ << " | " << cdms->size();
  cdms->emplace_back(kWisePlayKeySystem, Robustness::kSoftwareSecure,
                     absl::nullopt, false, kWisePlayCdmDisplayName,
                     kWisePlayCdmType, base::Version(),
                     base::FilePath()); /*supports_sub_key_systems=false*/
}

void AddHardwareSecureWisePlay(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;
  LOG(INFO) << "WiseplayDRM | " << __FUNCTION__ << " | " << cdms->size();
  cdms->emplace_back(kWisePlayKeySystem, Robustness::kHardwareSecure,
                     absl::nullopt, false, kWisePlayCdmDisplayName,
                     kWisePlayCdmType, base::Version(),
                     base::FilePath()); /*supports_sub_key_systems=false*/
}

void AddWisePlay(std::vector<content::CdmInfo>* cdms) {
  LOG(INFO) << "WiseplayDRM | " << __FUNCTION__ << " | " << cdms->size();
  AddSoftwareSecureWisePlay(cdms);
  AddHardwareSecureWisePlay(cdms);
}
#endif  // BUILDFLAG(ENABLE_WISEPLAY)

}  // namespace

ChromeContentClientCef::ChromeContentClientCef() = default;
ChromeContentClientCef::~ChromeContentClientCef() = default;

void ChromeContentClientCef::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
    RegisterCdmInfo(cdms);
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (cdm_host_file_paths) {
    cef::AddCdmHostFilePaths(cdm_host_file_paths);
  }
#endif
#endif

#if BUILDFLAG(ENABLE_WISEPLAY)
  if (cdms) {
    AddWisePlay(cdms);
  }
#endif
}

void ChromeContentClientCef::AddAdditionalSchemes(Schemes* schemes) {
  ChromeContentClient::AddAdditionalSchemes(schemes);
  CefAppManager::Get()->AddAdditionalSchemes(schemes);
}

#if BUILDFLAG(ENABLE_WISEPLAY)
media::OhosMediaDrmBridgeClient* ChromeContentClientCef::GetOhosMediaDrmBridgeClient() {
  LOG(INFO) << "WiseplayDRM | " << __FUNCTION__;
  return new OhosCefMediaDrmBridgeClient();
}
#endif  // BUILDFLAG(ENABLE_WISEPLAY)
