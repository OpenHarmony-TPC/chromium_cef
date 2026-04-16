// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_restrictions.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/trace_subscriber.h"
#include "cef/libcef/common/cef_switches.h"
#include "components/download/public/common/download_item.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
// #include "content/public/browser/notification_service.h"
// #include "content/public/browser/notification_types.h"
#include "arkweb/build/features/features.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(ARKWEB_HTTP_DNS)
#include "cef/ohos_cef_ext/libcef/browser/net_service/net_helpers.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_service.mojom.h"
#endif

#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
#include "cef/libcef/browser/download_item_impl.h"
#include "cef/libcef/browser/download_manager_delegate.h"
#include "cef/libcef/browser/download_manager_delegate_impl.h"
#include "cef/ohos_cef_ext/libcef/browser/arkweb_download_resume_util.h"
#include "cef/ohos_cef_ext/libcef/browser/arkweb_received_slice_helper_ext.h"
#include "cef/ohos_cef_ext/libcef/browser/cef_download_item_impl_ext.h"
#include "cef/ohos_cef_ext/libcef/browser/useragent/arkweb_useragent_utils.h"
#include "components/download/public/common/download_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/download_request_utils.h"
#endif

void CefApplyHttpDns() {
#if BUILDFLAG(ARKWEB_HTTP_DNS)
  if (!net_service::NetHelpers::HasValidDnsOverHttpConfig()) {
    LOG(WARNING) << __func__ << " User input mal mode:"
                 << net_service::NetHelpers::doh_mode;
    return;
  }

  network::mojom::NetworkService* network_service =
      content::GetNetworkService();
  if (network_service) {
    auto config = net::DnsOverHttpsServerConfig::FromString(
        net_service::NetHelpers::DnsOverHttpServerConfig());
    if (config.has_value()) {
      network_service->ConfigureStubHostResolver(
          true, net_service::NetHelpers::DnsOverHttpMode(),
          net::DnsOverHttpsConfig({{std::move(*config)}}), true);
    } else {
      LOG(INFO) << __func__ << "server config is invalid";
    }
  } else {
    LOG(INFO) << __func__
              << "will apply doh config after network service created";
  }
#endif  // BUILDFLAG(ARKWEB_HTTP_DNS)
}

void CefSetDownloadHandler(CefRefPtr<CefDownloadHandler> download_handler) {
#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
  LOG(INFO) << "set global download_handler";
  net_service::NetHelpers::SetDownloadHandler(std::move(download_handler));
#endif  //  ARKWEB_EX_DOWNLOAD
}

#if BUILDFLAG(ARKWEB_EXT_DOWNLOAD)
CefRefPtr<CefDownloadItem> CefGetDownloadItem(const std::string& guid) {
  LOG(DEBUG) << "get download item for " << guid;
  for (auto& context : CefBrowserContext::GetAll()) {
    content::DownloadManager* manager =
        context->AsBrowserContext()->GetDownloadManager();
    if (!manager) {
      continue;
    }
    download::DownloadItem* item = manager->GetDownloadByGuid(guid);
    if (item) {
      CefRefPtr<CefDownloadItemImplExt> download_item(
          new CefDownloadItemImplExt(item));
      return download_item;
    }
  }
  return nullptr;
}
#endif

