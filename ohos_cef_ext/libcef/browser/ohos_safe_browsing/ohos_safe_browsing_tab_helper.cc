// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/ohos_safe_browsing/ohos_safe_browsing_tab_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "crypto/sha2.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/net_service/url_loader_factory_getter.h"
#include "libcef/browser/ohos_safe_browsing/ohos_safe_browsing_response.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_prefs.h"
#include "libcef/browser/ohos_safe_browsing/ohos_sb_threat_type.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ohos_safe_browsing {

namespace {
constexpr size_t kHashPrefixLength = 6;  // bytes
constexpr char kPackageName[] = "packageName";
constexpr char kUrl[] = "url";
constexpr char kDomain[] = "domain";
constexpr char kUrlHashPrefix[] = "urlHashPrefix";
constexpr char kTpCheckSwitch[] = "tpCheckSwitch";
constexpr char kContentTypeJSON[] = "application/json";
constexpr char kXHwGrsAttrGroupHeader[] = "X-HW-GRS-ATTR-GROUP";
constexpr char kSerLocation[] = "ser-location=CN";

#if defined(CLOUD_ENV)
const char kSafeSearchApiUrl[] =
    CLOUD_ENDPOINT "/security/v1/oh/securityurls" CLOUD_GRAYSCALE_QUERY;
#else
const char kSafeSearchApiUrl[] =
    "https://browsercfg-drcn.cloud.dbankcloud.cn/security/v1/oh/securityurls";
#endif  // defined(CLOUD_ENV)
const char kReplaceStr[] = "***";

constexpr net::NetworkTrafficAnnotationTag kSafeBrowsingTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("ohos-safe-browsing", R"(
          semantics {
            sender: "ohos-safe-browsing check"
            description:
              ""
            trigger:
              ""
            data: ""
            destination: OTHER
          }
          policy {
            cookies_allowed: YES
            setting:
              ""
            policy_exception_justification:
              ""
          })");

content::BrowserContext* GetBrowserContext() {
  std::vector<CefBrowserContext*> browser_context_all =
      CefBrowserContext::GetAll();
  if (browser_context_all.size() > 0) {
    return browser_context_all[0]->AsBrowserContext();
  }
  return nullptr;
}

std::string GetFullHash(const GURL& gurl) {
  std::string url = gurl.spec();
  if (base::StartsWith(url, "https://")) {
    url = url.substr(8);
  } else if (base::StartsWith(url, "http://")) {
    url = url.substr(7);
  }
  if (base::EndsWith(url, "/")) {
    url = url.substr(0, url.rfind("/"));
  }

  std::string full_hash = crypto::SHA256HashString(url);
  return base::ToLowerASCII(
      base::HexEncode(full_hash.data(), full_hash.size()));
}

std::string GenerateHashPrefix(const GURL& gurl) {
  return base::ToLowerASCII(GetFullHash(gurl).substr(0, kHashPrefixLength));
}

OHSBThreatType TransformMappingType(std::string mapping_type) {
  if (mapping_type == "THREAT_ILLEGAL") {
    return OHSBThreatType::THREAT_ILLEGAL;
  }
  if (mapping_type == "THREAT_FRAUD") {
    return OHSBThreatType::THREAT_FRAUD;
  }
  if (mapping_type == "THREAT_RISK") {
    return OHSBThreatType::THREAT_RISK;
  }
  if (mapping_type == "THREAT_WARNING") {
    return OHSBThreatType::THREAT_WARNING;
  }
  if (mapping_type == "THREAT_UNPROCESS") {
    return OHSBThreatType::THREAT_UNPROCESSED;
  }
  return OHSBThreatType::THREAT_DEFAULT;
}

GURL StripAuth(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  return gurl.ReplaceComponents(rep);
}

std::string ConvertUrlQuery(const std::string& url) {
  if (url.length() == 0) {
    return url;
  }
  url::Parsed parsed;
  url::ParseStandardURL(url.data(), url.length(), &parsed);
  std::string converted;
  url::Component& part = parsed.query;

  if (parsed.path.is_valid()) {
    converted.append(url, 0, parsed.path.end());
  } else {
    return url;
  }
  if (!part.is_valid()) {
    return converted;
  }
  converted.append("?");

  int i = 0;
  int appendStart = part.begin;
  bool replaceFlag = false;
  for (i = part.begin; i < part.end(); i++) {
    if ('=' == url[i]) {
      converted.append(url, appendStart, i - appendStart + 1);
      appendStart = part.end();
      replaceFlag = true;
      continue;
    }

    if ('&' == url[i]) {
      if (replaceFlag) {
        converted.append(kReplaceStr);
        replaceFlag = false;
      }
      appendStart = i;
    }
  }

  if (replaceFlag) {
    converted.append(kReplaceStr);
  } else if (appendStart < part.end()) {
    converted.append(url, appendStart, part.end() - appendStart + 1);
  }
  return converted;
}

}  // namespace

