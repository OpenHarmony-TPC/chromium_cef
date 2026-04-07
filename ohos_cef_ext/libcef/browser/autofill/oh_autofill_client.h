/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef CEF_LIBCEF_BROWSER_AUTOFILL_OH_AUTOFILL_CLIENT_H_
#define CEF_LIBCEF_BROWSER_AUTOFILL_OH_AUTOFILL_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/timer/timer.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "include/cef_browser.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {
class AutocompleteHistoryManager;
class AutofillSuggestionDelegate;
class PersonalDataManager;
class StrikeDatabase;
}  // namespace autofill

namespace content {
class WebContents;
}

namespace gfx {
class RectF;
}

namespace syncer {
class SyncService;
}

class PrefService;

namespace autofill {

// Manager delegate for the autofill functionality. Android webview
// supports enabling autocomplete feature for each webview instance
// (different than the browser which supports enabling/disabling for
// a profile). Since there is only one pref service for a given browser
// context, we cannot enable this feature via UserPrefs. Rather, we always
// keep the feature enabled at the pref service, and control it via
// the delegates.
class OhAutofillClient : public autofill::ContentAutofillClient,
                         public content::WebContentsObserver {
 public:
  static OhAutofillClient* FromWebContents(content::WebContents* web_contents) {
    return static_cast<OhAutofillClient*>(
        ContentAutofillClient::FromWebContents(web_contents));
  }

  // The `use_android_autofill_manager` parameter determines which
  // DriverInitCallback to use:
  // - autofill::BrowserDriverInitHook() (to be used before Android O) or
  // - android_webview::AndroidDriverInitHook() (to be used as of Android O).
  static void CreateForWebContents(content::WebContents* contents);
  OhAutofillClient(const OhAutofillClient&) = delete;
  OhAutofillClient& operator=(const OhAutofillClient&) = delete;

  ~OhAutofillClient() override;

  base::WeakPtr<AutofillClient> GetWeakPtr() override;

  void SetSaveFormData(bool enabled);
  bool GetSaveFormData() const;

  // AutofillClient:
  void SetOnMessageCallback(CefRefPtr<CefWebMessageReceiver> callback) {
    callback_ = callback;
  }
  void SetFocusFieldGlobalId(const FieldGlobalId& field_id);
  void FillData(CefRefPtr<CefValue> data, int32_t trigger_type = 0);
  bool OnAutofillEvent(const std::string& json_str);
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  void ShowAutofillSettings(autofill::SuggestionType suggestion_type) override;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) override;
  void ShowEditAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate) override;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  void PinAutofillSuggestions() override;
  void HideAutofillSuggestions(
      autofill::SuggestionHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(
      autofill::mojom::ActionPersistence action_persistence,
      autofill::AutofillTriggerSource trigger_source,
      bool is_refill) override;
  bool IsContextSecure() const override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<autofill::AutofillManager> CreateManager(
      base::PassKey<autofill::ContentAutofillDriver> pass_key,
      autofill::ContentAutofillDriver& driver) override;

#if BUILDFLAG(ARKWEB_DATALIST)
  using SelectedCallback = base::OnceCallback<void(std::u16string text)>;
  void ShowAutofillPopup(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      SelectedCallback callback);
  void HideAutofillPopup();
  void SuggestionSelected(int position);
#endif

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
 bool EnableAutoFill() const;
#endif  // BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)

 protected:
  // Protected for testing.
  explicit OhAutofillClient(content::WebContents* web_contents);

 private:
  struct PendingFillRequest {
    std::string json_str;
    int32_t trigger_type = 0;
    FieldGlobalId field_id{};
    std::optional<gfx::Rect> node_bounds_in_screen;
  };

  friend class content::WebContentsUserData<OhAutofillClient>;

  content::WebContents& GetWebContents() const;
  bool ShouldDeferFill(int32_t trigger_type) const;
  void QueuePendingFill(std::string json_str,
                        int32_t trigger_type,
                        const FieldGlobalId& field_id);
  void FlushPendingFill();
  void ExecuteFill(const std::string& json_str,
                   int32_t trigger_type,
                   const FieldGlobalId& field_id);
  void ClearPendingFill();

  bool save_form_data_ = false;
  std::vector<autofill::Suggestion> suggestions_;
  base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate_;
  CefRefPtr<CefWebMessageReceiver> callback_;
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
#if BUILDFLAG(ARKWEB_DATALIST)
  SelectedCallback selected_callback_;
#endif
  FieldGlobalId focused_field_id_{};
  std::optional<gfx::Rect> focused_node_bounds_in_screen_;
  std::optional<PendingFillRequest> pending_fill_;
  base::OneShotTimer pending_fill_timer_;

#if DCHECK_IS_ON()
  bool use_autofill_manager_;
#endif
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<OhAutofillClient> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CEF_LIBCEF_BROWSER_PASSWORD_OH_AUTOFILL_CLIENT_H_
