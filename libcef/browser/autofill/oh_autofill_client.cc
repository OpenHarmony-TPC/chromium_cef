// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/autofill/oh_autofill_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect_f.h"

#include "chrome/browser/browser_process.h"

#include "libcef/browser/autofill/oh_autofill_manager.h"
#if defined(OHOS_EX_PASSWORD)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

using content::WebContents;

namespace autofill {

void OhAutofillClient::CreateForWebContents(content::WebContents* contents,
                                            bool use_autofill_manager) {
  DCHECK(contents);
  if (!ContentAutofillClient::FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(), base::WrapUnique(new OhAutofillClient(
                                             contents, use_autofill_manager)));
  }
#if DCHECK_IS_ON()
  DCHECK_EQ(use_autofill_manager,
            FromWebContents(contents)->use_autofill_manager_);
#endif
}

OhAutofillClient::~OhAutofillClient() {
  HideAutofillPopup(autofill::PopupHidingReason::kTabGone);
}

void OhAutofillClient::FillData(CefRefPtr<CefValue> data) {
#if defined(OHOS_AUTOFILL)
  if (!data) {
    LOG(ERROR) << "data is null";
    return;
  }
  std::string json_str = data->GetStdString();
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh) {
    LOG(ERROR) << "rfh is nullptr";
    return;
  }
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver) {
    LOG(ERROR) << "driver is nullptr";
    return;
  }
  auto mgr = static_cast<OhAutofillManager*>(driver->oh_autofill_manager());
  if (mgr) {
    mgr->FillData(json_str);
  }
#endif
}

bool OhAutofillClient::OnAutofillEvent(const std::string& json_str) {
  if (callback_) {
    CefRefPtr<CefValue> data = CefValue::Create();
    data->SetStdString(json_str);
    return callback_->OnMessageWithBoolResult(data);
  }
  return false;
}

void OhAutofillClient::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool OhAutofillClient::GetSaveFormData() const {
  return save_form_data_;
}