struct SafeBrowsingTabHelper::SBCheck {
  SBCheck(const GURL& url,
          std::unique_ptr<network::SimpleURLLoader> simple_url_loader);
  ~SBCheck();

  GURL url;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
  base::TimeTicks start_time;
};

SafeBrowsingTabHelper::SBCheck::SBCheck(
    const GURL& url,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader)
    : url(url),
      simple_url_loader(std::move(simple_url_loader)),
      start_time(base::TimeTicks::Now()) {}

SafeBrowsingTabHelper::SBCheck::~SBCheck() = default;

SafeBrowsingTabHelper::SafeBrowsingTabHelper(content::WebContents* web_contents,
                                             bool incognito_mode,
                                             CefBrowserHostBase* browser)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SafeBrowsingTabHelper>(*web_contents) {
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  if (!browser) {
    return;
  }
  PrefService* pref_service = user_prefs::UserPrefs::Get(browser_context);
  if (!pref_service) {
    return;
  }

  sb_client_.reset(new SbClient(web_contents, pref_service, incognito_mode));
  browser_ = browser;

  if (browser_) {
    browser_->AsArkWebBrowserHostExtImpl()->SetSafeBrowsingDetectionCallback(this);
    use_cloud_detection_ = false;
  }
}

SafeBrowsingTabHelper::~SafeBrowsingTabHelper() = default;

void SafeBrowsingTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    return;
  }

  if (content::NavigationRequest::From(navigation_handle)
          ->is_synchronous_renderer_commit()) {
    LOG(INFO) << "DidRedirectNavigation is_synchronous_renderer_commit";
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  QuerySafeBrowsingResults(navigation_handle);
}

void SafeBrowsingTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    return;
  }

  if (content::NavigationRequest::From(navigation_handle)
          ->is_synchronous_renderer_commit()) {
    LOG(INFO) << "is_synchronous_renderer_commit";
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  QuerySafeBrowsingResults(navigation_handle);
}

void SafeBrowsingTabHelper::QuerySafeBrowsingResults(
    content::NavigationHandle* navigation_handle) {
  if (browser_->AsArkWebBrowserHostExtImpl()
          ->IsSafeBrowsingDetectionDisabled()) {
    LOG(INFO) << "SafeBrowsingDetection is disabled";
    return;
  }

  scoped_refptr<net_service::URLLoaderFactoryGetter> loader_factory_getter;
  loader_factory_getter =
      net_service::URLLoaderFactoryGetter::Create(nullptr, GetBrowserContext());
  if (!loader_factory_getter) {
    LOG(ERROR) << "SafeBrowsing loader factory getter is null";
    return;
  }

  if (browser_) {
    browser_->AsArkWebBrowserHostExtImpl()->HandleSafeBrowsingDetection(
        navigation_handle->GetURL().spec());
    use_cloud_detection_ = false;
  }

  if (!use_cloud_detection_) {
    return;
  }

  bool is_safe_browsing_enabled =
      browser_->AsArkWebBrowserHostExtImpl()->IsSafeBrowsingEnabled();

  std::string package_name = "default";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kBundleName)) {
    package_name = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kBundleName);
  }
  std::string url_hash_prefix = GenerateHashPrefix(navigation_handle->GetURL());
  int tp_check_switch = is_safe_browsing_enabled ? 1 : 0;
  base::Value::Dict request_data;
  request_data.Set(kPackageName, package_name);
  if (is_safe_browsing_enabled) {
    request_data.Set(
        kUrl, ConvertUrlQuery(StripAuth(navigation_handle->GetURL()).spec()));
  }
  request_data.Set(kDomain, (navigation_handle->GetURL().scheme() + "://" +
                             navigation_handle->GetURL().host()));
  request_data.Set(kUrlHashPrefix, url_hash_prefix);
  request_data.Set(kTpCheckSwitch, tp_check_switch);
  std::string request_string;
  base::JSONWriter::Write(request_data, &request_string);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kSafeSearchApiUrl);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(kXHwGrsAttrGroupHeader, kSerLocation);
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kSafeBrowsingTrafficAnnotationTag);
  simple_url_loader->SetAllowHttpErrorResults(true);
  simple_url_loader->AttachStringForUpload(request_string, kContentTypeJSON);
  std::string converted_url =
      is_safe_browsing_enabled
          ? ConvertUrlQuery(navigation_handle->GetURL().spec())
          : navigation_handle->GetURL().spec();

  checks_running_.push_front(std::make_unique<SBCheck>(
      navigation_handle->GetURL(), std::move(simple_url_loader)));
  auto it = checks_running_.begin();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      loader_factory_getter->GetURLLoaderFactory();
  network::SimpleURLLoader* loader = it->get()->simple_url_loader.get();
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&SafeBrowsingTabHelper::OnSimpleLoaderComplete,
                     base::Unretained(this), it, navigation_handle->GetURL(),
                     converted_url));
}

