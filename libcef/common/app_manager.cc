// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/common/app_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "cef/libcef/common/net/scheme_info.h"
#include "cef/libcef/common/scheme_registrar_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(ARKWEB_CUSTOM_SCHEME_CODECACHE)
#include "libcef/common/net/scheme_registration.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/path_service.h"
#endif

#if BUILDFLAG(ARKWEB_SCHEME_HANDLER)
#include "base/json/json_reader.h"
#include "base/values.h"
#endif

namespace {

CefAppManager* g_manager = nullptr;

}  // namespace

// static
CefAppManager* CefAppManager::Get() {
  return g_manager;
}

CefAppManager::CefAppManager() {
  // Only a single instance should exist.
  DCHECK(!g_manager);
  g_manager = this;
}

CefAppManager::~CefAppManager() {
  g_manager = nullptr;
}

void CefAppManager::AddCustomScheme(const CefSchemeInfo* scheme_info) {
  DCHECK(!scheme_info_list_locked_);
  scheme_info_list_.push_back(*scheme_info);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kProcessType)) {
    // Register as a Web-safe scheme in the browser process so that requests for
    // the scheme from a render process will be allowed in
    // resource_dispatcher_host_impl.cc ShouldServiceRequest.
    content::ChildProcessSecurityPolicy* policy =
        content::ChildProcessSecurityPolicy::GetInstance();
    if (!policy->IsWebSafeScheme(scheme_info->scheme_name)) {
      policy->RegisterWebSafeScheme(scheme_info->scheme_name);
    }
  }

#if BUILDFLAG(ARKWEB_CUSTOM_SCHEME_CODECACHE)
  if (scheme_info->is_code_cache_enabled) {
    LOG(DEBUG) << "App manager register the scheme:"
               << scheme_info->scheme_name.c_str() << " supported code cache.";
    scheme::AddSchemesSupportCodeCache(scheme_info->scheme_name);
  }
#endif
}

bool CefAppManager::HasCustomScheme(const std::string& scheme_name) {
  DCHECK(scheme_info_list_locked_);
  if (scheme_info_list_.empty()) {
    return false;
  }

  SchemeInfoList::const_iterator it = scheme_info_list_.begin();
  for (; it != scheme_info_list_.end(); ++it) {
    if (it->scheme_name == scheme_name) {
      return true;
    }
  }

  return false;
}

const CefAppManager::SchemeInfoList* CefAppManager::GetCustomSchemes() {
  DCHECK(scheme_info_list_locked_);
  return &scheme_info_list_;
}

void CefAppManager::AddAdditionalSchemes(
    content::ContentClient::Schemes* schemes) {
  DCHECK(!scheme_info_list_locked_);

  auto application = GetApplication();
  if (application) {
    CefSchemeRegistrarImpl schemeRegistrar;
    application->OnRegisterCustomSchemes(&schemeRegistrar);
    schemeRegistrar.GetSchemes(schemes);
  }

#if BUILDFLAG(ARKWEB_SCHEME_HANDLER)
  {
    CefSchemeRegistrarImpl schemeRegistrar;
    base::CommandLine* cmdLine = base::CommandLine::ForCurrentProcess();
    if (cmdLine && cmdLine->HasSwitch(switches::kOhSchemeHandlerCustomScheme)) {
      std::string cmdlineSchemes =
          cmdLine->GetSwitchValueASCII(switches::kOhSchemeHandlerCustomScheme);
      LOG(INFO) << "scheme_handler cmdlineScheme:" << cmdlineSchemes;
      std::optional<base::Value> schemeInfos =
          base::JSONReader::Read(cmdlineSchemes);
      if (schemeInfos && schemeInfos->is_dict()) {
        for (auto kv : schemeInfos->GetDict()) {
          schemeRegistrar.AddCustomScheme(kv.first, kv.second.GetInt());
        }
      }
    }
    schemeRegistrar.GetSchemes(schemes);
  }
#endif

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  RenderAddCustomSchemes();
#endif

  scheme_info_list_locked_ = true;
}

#if BUILDFLAG(IS_WIN)
const wchar_t* CefAppManager::GetResourceDllName() {
  static wchar_t file_path[MAX_PATH + 1] = {0};

  if (file_path[0] == 0) {
    // Retrieve the module path (usually libcef.dll).
    base::FilePath module;
    base::PathService::Get(base::FILE_MODULE, &module);
    const std::wstring wstr = module.value();
    size_t count = std::min(static_cast<size_t>(MAX_PATH), wstr.size());
    wcsncpy(file_path, wstr.c_str(), count);
    file_path[count] = 0;
  }

  return file_path;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
std::vector<std::string> CefAppManager::CustomSchemeCmdLineSplit(
    std::string str,
    const char split) {
  std::istringstream inStream(str);
  std::vector<std::string> ret;
  std::string token;
  while (getline(inStream, token, split)) {
    if (!token.empty()) {
      ret.push_back(token);
      token.clear();
    }
  }
  return ret;
}

void CefAppManager::RenderAddCustomSchemes() {
  CefRefPtr<CefCommandLine> cmdLine = CefCommandLine::GetGlobalCommandLine();
  if (cmdLine->HasSwitch(switches::kProcessType)) {
    LOG(INFO) << "render Add custom schemes";
    if (cmdLine->HasSwitch(switches::kOhosCustomScheme)) {
      std::string cmdlineSchemes =
          cmdLine->GetSwitchValue(switches::kOhosCustomScheme).ToString();
      LOG(INFO) << "render cmdlineScheme:" << cmdlineSchemes;
      std::vector<std::string> schemesInfo =
          CustomSchemeCmdLineSplit(cmdlineSchemes, ';');
      for (auto it = schemesInfo.begin(); it != schemesInfo.end(); ++it) {
        std::string schemeSplit = *it;
        std::vector<std::string> scheme =
            CustomSchemeCmdLineSplit(schemeSplit, ',');
        if (scheme.size() != 3) {
          break;
        }
        CefSchemeInfo regScheme = {"",
                                   false,
                                   false,
                                   false,
                                   false,
                                   false,
                                   false,
                                   false
#if BUILDFLAG(ARKWEB_NETWORK_BASE) || BUILDFLAG(ARKWEB_CUSTOM_SCHEME_CODECACHE)
                                   ,
                                   false
#endif
        };
        regScheme.scheme_name = scheme[0];
        if (scheme[1] == std::string("1")) {
          regScheme.is_cors_enabled = true;
        }
        if (scheme[2] == std::string("1")) {
          regScheme.is_fetch_enabled = true;
        }
        LOG(INFO) << "render add custom scheme name:" << regScheme.scheme_name
                  << " is_cors:" << regScheme.is_cors_enabled
                  << " is_fetch:" << regScheme.is_fetch_enabled;
        AddCustomScheme(&regScheme);
      }
    }
  }
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_LOAD)