bool OhAutofillClient::IsOffTheRecord() {
  return GetWebContents().GetBrowserContext()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
OhAutofillClient::GetURLLoaderFactory() {
  return GetWebContents()
      .GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

autofill::AutofillDownloadManager* OhAutofillClient::GetDownloadManager() {
  if (!download_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    download_manager_ = std::make_unique<autofill::AutofillDownloadManager>(
        this, GetChannel(), GetLogManager());
  }
  return download_manager_.get();
}

autofill::PersonalDataManager* OhAutofillClient::GetPersonalDataManager() {
  if (!personaldata_manager_) {
    personaldata_manager_ = std::make_unique<autofill::PersonalDataManager>(
        g_browser_process->GetApplicationLocale());
  }
  return personaldata_manager_.get();
}

autofill::AutocompleteHistoryManager*
OhAutofillClient::GetAutocompleteHistoryManager() {
  // autofillClient::GetSingleFieldFormFillRouter need create an object,
  // otherwise null pointer reference,actually not use functions.
  if (!autocomplete_history_manager_) {
    autocomplete_history_manager_ =
        std::make_unique<autofill::AutocompleteHistoryManager>();
  }
  return autocomplete_history_manager_.get();
}

PrefService* OhAutofillClient::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* OhAutofillClient::GetPrefs() const {
  if (g_browser_process) {
    return g_browser_process->local_state();
  }

  return nullptr;
}

syncer::SyncService* OhAutofillClient::GetSyncService() {
  return nullptr;
}

signin::IdentityManager* OhAutofillClient::GetIdentityManager() {
  return nullptr;
}

autofill::FormDataImporter* OhAutofillClient::GetFormDataImporter() {
  return nullptr;
}

autofill::payments::PaymentsClient* OhAutofillClient::GetPaymentsClient() {
  return nullptr;
}

autofill::StrikeDatabase* OhAutofillClient::GetStrikeDatabase() {
  return nullptr;
}

ukm::UkmRecorder* OhAutofillClient::GetUkmRecorder() {
  return nullptr;
}

ukm::SourceId OhAutofillClient::GetUkmSourceId() {
  // UKM recording is not supported for WebViews.
  return ukm::kInvalidSourceId;
}

autofill::AddressNormalizer* OhAutofillClient::GetAddressNormalizer() {
  return nullptr;
}

const GURL& OhAutofillClient::GetLastCommittedPrimaryMainFrameURL() const {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL();
}

url::Origin OhAutofillClient::GetLastCommittedPrimaryMainFrameOrigin() const {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

security_state::SecurityLevel
OhAutofillClient::GetSecurityLevelForUmaHistograms() {
  // The metrics are not recorded for Android webview, so return the count value
  // which will not be recorded.
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

const translate::LanguageState* OhAutofillClient::GetLanguageState() {
  return nullptr;
}

translate::TranslateDriver* OhAutofillClient::GetTranslateDriver() {
  return nullptr;
}

void OhAutofillClient::ShowAutofillSettings(autofill::PopupType popup_type) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  NOTIMPLEMENTED();
}

std::vector<std::string>
OhAutofillClient::GetAllowedMerchantsForVirtualCards() {
  return std::vector<std::string>();
}

std::vector<std::string>
OhAutofillClient::GetAllowedBinRangesForVirtualCards() {
  return std::vector<std::string>();
}

void OhAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {}

void OhAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {}

void OhAutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {}

void OhAutofillClient::ConfirmSaveIBANLocally(
    const IBAN& iban,
    bool should_show_prompt,
    LocalSaveIBANPromptCallback callback) {}

void OhAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {}

void OhAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {}

void OhAutofillClient::UpdateWebauthnOfferDialogWithError() {}

bool OhAutofillClient::CloseWebauthnDialog() {
  return false;
}

void OhAutofillClient::ConfirmSaveUpiIdLocally(
    const std::string& upi_id,
    base::OnceCallback<void(bool accept)> callback) {}

void OhAutofillClient::OfferVirtualCardOptions(
    const std::vector<CreditCard*>& candidates,
    base::OnceCallback<void(const std::string&)> callback) {}

void OhAutofillClient::ConfirmSaveCreditCardLocally(
    const autofill::CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::ConfirmSaveCreditCardToCloud(
    const autofill::CreditCard& card,
    const autofill::LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::CreditCardUploadCompleted(bool card_saved) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::ConfirmCreditCardFillAssist(
    const autofill::CreditCard& card,
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::ConfirmSaveAddressProfile(
    const autofill::AutofillProfile& profile,
    const autofill::AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
  NOTIMPLEMENTED();
}

bool OhAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void OhAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {
  NOTIMPLEMENTED();
}

bool OhAutofillClient::IsTouchToFillCreditCardSupported() {
  return false;
}

bool OhAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<autofill::TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard> cards_to_suggest) {
  NOTREACHED();
  return false;
}

void OhAutofillClient::HideTouchToFillCreditCard() {
  NOTREACHED();
}

void OhAutofillClient::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  suggestions_ = open_args.suggestions;
  delegate_ = delegate;

  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = GetWebContents().GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      open_args.element_bounds + client_area.OffsetFromOrigin();

  LOG(INFO) << "showAutofillPopup client_area" << client_area.ToString()
            << ", element_bounds_in_screen_space"
            << element_bounds_in_screen_space.ToString();
  ShowAutofillPopupImpl(element_bounds_in_screen_space,
                        open_args.text_direction == base::i18n::RIGHT_TO_LEFT,
                        open_args.suggestions, delegate->GetPopupType());
}

void OhAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  // Leaving as an empty method since updating autofill popup window
  // dynamically does not seem to be a useful feature for android webview.
  // See crrev.com/18102002 if need to implement.
}

std::vector<autofill::Suggestion> OhAutofillClient::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return {};
}

void OhAutofillClient::PinPopupView() {
  NOTIMPLEMENTED();
}

autofill::AutofillClient::PopupOpenArgs OhAutofillClient::GetReopenPopupArgs()
    const {
  NOTIMPLEMENTED();
  return {};
}

void OhAutofillClient::UpdatePopup(
    const std::vector<autofill::Suggestion>& suggestions,
    autofill::PopupType popup_type) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::HideAutofillPopup(autofill::PopupHidingReason reason) {
  delegate_.reset();
#if defined(OHOS_EX_PASSWORD) || (OHOS_DATALIST)
  GetWebContents().HideAutofillPopup();
#endif
}

bool OhAutofillClient::IsAutocompleteEnabled() const {
  return GetSaveFormData();
}

bool OhAutofillClient::IsPasswordManagerEnabled() {
  return false;
}

void OhAutofillClient::PropagateAutofillPredictions(
    autofill::AutofillDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {}

void OhAutofillClient::DidFillOrPreviewForm(
    autofill::mojom::RendererFormDataAction action,
    autofill::AutofillTriggerSource trigger_source,
    bool is_refill) {}

void OhAutofillClient::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool OhAutofillClient::IsContextSecure() const {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      GetWebContents().GetController().GetLastCommittedEntry();
  if (!navigation_entry) {
    return false;
  }

  ssl_status = navigation_entry->GetSSL();
  // Note: As of crbug.com/701018, Chrome relies on SecurityStateTabHelper to
  // determine whether the page is secure, but WebView has no equivalent class.

  return navigation_entry->GetURL().SchemeIsCryptographic() &&
         ssl_status.certificate &&
         !net::IsCertStatusError(ssl_status.cert_status) &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

void OhAutofillClient::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  NOTIMPLEMENTED();
}

autofill::FormInteractionsFlowId
OhAutofillClient::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

void OhAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::Dismissed() {}

void OhAutofillClient::SuggestionSelected(int position) {
#if defined(OHOS_EX_PASSWORD)
  if (position < 0 || position > static_cast<int>(suggestions_.size()) - 1) {
    LOG(WARNING) << " parameter is invalid, position: " << position;
    return;
  }
#endif
  if (delegate_) {
    delegate_->DidAcceptSuggestion(suggestions_[position], position);
  }
}

void DriverInit(AutofillClient* client, const std::string& app_locale,
                ContentAutofillDriver* driver) {
  autofill::BrowserDriverInitHook(client, app_locale, driver);
#if defined(OHOS_AUTOFILL)
  autofill::OhDriverInitHook(client, driver);
#endif
}

// Ownership: The native object is created (if autofill enabled) and owned by
// AwContents. The native object creates the java peer which handles most
// autofill functionality at the java side. The java peer is owned by Java
// AwContents. The native object only maintains a weak ref to it.
OhAutofillClient::OhAutofillClient(WebContents* contents,
                                   bool use_autofill_manager)
    : autofill::ContentAutofillClient(
          contents,
          use_autofill_manager
              ? base::BindRepeating(&autofill::OhDriverInitHook, this)
              : base::BindRepeating(&autofill::DriverInit,
                                    this,
                                    g_browser_process->GetApplicationLocale()))
#if DCHECK_IS_ON()
      ,
      use_autofill_manager_(use_autofill_manager)
#endif
{
}

void OhAutofillClient::ShowAutofillPopupImpl(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions,
    autofill::PopupType popup_type) {
#if defined(OHOS_DATALIST)
  if(popup_type != autofill::PopupType::kPasswords) {
    GetWebContents().ShowAutofillPopup(element_bounds, is_rtl, suggestions, false);
    return;
  }
#endif

#if defined(OHOS_EX_PASSWORD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser) && popup_type == autofill::PopupType::kPasswords) {
    GetWebContents().ShowAutofillPopup(element_bounds, is_rtl, suggestions, true);
  }
#endif
}

content::WebContents& OhAutofillClient::GetWebContents() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      ContentAutofillClient::GetWebContents());
}

}  // namespace autofill