void SafeBrowsingTabHelper::OnDetectionResult(int code,
                                              int policy,
                                              const std::string& mappingType,
                                              const std::string& url) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SafeBrowsingTabHelper::OnDetectionResult,
                                  weak_ptr_factory_.GetWeakPtr(), code, policy,
                                  mappingType, url));
    return;
  }

  if (policy >= OHSBPolicyType::POLICY_NO_PROMPT &&
      policy <= OHSBPolicyType::POLICY_HALF_POPUP) {
    LOG(INFO) << "SafeBrowsing SA detection with policy: " << policy
              << ", code: " << code << ", type: " << mappingType;

    OHSBThreatType threat_type = TransformMappingType(mappingType);
    sb_client_->SetEvilUrlPolicyAndHwCode(GURL(url), policy, threat_type, code,
                                          GURL(""));
  }
}

void SafeBrowsingTabHelper::OnSimpleLoaderComplete(
    SBCheckList::iterator it,
    const GURL& url,
    std::string converted_url,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<SBCheck> check = std::move(*it);
  checks_running_.erase(it);

  int response_code = -1;  // Invalid response code.
  int net_error = check->simple_url_loader->NetError();

  if (check->simple_url_loader->ResponseInfo() &&
      check->simple_url_loader->ResponseInfo()->headers) {
    response_code =
        check->simple_url_loader->ResponseInfo()->headers->response_code();
  }
  if (!response_body || response_body->empty() || *response_body == "{}") {
    LOG(INFO) << "OnSimpleLoaderComplete response_body null response_code:"
              << response_code << ", net_error:" << net_error;
    return;
  }
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(*response_body);
  if (!parsed_json || !parsed_json->is_dict()) {
    LOG(ERROR) << "JSONReader failed reading safe browsing response body : "
               << *response_body;
    return;
  }
  SafeBrowsingResponse response;
  base::JSONValueConverter<SafeBrowsingResponse> converter;
  converter.Convert(*parsed_json, &response);

  std::string check_result_url = response.url_check_result.url;
  int policy = response.url_check_result.policy;
  int hw_code = response.url_check_result.vendor_info.hw_code;
  std::string mapping_type = response.url_check_result.mapping_type;
  if (hw_code >= 0 && (policy >= OHSBPolicyType::POLICY_NO_PROMPT ||
                       policy <= OHSBPolicyType::POLICY_HALF_POPUP)) {
    LOG(INFO) << "SafeBrowsing hit url check result policy: " << policy;
    if (base::StartsWith(converted_url, check_result_url)) {
      sb_client_->SetEvilUrlPolicyAndHwCode(
          url, policy, TransformMappingType(mapping_type), hw_code, GURL(""));
      return;
    }
  }

  for (auto& check_result : response.hash_check_result) {
    for (auto& hash_result : check_result.hash_result_list) {
      if (hash_result.url_hash == GetFullHash(url)) {
        std::string hash_mapping_type = check_result.mapping_type;
        int hash_policy = hash_result.policy;
        int hash_hw_code = std::stoi(hash_result.hw_code);
        LOG(INFO) << "SafeBrowsing hit hash check policy " << hash_policy
                  << " url_hw_code " << hash_hw_code;
        if (hash_hw_code < 0 ||
            hash_policy < OHSBPolicyType::POLICY_NO_PROMPT ||
            hash_policy > OHSBPolicyType::POLICY_HALF_POPUP) {
          return;
        }
        sb_client_->SetEvilUrlPolicyAndHwCode(
            url, hash_policy, TransformMappingType(hash_mapping_type),
            hash_hw_code, GURL(""));

        return;
      }
    }
  }
}

void SafeBrowsingTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {}

void SafeBrowsingTabHelper::NavigationStopped() {}

void SafeBrowsingTabHelper::OnConnectionChanged(
    network::mojom::ConnectionType type) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingTabHelper);

}  // namespace ohos_safe_browsing