void CefResumeDownload(const CefString& guid,
                       const std::vector<CefString>& url_chain,
                       const CefString& referrer_url,
                       const CefString& full_path,
                       int64_t received_bytes,
                       int64_t total_bytes,
                       const CefString& etag,
                       const CefString& mime_type,
                       const CefString& last_modified,
                       const CefString& received_slices_string) {
#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
  std::vector<CefBrowserContext*> browser_context_all =
      CefBrowserContext::GetAll();
  if (browser_context_all.size() > 0) {
    //  use first browser context to resume
    CefBrowserContext* context = browser_context_all[0];
    content::BrowserContext* browser_context = context->AsBrowserContext();
    content::DownloadManager* manager = browser_context->GetDownloadManager();
    if (!manager) {
      LOG(ERROR) << "download manager not exists, resume download failed";
      return;
    }
    std::vector<GURL> gurl_url_chain;
    for (auto& url : url_chain) {
      GURL gurl = GURL(url.ToString());
      if (gurl.is_empty() || !gurl.is_valid()) {
        LOG(ERROR) << "download url is not valid, resume download failed";
        return;
      }
      gurl_url_chain.push_back(gurl);
    }
    base::FilePath file_full_path =
        base::FilePath::FromUTF8Unsafe(full_path.ToString());

    std::vector<download::DownloadItem::ReceivedSlice> received_slices =
        arkweb_received_slice_helper_ext::FromString(
            received_slices_string.ToString());
    manager->GetNextId(base::BindOnce(
        &download_resume_util::ResumeDownloadWithId, manager, guid.ToString(), std::move(gurl_url_chain),
        GURL(referrer_url.ToString()), std::move(file_full_path), received_bytes, total_bytes,
        etag.ToString(), mime_type.ToString(), last_modified.ToString(),
        received_slices));
  } else {
    LOG(ERROR) << "browser contexts is empty, resume download failed";
  }
#endif  //  BUILDFLAG(ARKWEB_EX_DOWNLOAD)
}

void CefSetFileRenameOption(const int file_rename_option) {
#if BUILDFLAG(ARKWEB_EX_DOWNLOAD)
  SetFileRenameOptions((download::FileRenameOptions)file_rename_option);
#endif  //  BUILDFLAG(ARKWEB_EX_DOWNLOAD)
}

void CefOnReadDownloadDataDoneOnUIThread(const std::string& guid,
                                         CefRefPtr<CefReadDownloadDataCallback> callback,
                                         const std::vector<uint8_t>& buffer) {
  if (buffer.size() == 0) {
    LOG(INFO) << "OnReadDownloadDataDone: buffer is nullptr.";
    callback->OnReadDownloadDataDone(guid, nullptr);
    return;
  }
  LOG(INFO) << "OnReadDownloadDataDone: buffer size: " << buffer.size();
  CefRefPtr<CefBinaryValue> cefBuffer =
          CefBinaryValue::Create(buffer.data(), buffer.size());
  callback->OnReadDownloadDataDone(guid, cefBuffer);
}

void CefOnReadDownloadDataDone(const std::string& guid,
                               CefRefPtr<CefReadDownloadDataCallback> callback,
                               const std::vector<uint8_t>& buffer) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(CefOnReadDownloadDataDoneOnUIThread, guid, callback, buffer));
    return;
  }
  CefOnReadDownloadDataDoneOnUIThread(guid, callback, buffer);
}

void CefReadDownloaData(const std::string& guid,
                        const int32_t read_size,
                        CefRefPtr<CefReadDownloadDataCallback> callback) {
  LOG(INFO) << "CefReadDownloaData start guid: " << guid
            << ",read_size: " << read_size;
  for (auto& context: CefBrowserContext::GetAll()) {
    content::DownloadManager* manager = context->AsBrowserContext()->GetDownloadManager();
    if (!manager) {
      continue;
    }
    download::DownloadItem* item = manager->GetDownloadByGuid(guid);
    if (item) {
      LOG(INFO) << "CefReadDownloaData item exist";
      item->ReadDownloadData(guid, read_size,
                           base::BindOnce(&CefOnReadDownloadDataDone, guid, callback));
      return;
    }
  }
  callback->OnReadDownloadDataDone(guid, nullptr);
}

