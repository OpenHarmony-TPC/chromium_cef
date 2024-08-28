/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "libcef/browser/ohos_safe_browsing/ohos_sb_block_page.h"

#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "ohos_resources/components/string/grit/ohos_components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace ohos_safe_browsing {

OhosSbBlockPage::OhosSbBlockPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    int policy,
    OHSBThreatType block_type,
    std::unique_ptr<SecurityInterstitialControllerClient> controller)
    : SecurityInterstitialPage(web_contents,
                               request_url,
                               std::move(controller)),
      block_type_(block_type),
      policy_(policy) {}

OhosSbBlockPage::~OhosSbBlockPage() = default;

std::string OhosSbBlockPage::GetHTMLContents() {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_BLOCK_ERROR_OHOS_HTML);

  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, load_time_data);
}

void OhosSbBlockPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void OhosSbBlockPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  // CMD_DONT_PROCEED
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    controller()->GoBack();
  }

  // CMD_PROCEED
  if (command == security_interstitials::CMD_PROCEED) {
    controller()->Reload();
  }
}

void OhosSbBlockPage::OnInterstitialClosing() {}

bool OhosSbBlockPage::ShouldDisplayURL() const {
  return false;
}

void OhosSbBlockPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("policy", std::to_string(policy_));
  load_time_data.Set("title",
                     l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_TITLE));
  if (block_type_ == OHSBThreatType::THREAT_ILLEGAL ||
      block_type_ == OHSBThreatType::THREAT_RISK) {
    load_time_data.Set("block_info_title", l10n_util::GetStringUTF8(
                                               IDS_OHOS_BLOCK_PAGE_INFO_TITLE));
  } else {
    load_time_data.Set(
        "block_info_title",
        l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_FRAUD_INFO_TITLE));
  }

  if (block_type_ == OHSBThreatType::THREAT_ILLEGAL) {
    load_time_data.Set(
        "block_info_body",
        l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_ILLEGAL_INFO_BODY));
  } else if (block_type_ == OHSBThreatType::THREAT_RISK) {
    load_time_data.Set(
        "block_info_body",
        l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_RISK_INFO_BODY));
  } else {
    load_time_data.Set(
        "block_info_body",
        l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_FRAUD_INFO_BODY));
  }

  if (base::i18n::IsRTL()) {
    load_time_data.Set("page_direction", "rtl");
  } else {
    load_time_data.Set("page_direction", "ltr");
  }

  load_time_data.Set("dontproceed", l10n_util::GetStringUTF8(
                                        IDS_OHOS_BLOCK_PAGE_DONT_PROCEED));
  load_time_data.Set("proceed",
                     l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_PROCEED));
}

void OhosSbBlockPage::PopulateUrlTrustListInterstitialStrings(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("title",
    l10n_util::GetStringUTF8(IDS_OHOS_BLOCK_PAGE_TITLE));
  load_time_data.Set("block_info_title", l10n_util::GetStringUTF8(
    IDS_OHOS_BLOCK_PAGE_URL_TRUST_LIST_TITLE));

  if (base::i18n::IsRTL()) {
    load_time_data.Set("page_direction", "rtl");
  } else {
    load_time_data.Set("page_direction", "ltr");
  }

  load_time_data.Set("dontproceed", l10n_util::GetStringUTF8(
    IDS_OHOS_BLOCK_PAGE_DONT_PROCEED));
}

std::string OhosSbBlockPage::GetUrlTrustListErrorHTMLContents() {
  base::Value::Dict load_time_data;
  PopulateUrlTrustListInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          URL_TRUST_LIST_ERROR_OHOS_HTML);

  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, load_time_data);
}
}  // namespace ohos_safe_browsing
