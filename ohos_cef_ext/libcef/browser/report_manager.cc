// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/report_manager.h"

#include "content/public/browser/render_process_host.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/origin_whitelist_impl.h"
#if BUILDFLAG(ARKWEB_CRASHPAD)
#include "libcef/browser/thread_util.h"
#endif
#include "libcef/common/frame_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "res_reporter.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/res_sched_client_adapter.h"

CefReportManager::CefReportManager(int render_process_id)
    : render_process_id_(render_process_id) {}

CefReportManager::~CefReportManager() = default;

// static
void CefReportManager::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
#if BUILDFLAG(ARKWEB_CRASHPAD)
  CEF_REQUIRE_UIT();
#endif
  registry->AddInterface(base::BindRepeating(
      [](int render_process_id,
         mojo::PendingReceiver<cef::mojom::ReportManager> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<CefReportManager>(render_process_id),
            std::move(receiver));
      },
      static_cast<int>(host->GetID())));
}

void CefReportManager::ReportKeyThread(int status,
                                       int process_id,
                                       int thread_id,
                                       int role) {
#if BUILDFLAG(ARKWEB_CRASHPAD)
  CEF_REQUIRE_UIT();
#endif
  LOG(DEBUG) << "ReportKeyThread process_id:" << process_id
             << ", thread_id:" << thread_id
             << ", render_process_id:" << render_process_id_;

  OHOS::NWeb::ResSchedClientAdapter::ReportKeyThread(
      static_cast<OHOS::NWeb::ResSchedStatusAdapter>(status), process_id,
      thread_id, static_cast<OHOS::NWeb::ResSchedRoleAdapter>(role));
}

void CefReportManager::AddRtg(const std::vector<int>& tids) {
#if BUILDFLAG(ARKWEB_CRASHPAD)
  CEF_REQUIRE_UIT();
#endif
  LOG(DEBUG) << "AddRtg";
  ResReporter::GetInstance().AddRtg(tids);
}

void CefReportManager::FetchBegin() {
#if BUILDFLAG(ARKWEB_CRASHPAD)
  CEF_REQUIRE_UIT();
#endif
  LOG(DEBUG) << "FetchBegin";
  ResReporter::GetInstance().FetchBegin();
}

void CefReportManager::FetchEnd() {
#if BUILDFLAG(ARKWEB_CRASHPAD)
  CEF_REQUIRE_UIT();
#endif
  LOG(DEBUG) << "FetchEnd";
  ResReporter::GetInstance().FetchEnd();
}
 