void ParseDownloadUrlParamsIntoClass(
    const DownloadUrlParameters& input_params,
    std::unique_ptr<download::DownloadUrlParameters>& params) {
  if (params != nullptr) {
    // method
    params->set_method(input_params.method);
 
    // postBody
    auto postBody = network::ResourceRequestBody::CreateFromBytes(
        input_params.postBody.c_str(),
        static_cast<size_t>(input_params.postBody.length()));
    params->set_post_body(postBody);
 
    // headers
    std::string header_str = input_params.headers;
    size_t current_pos = 0;
    while (current_pos < header_str.size()) {
      size_t line_end = header_str.find("\r\n", current_pos);
      if (line_end == std::string::npos) {
        line_end = header_str.size();
      }
      std::string line = header_str.substr(current_pos, line_end - current_pos);
 
      if (line.empty()) {
        current_pos = line_end + 2;
        continue;
      }
 
      size_t colon_pos = line.find(':');
      if (colon_pos == std::string::npos || colon_pos == 0 ||
          colon_pos == line.size() - 1) {
        current_pos = line_end + 2;
        continue;
      }
 
      std::string header_name = line.substr(0, colon_pos);
      std::string header_value = line.substr(colon_pos + 1);
 
      base::TrimWhitespaceASCII(header_name, base::TRIM_ALL, &header_name);
      base::TrimWhitespaceASCII(header_value, base::TRIM_ALL, &header_value);
 
      if (!header_name.empty() && !header_value.empty() &&
          net::HttpUtil::IsValidHeaderName(header_name)) {
        params->add_request_header(header_name, header_value);
        LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set valid header: " << header_name
                   << " = " << header_value;
      } else {
        LOG(WARNING) << "[ARKWEB_DOWNLOADER] skip invalid header - name: '"
                     << header_name << "', value: '" << header_value << "'";
      }
 
      current_pos = line_end + 2;
    }
 
    // referrer
    params->set_referrer(GURL(input_params.referrer));
 
    // referrerPolicy
    params->set_referrer_policy(
        static_cast<net::ReferrerPolicy>(input_params.referrerPolicy));
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set referrerPolicy = "
               << static_cast<int>(params->referrer_policy());
 
    // referrerEncoding
    params->set_referrer_encoding(input_params.referrerEncoding);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set referrerEncoding = "
               << params->referrer_encoding();
 
    // initiator
    GURL initiator_gurl(input_params.initiator);
    auto initiator_org = url::Origin::Create(initiator_gurl);
    params->set_initiator(std::make_optional(initiator_org));
 
    // preferCache
    params->set_prefer_cache(input_params.preferCache);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set preferCache = "
               << params->prefer_cache();
 
    // filePath
    params->set_file_path(base::FilePath(input_params.filePath));
    auto file_path_str = params->file_path().value();
    if (file_path_str.length() <= 2) {
      file_path_str = "**";
    } else {
      file_path_str = file_path_str.substr(0, 2) + "**";
    }
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set filePath = "
               << file_path_str;
 
    // suggestedName
    std::string utf8_name = std::string(input_params.suggestedName);
    std::u16string utf16_name = base::UTF8ToUTF16(utf8_name);
    params->set_suggested_name(utf16_name);
 
    // offset
    params->set_offset(static_cast<int64_t>(input_params.offset));
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set offset = " << params->offset();
 
    // crossOriginRedirects
    params->set_cross_origin_redirects(
        static_cast<network::mojom::RedirectMode>(
            input_params.crossOriginRedirects));
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] crossOriginRedirects = "
               << params->cross_origin_redirects();
 
    // transient
    params->set_transient(input_params.transient);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set transient = "
               << params->is_transient();
 
    // guid
    params->set_guid(input_params.guid);
 
    // hasUserGesture
    params->set_has_user_gesture(input_params.hasUserGesture);
    LOG(DEBUG) << "[ARKWEB_DOWNLOADER] set hasUserGesture = "
               << params->has_user_gesture();
  }
}
 
void CefStartDownload(const CefString& url,
                      const DownloadUrlParameters& input_params) {
  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid()) {
    return;
  }
 
  std::vector<CefBrowserContext*> browser_context_all =
      CefBrowserContext::GetAll();
  if (browser_context_all.size() <= 0) {
    LOG(ERROR) << "browser contexts is empty, resume download failed";
    return;
  }
  CefBrowserContext* context = browser_context_all[0];
  content::BrowserContext* browser_context = context->AsBrowserContext();
  content::DownloadManager* manager = browser_context->GetDownloadManager();
  if (!manager) {
    LOG(ERROR) << "download manager not exists, resume download failed";
    return;
  }
 
  std::unique_ptr<download::DownloadUrlParameters> params =
      std::make_unique<download::DownloadUrlParameters>(gurl, MISSING_TRAFFIC_ANNOTATION);
 
  ParseDownloadUrlParamsIntoClass(input_params, params);
 
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  params->add_request_header(net::HttpRequestHeaders::kUserAgent,
                             arkweb_useragent_utils::GetUAStringForHost(nullptr, gurl.host()));
#endif
 
  manager->DownloadUrl(std::move(params));
}
