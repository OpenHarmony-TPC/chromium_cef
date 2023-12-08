// Copyright (c) 2021 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/autofill/oh_autofill_client.h"

#include <utility>

#include "base/notreached.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect_f.h"

#include "chrome/browser/browser_process.h"

#if defined(OHOS_NWEB_EX)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

using content::WebContents;

namespace autofill {

OhAutofillClient::~OhAutofillClient() {
  HideAutofillPopup(autofill::PopupHidingReason::kTabGone);
}

void OhAutofillClient::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool OhAutofillClient::GetSaveFormData() {
  return save_form_data_;
}

autofill::PersonalDataManager* OhAutofillClient::GetPersonalDataManager() {
  return nullptr;
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
  return const_cast<PrefService*>(base::as_const(*this).GetPrefs());
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

const GURL& OhAutofillClient::GetLastCommittedURL() const {
  return GetWebContents().GetLastCommittedURL();
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

void OhAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    UnmaskCardReason reason,
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
                        open_args.suggestions);
}

void OhAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  // Leaving as an empty method since updating autofill popup window
  // dynamically does not seem to be a useful feature for android webview.
  // See crrev.com/18102002 if need to implement.
}

base::span<const autofill::Suggestion> OhAutofillClient::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return base::span<const autofill::Suggestion>();
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
#if defined(OHOS_NWEB_EX)
  GetWebContents().HideAutofillPopup();
#endif
}

bool OhAutofillClient::IsAutocompleteEnabled() {
  return GetSaveFormData();
}

void OhAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {}

void OhAutofillClient::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool OhAutofillClient::IsContextSecure() const {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      GetWebContents().GetController().GetLastCommittedEntry();
  if (!navigation_entry)
    return false;

  ssl_status = navigation_entry->GetSSL();
  // Note: As of crbug.com/701018, Chrome relies on SecurityStateTabHelper to
  // determine whether the page is secure, but WebView has no equivalent class.

  return navigation_entry->GetURL().SchemeIsCryptographic() &&
         ssl_status.certificate &&
         !net::IsCertStatusError(ssl_status.cert_status) &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

bool OhAutofillClient::ShouldShowSigninPromo() {
  return false;
}

bool OhAutofillClient::AreServerCardsSupported() const {
  return true;
}

void OhAutofillClient::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED();
}

void OhAutofillClient::Dismissed() {}

void OhAutofillClient::SuggestionSelected(int position) {
#if defined(OHOS_NWEB_EX)
  if (position < 0 || position > static_cast<int>(suggestions_.size()) - 1) {
    LOG(WARNING) << " parameter is invalid, position: " << position;
    return;
  }
#endif

  if (delegate_) {
    delegate_->DidAcceptSuggestion(suggestions_[position].value,
                                   suggestions_[position].frontend_id,
                                   suggestions_[position].backend_id, position);
  }
}

// Ownership: The native object is created (if autofill enabled) and owned by
// AwContents. The native object creates the java peer which handles most
// autofill functionality at the java side. The java peer is owned by Java
// AwContents. The native object only maintains a weak ref to it.
OhAutofillClient::OhAutofillClient(WebContents* contents)
    : content::WebContentsUserData<OhAutofillClient>(*contents),
      content::WebContentsObserver(contents) {}

void OhAutofillClient::ShowAutofillPopupImpl(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions) {
#if defined(OHOS_NWEB_EX)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser)) {
    GetWebContents().ShowAutofillPopup(element_bounds, is_rtl, suggestions);
  }
#endif
}

content::WebContents& OhAutofillClient::GetWebContents() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      content::WebContentsUserData<OhAutofillClient>::GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhAutofillClient);

}  // namespace autofill
