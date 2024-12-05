/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 *
 * Based on chrome_password_manager_client.h originally written by
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_MANAGER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_credential_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/prefs/pref_member.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

#include "components/password_manager/core/browser/oh_sync_credentials_filter.h"

class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
class Profile;

namespace autofill {
class LogManager;
class RoutingLogManager;

namespace password_generation {
struct PasswordGenerationUIData;
}  // namespace password_generation
}  // namespace autofill

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device_reauth {
class DeviceAuthenticator;
}

namespace password_manager {
class WebAuthnCredentialsDelegate;
}
// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class OhPasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<OhPasswordManagerClient>,
      public autofill::mojom::PasswordGenerationDriver {
 public:
  ~OhPasswordManagerClient() override;

  // PasswordManagerClient implementation.
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsFillingEnabled(const GURL& url) const override;
  bool IsAutoSignInEnabled() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_update) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;

  void GeneratePassword(
      autofill::password_generation::PasswordGenerationType type) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<password_manager::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager) override;
  void PasswordWasAutofilled(
      const std::vector<const password_manager::PasswordForm*>& best_matches,
      const url::Origin& origin,
      const std::vector<const password_manager::PasswordForm*>*
          federated_matches,
      bool was_autofilled_on_pageload) override;
  void AutofillHttpAuth(
      const password_manager::PasswordForm& preferred_match,
      const password_manager::PasswordFormManagerForUI* form_manager) override;
  void NotifyUserCredentialsWereLeaked(
      password_manager::CredentialLeakType leak_type,
      const GURL& origin,
      const std::u16string& username) override;
  bool IsIsolationForPasswordSitesEnabled() const override;

  PrefService* GetPrefs() const override;
  password_manager::PasswordStore* GetPasswordStore();
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override;
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override;
  password_manager::SyncState GetPasswordSyncState() const override;
  bool WasLastNavigationHTTPError() const override;
  net::CertStatus GetMainFrameCertStatus() const override;
  bool IsIncognito() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  const password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      const override;
  password_manager::HttpAuthManager* GetHttpAuthManager() override;
  autofill::AutofillDownloadManager* GetAutofillDownloadManager() override;

  bool IsCommittedMainFrameSecure() const override;
  void AnnotateNavigationEntry(bool has_password_field) override;
  autofill::LanguageCode GetPageLanguage() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  autofill::LogManager* GetLogManager() override;
  password_manager::PasswordRequirementsService*
  GetPasswordRequirementsService() override;
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  void UpdateFormManagers() override;
  void NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer referrer) override;
  bool IsNewTabPage() const override;

#if defined(OHOS_PASSWORD_AUTOFILL)
  struct AutofillIMFInfo {
    bool is_username = false;
    bool is_other_account = false;
    bool is_new_password = false;
  };

  void ProcessAutofillCancel(const std::string& fillContent);

  void AutoFillWithIMFEvent(bool is_username,
                            bool is_other_account,
                            bool is_new_password,
                            const std::string& content);

  void FillData(const std::string& page_url,
                const std::string& username,
                const std::string& password,
                bool is_other_account);

  bool IsUsernamePasswordForm(autofill::FormRendererId form_id) {
    return form_id == last_fill_form_id_;
  }

  bool IsUsernamePasswordField(autofill::FieldRendererId field_id) {
    return field_id == last_fill_focus_renderer_id_;
  }

  void FillAccountSuggestion(
      const GURL& page_url,
      const std::u16string& username,
      const std::u16string& password) override;

  void OnRequestAutofill(
      password_manager::PasswordManagerDriver* driver,
      const GURL& page_url,
      autofill::FormRendererId form_id,
      const autofill::mojom::OhosPasswordFormAutofillState state,
      const autofill::InputFillRequestData& username_data,
      const autofill::InputFillRequestData& password_data) override;
#endif

  // autofill::mojom::PasswordGenerationDriver overrides.
  void AutomaticGenerationAvailable(
      const autofill::password_generation::PasswordGenerationUIData& ui_data)
      override;
  void ShowPasswordEditingPopup(const gfx::RectF& bounds,
                                const autofill::FormData& form_data,
                                autofill::FieldRendererId field_renderer_id,
                                const std::u16string& password_value) override;

  void PasswordGenerationRejectedByTyping() override;
  void PresaveGeneratedPassword(const autofill::FormData& form_data,
                                const std::u16string& password_value) override;
  void PasswordNoLongerGenerated(const autofill::FormData& form_data) override;
  void FrameWasScrolled() override;
  void GenerationElementLostFocus() override;

#if defined(ON_FOCUS_PING_ENABLED) && BUILDFLAG(FULL_SAFE_BROWSING)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;

  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(PasswordGenerationPopupObserver* observer);

  static void BindCredentialManager(
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver,
      content::RenderFrameHost* render_frame_host);

  // A helper method to determine whether a save/update bubble can be shown
  // on this |url|.
  static bool CanShowBubbleOnURL(const GURL& url);

#if defined(UNIT_TEST)
  bool was_store_ever_called() const { return was_store_ever_called_; }
  bool has_binding_for_credential_manager() const {
    return content_credential_manager_.HasBinding();
  }
  bool was_on_paste_called() const { return was_on_paste_called_; }
#endif

  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move)
      override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;

  void TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<void(ReauthSucceeded)> reauth_callback) override;

  void TriggerSignIn(signin_metrics::AccessPoint access_point) override;
  // Returns a pointer to a DeviceAuthenticator. Might be null if
  // BiometricAuthentication is not available for a given platform.
  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;

 protected:
  // Callable for tests.
  OhPasswordManagerClient(content::WebContents* web_contents,
                          autofill::AutofillClient* autofill_client);

 private:
  friend class content::WebContentsUserData<OhPasswordManagerClient>;

  // content::WebContentsObserver overrides.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void OnPaste() override;
#endif

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Checks if the current page specified in |url| fulfils the conditions for
  // the password manager to be active on it.
  bool IsPasswordManagementEnabledForCurrentPage(const GURL& url) const;

  // password_manager::PasswordManagerClientHelperDelegate implementation.
  void PromptUserToEnableAutosignin() override;
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() override;
  password_manager::FieldInfoManager* GetFieldInfoManager() const override;

  void GenerationResultAvailable(
      autofill::password_generation::PasswordGenerationType type,
      base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
      const absl::optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data);

  // |ui_data| is empty in case the renderer failed to start manual generation.
  // In this case nothing should happen.
  void ShowPasswordGenerationPopup(
      autofill::password_generation::PasswordGenerationType type,
      password_manager::ContentPasswordManagerDriver* driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data);

  gfx::RectF TransformToRootCoordinates(
      content::RenderFrameHost* frame_host,
      const gfx::RectF& bounds_in_frame_coordinates);

#if defined(OHOS_PASSWORD_AUTOFILL)
  absl::optional<std::string> PasswordFormToJsonForRequest(
      const std::string& event,
      const GURL& page_url,
      const autofill::InputFillRequestData& username_data,
      const autofill::InputFillRequestData& password_data,
      AutofillIMFInfo* imf_info = nullptr);

  absl::optional<std::string> PasswordFormToJsonForSave(
      const password_manager::PasswordForm& form);

  void SuppressKeyboard();
  void UnsuppressKeyboard();

  bool IsLoginInfoConsistentWithFilled(
      const password_manager::PasswordForm& info);

  void UpdateLastRequestFilledItems(
      const autofill::InputFillRequestData& username_data,
      const autofill::InputFillRequestData& password_data);

  void NotifyAutofillPopupShow(bool is_show);
#endif

  content::BrowserContext* const context_;

  password_manager::PasswordManager password_manager_;
  password_manager::PasswordFeatureManagerImpl password_feature_manager_;
  password_manager::HttpAuthManagerImpl httpauth_manager_;

  password_manager::ContentPasswordManagerDriverFactory* driver_factory_;

  // As a mojo service, will be registered into service registry
  // of the main frame host by ChromeContentBrowserClient
  // once main frame host was created.

  content::RenderFrameHostReceiverSet<autofill::mojom::PasswordGenerationDriver>
      password_generation_driver_receivers_;

  // Observer for password generation popup.
  PasswordGenerationPopupObserver* observer_;

  // Set to false to disable password saving (will no longer ask if you
  // want to save passwords and also won't fill the passwords).

  const password_manager::OhSyncCredentialsFilter credentials_filter_;

  std::unique_ptr<autofill::RoutingLogManager> log_manager_;
  std::unique_ptr<password_manager::PasswordChangeSuccessTracker>
      password_change_success_tracker_;
  // Recorder of metrics that is associated with the last committed navigation
  // of the WebContents owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  absl::optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Whether navigator.credentials.store() was ever called from this
  // WebContents. Used for testing.
  bool was_store_ever_called_ = false;

  // Whether OnPaste() was called from this ChromePasswordManagerClient
  bool was_on_paste_called_ = false;

#if defined(OHOS_PASSWORD_AUTOFILL)
  using AutofilledMap = std::unordered_map<std::uint64_t, std::string>;

  GURL form_to_request_url_;

  autofill::FormRendererId last_fill_form_id_;
  autofill::FieldRendererId last_fill_focus_renderer_id_;

  AutofilledMap auto_filled_forms_username_;
  AutofilledMap auto_filled_forms_password_;

  autofill::InputFillRequestData last_request_fill_username_;
  autofill::InputFillRequestData last_request_fill_password_;

  // Keyboard suppressor
  bool is_need_restore_keyboard_ = false;
  raw_ptr<autofill::ContentAutofillDriver> suppressed_driver_ = nullptr;
  base::OneShotTimer unsuppress_timer_;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_MANAGER_CLIENT_H_
