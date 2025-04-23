/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 *
 * Based on chrome_password_manager_client.cc originally written by
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "libcef/browser/password/oh_password_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/ohos/sys_info_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/field_info_manager_factory.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/version_info/version_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "crypto/sha2.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/url_constants.h"

#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/autofill/oh_autofill_client.h"
#include "libcef/browser/autofill/oh_autofill_manager.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/context.h"
#include "libcef/browser/password/oh_password_store_factory.h"
#include "libcef/common/alloy/alloy_main_runner_delegate.h"

using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationType;
using password_manager::BadMessageReason;
using password_manager::ContentPasswordManagerDriverFactory;
using password_manager::FieldInfoManager;
using password_manager::PasswordForm;
using password_manager::PasswordManagerClientHelper;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::metrics_util::PasswordType;
using sessions::SerializedNavigationEntry;

using password_manager::FieldInfoManager;
using password_manager::FieldInfoManagerImpl;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace {
#if defined(OHOS_PASSWORD_AUTOFILL)
const std::string SOURCE = "source";
const std::string SOURCE_LOGIN = "login";
const std::string EVENT = "event";
const std::string EVENT_SAVE = "save";
const std::string EVENT_FILL = "fill";
const std::string EVENT_UPDATE = "update";

const std::string KEY_FOCUS = "focus";
const std::string KEY_RECT_X = "x";
const std::string KEY_RECT_Y = "y";
const std::string KEY_RECT_W = "width";
const std::string KEY_RECT_H = "height";
const std::string KEY_PLACEHOLDER = "placeholder";
const std::string KEY_VALUE = "value";

const std::string KEY_PAGE_URL = "pageUrl";
const std::string KEY_IS_USER_SELECTED = "isUserSelected";
const std::string KEY_IS_OTHER_ACCOUNT = "isOtherAccount";

const std::string KEY_USERNAME = "username";
const std::string KEY_PASSWORD = "password";

const std::string HASH_SALT = "OHOS@PASSWORD@AUTOFILL";
#endif
} // namespace

// static
void OhPasswordManagerClient::CreateForWebContentsWithAutofillClient(
    content::WebContents* contents,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(contents)) {
    return;
  }

  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new OhPasswordManagerClient(contents, autofill_client)));
}

OhPasswordManagerClient::OhPasswordManagerClient(
    content::WebContents* web_contents,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OhPasswordManagerClient>(*web_contents),
      context_(web_contents->GetBrowserContext()),
      password_manager_(this),
      password_feature_manager_(CefBrowserContext::FromBrowserContext(context_)
                                    ->AsProfile()
                                    ->GetPrefs(),
                                g_browser_process->local_state(),
                                nullptr),
      httpauth_manager_(this),
      driver_factory_(nullptr),
      password_generation_driver_receivers_(web_contents, this),
      observer_(nullptr),
      credentials_filter_(this) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this,
                                                            autofill_client);
  driver_factory_ =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  log_manager_ = autofill::LogManager::Create(
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          context_),
      base::BindRepeating(
          &ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability,
          base::Unretained(driver_factory_)));

  driver_factory_->RequestSendLoggingAvailability();
}

OhPasswordManagerClient::~OhPasswordManagerClient() {}

bool OhPasswordManagerClient::IsPasswordManagementEnabledForCurrentPage(
    const GURL& url) const {
  bool is_enabled = CanShowBubbleOnURL(url);

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogURL(Logger::STRING_SECURITY_ORIGIN, url);
    logger.LogBoolean(
        Logger::STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE,
        is_enabled);
  }
  return is_enabled;
}

bool OhPasswordManagerClient::IsSavingAndFillingEnabled(const GURL& url) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    // Disable the password saving UI for automated tests. It obscures the
    // page, and there is no API to access (or dismiss) UI bubbles/infobars.
    return false;
  }
  return !IsIncognito() && IsFillingEnabled(url);
}

bool OhPasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  const bool ssl_errors = net::IsCertStatusError(GetMainFrameCertStatus());

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT, ssl_errors);
  }

  return !ssl_errors && IsPasswordManagementEnabledForCurrentPage(url);
}

bool OhPasswordManagerClient::IsAutoSignInEnabled() const {
  return false;
}

bool OhPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
#if defined(OHOS_PASSWORD_AUTOFILL)
  if (!form_to_save) {
    LOG(ERROR) << "form_to_save is nullptr";
    return false;
  }

  // The save password infobar and the password bubble prompt in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return false;
  }

  if (form_to_save->IsBlocklisted()) {
    return false;
  }

  // If the information used when the user logs in is current site filled in and
  // not modified, then do not prompt the user to save.
  if (IsLoginInfoConsistentWithFilled(form_to_save->GetPendingCredentials())) {
    LOG(INFO) << "auto filled password, not save on login";
    return false;
  }

  auto* autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents());
  if (!autofill_client) {
    LOG(ERROR) << "autofill_client is nullptr";
    return false;
  }
  auto json_str =
      PasswordFormToJsonForSave(form_to_save->GetPendingCredentials());
  if (json_str.has_value()) {
    LOG(INFO) << "call autofill for save from system.";
    bool result = autofill_client->OnAutofillEvent(json_str.value());
    if (!result) {
      LOG(ERROR) << "failed to call autofill for save";
      return false;
    }
  }
#endif
  return false;
}

password_manager::PasswordStoreInterface*
OhPasswordManagerClient::GetProfilePasswordStore() const {
  return OhPasswordStoreFactory::GetPasswordStoreForContext(
             context_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface*
OhPasswordManagerClient::GetAccountPasswordStore() const {
  return nullptr;
}

password_manager::PasswordReuseManager*
OhPasswordManagerClient::GetPasswordReuseManager() const {
  return nullptr;
}

password_manager::PasswordChangeSuccessTracker*
OhPasswordManagerClient::GetPasswordChangeSuccessTracker() {
  if (!password_change_success_tracker_) {
    password_change_success_tracker_ =
        std::make_unique<password_manager::PasswordChangeSuccessTrackerImpl>(
            GetPrefs());
  }
  return password_change_success_tracker_.get();
}

password_manager::FieldInfoManager*
OhPasswordManagerClient::GetFieldInfoManager() const {
  return nullptr;
}

void OhPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return;
  }
}

void OhPasswordManagerClient::HideManualFallbackForSaving() {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return;
  }
}

bool OhPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  return true;
}

scoped_refptr<device_reauth::DeviceAuthenticator>
OhPasswordManagerClient::GetDeviceAuthenticator() {
  return nullptr;
}

void OhPasswordManagerClient::GeneratePassword(
    autofill::password_generation::PasswordGenerationType type) {
  password_manager::ContentPasswordManagerDriver* content_driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  // Using unretained pointer is safe because |this| outlives
  // ContentPasswordManagerDriver that holds the connection.
  if (content_driver != nullptr) {
    content_driver->GeneratePassword(base::BindOnce(
        &OhPasswordManagerClient::GenerationResultAvailable,
        base::Unretained(this), type, base::AsWeakPtr(content_driver)));
  }
}

void OhPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
}

void OhPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<password_manager::PasswordForm> form) {}

void OhPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {}

void OhPasswordManagerClient::NotifyStorePasswordCalled() {
  was_store_ever_called_ = true;
}

void OhPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {}

void OhPasswordManagerClient::PasswordWasAutofilled(
    const std::vector<const PasswordForm*>& best_matches,
    const url::Origin& origin,
    const std::vector<const PasswordForm*>* federated_matches,
    bool was_autofilled_on_pageload) {}

void OhPasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const password_manager::PasswordFormManagerForUI* form_manager) {
  httpauth_manager_.Autofill(preferred_match, form_manager);
  DCHECK(!form_manager->GetBestMatches().empty());
  PasswordWasAutofilled(form_manager->GetBestMatches(),
                        url::Origin::Create(form_manager->GetURL()), nullptr,
                        false);
}

void OhPasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username) {}

safe_browsing::PasswordProtectionService*
OhPasswordManagerClient::GetPasswordProtectionService() const {
  return NULL;
}

#if defined(ON_FOCUS_PING_ENABLED) && BUILDFLAG(FULL_SAFE_BROWSING)
void OhPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {}
#endif  // defined(ON_FOCUS_PING_ENABLED)

void OhPasswordManagerClient::TriggerReauthForPrimaryAccount(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
  std::move(reauth_callback).Run(ReauthSucceeded(false));
}

void OhPasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {}

ukm::SourceId OhPasswordManagerClient::GetUkmSourceId() {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

PasswordManagerMetricsRecorder* OhPasswordManagerClient::GetMetricsRecorder() {
  return base::OptionalToPtr(metrics_recorder_);
}

void OhPasswordManagerClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->IsSameDocument()) {
    // Send any collected metrics by destroying the metrics recorder.
    metrics_recorder_.reset();
  }

  httpauth_manager_.OnDidFinishMainFrameNavigation();
}

// GetPrefs
PrefService* OhPasswordManagerClient::GetPrefs() const {
  if (g_browser_process) {
    return g_browser_process->local_state();
  }

  return nullptr;
}

PrefService* OhPasswordManagerClient::GetLocalStatePrefs() const {
  if (g_browser_process) {
    return g_browser_process->local_state();
  }

  return nullptr;
}

const syncer::SyncService* OhPasswordManagerClient::GetSyncService() const {
  return nullptr;
}

password_manager::PasswordStore* OhPasswordManagerClient::GetPasswordStore() {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  // TODO(gcasto): Is is safe to change this to
  // ServiceAccessType::IMPLICIT_ACCESS?
  return OhPasswordStoreFactory::GetPasswordStoreForContext(
             context_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::SyncState OhPasswordManagerClient::GetPasswordSyncState()
    const {
  return (password_manager::SyncState)0;
}

bool OhPasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (log_manager_->IsLoggingActive()) {
    logger =
        std::make_unique<password_manager::BrowserSavePasswordProgressLogger>(
            log_manager_.get());
    logger->LogMessage(Logger::STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD);
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry) {
    return false;
  }
  int http_status_code = entry->GetHttpStatusCode();

  if (logger) {
    logger->LogNumber(Logger::STRING_HTTP_STATUS_CODE, http_status_code);
  }

  if (http_status_code >= 400 && http_status_code < 600) {
    // #ifdef HW_BUILD_PASSWORD
    LOG(WARNING) << "[PasswordSave] Last navigation occur http error, "
                    "http_status_code is: "
                 << http_status_code;
    // #endif
    return true;
  }
  return false;
}

net::CertStatus OhPasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return 0;
  }
  return entry->GetSSL().cert_status;
}

void OhPasswordManagerClient::PromptUserToEnableAutosignin() {}

bool OhPasswordManagerClient::IsIncognito() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

const password_manager::PasswordManager*
OhPasswordManagerClient::GetPasswordManager() const {
  return &password_manager_;
}

const password_manager::PasswordFeatureManager*
OhPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

password_manager::HttpAuthManager*
OhPasswordManagerClient::GetHttpAuthManager() {
  return &httpauth_manager_;
}

autofill::AutofillDownloadManager*
OhPasswordManagerClient::GetAutofillDownloadManager() {
  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents());
  if (factory) {
    autofill::ContentAutofillDriver* driver =
        factory->DriverForFrame(web_contents()->GetPrimaryMainFrame());
    // |driver| can be NULL if the tab is being closed.
    if (driver) {
      autofill::AutofillManager* autofill_manager = driver->autofill_manager();
      if (autofill_manager) {
        return autofill_manager->download_manager();
      }
    }
  }
  return nullptr;
}

void OhPasswordManagerClient::SetTestObserver(
    PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

void OhPasswordManagerClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
void OhPasswordManagerClient::OnPaste() {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string& text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &text);
  was_on_paste_called_ = true;
}
#endif

gfx::RectF OhPasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounds + client_area.OffsetFromOrigin();
}

void OhPasswordManagerClient::AutomaticGenerationAvailable(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          ui_data.form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED)) {
    return;
  }

  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_generation_driver_receivers_.GetCurrentTargetFrame());
  if (driver != nullptr) {
    ShowPasswordGenerationPopup(PasswordGenerationType::kAutomatic, driver,
                                ui_data);
  }
}

void OhPasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::FormData& form_data,
    autofill::FieldRendererId field_renderer_id,
    const std::u16string& password_value) {}

void OhPasswordManagerClient::PasswordGenerationRejectedByTyping() {}

void OhPasswordManagerClient::PresaveGeneratedPassword(
    const autofill::FormData& form_data,
    const std::u16string& password_value) {
  // CheckChildProcessSecurityPolicy -> CheckChildProcessSecurityPolicyForURL
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD)) {
    return;
  }

  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  password_manager_.OnPresaveGeneratedPassword(driver, form_data,
                                               password_value);
}

void OhPasswordManagerClient::PasswordNoLongerGenerated(
    const autofill::FormData& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          password_form.url,
          BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED)) {
    return;
  }

  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  password_manager_.OnPasswordNoLongerGenerated(driver, password_form);
}

void OhPasswordManagerClient::FrameWasScrolled() {}

void OhPasswordManagerClient::GenerationElementLostFocus() {
  // TODO(crbug.com/968046): Look into removing this since FocusedInputChanged
  // seems to be a good replacement.
}

void OhPasswordManagerClient::AnnotateNavigationEntry(bool has_password_field) {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return;
  }

  SerializedNavigationEntry::PasswordState old_state =
      sessions::GetPasswordStateFromNavigation(entry);

  SerializedNavigationEntry::PasswordState new_state =
      (has_password_field ? SerializedNavigationEntry::HAS_PASSWORD_FIELD
                          : SerializedNavigationEntry::NO_PASSWORD_FIELD);

  if (new_state > old_state) {
    SetPasswordStateInNavigation(new_state, entry);
  }
}

autofill::LanguageCode OhPasswordManagerClient::GetPageLanguage() const {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  return autofill::LanguageCode();
}

const password_manager::CredentialsFilter*
OhPasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

autofill::LogManager* OhPasswordManagerClient::GetLogManager() {
  return log_manager_.get();
}

password_manager::PasswordRequirementsService*
OhPasswordManagerClient::GetPasswordRequirementsService() {
  return password_manager::PasswordRequirementsServiceFactory::
      GetForBrowserContext(web_contents()->GetBrowserContext());
}

signin::IdentityManager* OhPasswordManagerClient::GetIdentityManager() {
  return NULL;
}

scoped_refptr<network::SharedURLLoaderFactory>
OhPasswordManagerClient::GetURLLoaderFactory() {
  return context_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

#if defined(OHOS_PASSWORD_AUTOFILL)
using autofill::mojom::OhosPasswordFormAutofillState;

absl::optional<std::string>
OhPasswordManagerClient::PasswordFormToJsonForRequest(
    const std::string& event,
    const GURL& page_url,
    const autofill::InputFillRequestData& username_data,
    const autofill::InputFillRequestData& password_data,
    AutofillIMFInfo* imf_info) {
  if (!web_contents()) {
    LOG(ERROR) << "web_contents is nullptr";
    return absl::nullopt;
  }
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  if (!rfh || !rfh->IsActive()) {
    LOG(ERROR) << "rfh is nullptr or not active";
    return absl::nullopt;
  }
  auto browser = CefBrowserHostBase::GetBrowserForHost(rfh);
  if (!browser) {
    LOG(ERROR) << "browser is nullptr";
    return absl::nullopt;
  }

  float ratio = browser->GetVirtualPixelRatio();
  auto offset =
      content::WebContents::FromRenderFrameHost(rfh)->GetContainerBounds();
  int32_t viewport_height = 0;
  if (browser->GetHost()) {
    viewport_height = browser->GetHost()->GetShrinkViewportHeight();
  }

  base::Value::List view_data_list;
  view_data_list.Append(base::Value::Dict().Set(EVENT, event));
  view_data_list.Append(base::Value::Dict().Set(SOURCE, SOURCE_LOGIN));
  view_data_list.Append(base::Value::Dict().Set(KEY_PAGE_URL, page_url.spec()));
  if (imf_info) {
    view_data_list.Append(
        base::Value::Dict().Set(KEY_IS_USER_SELECTED, imf_info->is_username));
    view_data_list.Append(base::Value::Dict().Set(KEY_IS_OTHER_ACCOUNT,
                                                  imf_info->is_other_account));
  }

  password_manager::ContentPasswordManagerDriver* pwd_manager_driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  std::unordered_map<std::string, autofill::InputFillRequestData> fillItem = {
      {KEY_USERNAME, username_data}, {KEY_PASSWORD, password_data}};
  for (auto item : fillItem) {
    base::Value::List list;
    list.Append(
        base::Value::Dict().Set(KEY_FOCUS, item.second.is_focused ? 1 : 0));
    if (pwd_manager_driver != nullptr) {
      item.second.bounds = TransformToRootCoordinates(pwd_manager_driver->render_frame_host(), item.second.bounds);
    }
    list.Append(base::Value::Dict().Set(
        KEY_RECT_X,
        static_cast<int32_t>((item.second.bounds.x() + offset.x()) * ratio)));
    list.Append(base::Value::Dict().Set(
        KEY_RECT_Y,
        static_cast<int32_t>((item.second.bounds.y() + offset.y() + viewport_height) * ratio)));
    list.Append(base::Value::Dict().Set(
        KEY_RECT_W, static_cast<int32_t>(item.second.bounds.width() * ratio)));
    list.Append(base::Value::Dict().Set(
        KEY_RECT_H, static_cast<int32_t>(item.second.bounds.height() * ratio)));
    if (base::ohos::IsPcDevice() && item.second.is_focused) {
      LOG(INFO) << "autofill request toJson, rect:"
                << base::WriteJson(list).value_or("null");
    }
    list.Append(base::Value::Dict().Set(KEY_VALUE,
                                        base::UTF16ToUTF8(item.second.value)));

    auto dict = base::Value::Dict().Set(item.first, std::move(list));
    view_data_list.Append(std::move(dict));
  }

  return base::WriteJson(view_data_list);
}

absl::optional<std::string> OhPasswordManagerClient::PasswordFormToJsonForSave(
    const password_manager::PasswordForm& form) {
  base::Value::List view_data_list;
  view_data_list.Append(base::Value::Dict().Set(EVENT, EVENT_SAVE));
  view_data_list.Append(base::Value::Dict().Set(SOURCE, SOURCE_LOGIN));
  view_data_list.Append(base::Value::Dict().Set(
      KEY_PAGE_URL, url::Origin::Create(form.url).GetURL().spec()));

  std::unordered_map<std::string, std::u16string> saveItem = {
      {KEY_USERNAME, form.username_value}, {KEY_PASSWORD, form.password_value}};
  for (auto item : saveItem) {
    base::Value::List list;
    list.Append(
        base::Value::Dict().Set(KEY_VALUE, base::UTF16ToUTF8(item.second)));
    auto dict = base::Value::Dict().Set(item.first, std::move(list));
    view_data_list.Append(std::move(dict));
  }

  return base::WriteJson(view_data_list);
}

void OhPasswordManagerClient::ProcessAutofillCancel(
    const std::string& fillContent) {
  // If the soft keyboard does not need to be restored, the fill cancel event
  // will not be processed.
  if (!is_need_restore_keyboard_) {
    LOG(INFO) << "don't need to handle the fill cancle event";
    return;
  }
  is_need_restore_keyboard_ = false;

  LOG(INFO) << "autofill handle fill cancel event";
  UnsuppressKeyboard();

  if (!web_contents()) {
    LOG(ERROR) << "web_contents is nullptr";
    return;
  }
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  if (!rfh || !rfh->IsActive()) {
    LOG(ERROR) << "rfh is nullptr or not active";
    return;
  }
  auto browser = CefBrowserHostBase::GetBrowserForHost(rfh);
  if (browser && browser->GetClient() &&
      browser->GetClient()->GetRenderHandler()) {
    browser->GetClient()->GetRenderHandler()->SetFillContent(fillContent);
  }
  auto* pwd_manager_driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (!pwd_manager_driver) {
    LOG(ERROR) << "pwd_manager_driver is nullptr";
    return;
  }

  // Will retry to pull up inputmethod soft keyboard.
  pwd_manager_driver->AutofillSurfaceClosed(true);
}

void OhPasswordManagerClient::AutoFillWithIMFEvent(bool is_username,
                                                   bool is_other_account,
                                                   bool is_new_password,
                                                   const std::string& content) {
  LOG(INFO) << "autofill with IMF event, flag=" << is_username
            << is_other_account << is_new_password;
  auto* autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents());
  if (!autofill_client) {
    LOG(ERROR) << " autofill_client is nullptr";
    return;
  }

  auto username = last_request_fill_username_;
  auto password = last_request_fill_password_;
  if (is_username) {
    username.value = base::UTF8ToUTF16(content);
  }
  AutofillIMFInfo imf_info = {
      is_username,
      is_other_account,
      is_new_password,
  };
  auto json_str = PasswordFormToJsonForRequest(EVENT_FILL, form_to_request_url_,
                                               username, password, &imf_info);
  if (json_str.has_value()) {
    LOG(INFO) << "call autofill for request from IMF";
    bool result = autofill_client->OnAutofillEvent(json_str.value());
    if (!result) {
      LOG(ERROR) << "failed to call autofill for request";
      return;
    }
    is_need_restore_keyboard_ = true;
  }
}

void OhPasswordManagerClient::FillData(const std::string& page_url,
                                       const std::string& username,
                                       const std::string& password,
                                       bool is_other_account) {
  UnsuppressKeyboard();
  is_need_restore_keyboard_ = false;
  auto username_id = last_request_fill_username_.field_renderer_id;
  auto password_id = last_request_fill_password_.field_renderer_id;
  auto digest = is_other_account
                    ? std::string()
                    : crypto::SHA256HashString(username + HASH_SALT + password);
  if (username_id) {
    auto_filled_forms_username_[*username_id] = digest;
  }
  if (password_id) {
    auto_filled_forms_password_[*password_id] = digest;
  }

  FillAccountSuggestion(GURL(page_url), base::UTF8ToUTF16(username),
                        base::UTF8ToUTF16(password));
}

void OhPasswordManagerClient::SuppressKeyboard() {
  if (!web_contents()) {
    LOG(ERROR) << "web_contents is nullptr";
    return;
  }
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  if (!rfh || !rfh->IsActive()) {
    LOG(ERROR) << "rfh is nullptr or not active";
    return;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver) {
    LOG(ERROR) << "autofill_driver is nullptr";
    return;
  }
  if (suppressed_driver_.get() == autofill_driver) {
    return;
  }
  UnsuppressKeyboard();
  LOG(INFO) << "Set the soft keyboard to be suppressd";
  autofill_driver->SetShouldSuppressKeyboard(true);
  suppressed_driver_ = autofill_driver;
  is_need_restore_keyboard_ = true;
  unsuppress_timer_.Start(FROM_HERE, base::Seconds(1), this,
                          &OhPasswordManagerClient::UnsuppressKeyboard);
}

void OhPasswordManagerClient::UnsuppressKeyboard() {
  if (!suppressed_driver_) {
    return;
  }
  LOG(INFO) << "Set the soft keyboard to be unsuppressd";
  suppressed_driver_->SetShouldSuppressKeyboard(false);
  suppressed_driver_ = nullptr;
  unsuppress_timer_.Stop();
}

bool OhPasswordManagerClient::IsLoginInfoConsistentWithFilled(
    const password_manager::PasswordForm& info) {
  auto username_id = info.username_element_renderer_id;
  auto password_id = info.password_element_renderer_id;
  AutofilledMap::iterator it;
  AutofilledMap* auto_filled_forms = nullptr;
  LOG(INFO) << "login autosave, username renderer_id:" << username_id
            << ", password renderer_id:" << password_id;
  if (password_id) {
    auto_filled_forms = &auto_filled_forms_password_;
    it = auto_filled_forms->find(*password_id);
  } else if (username_id) {
    auto_filled_forms = &auto_filled_forms_username_;
    it = auto_filled_forms->find(*username_id);
  }

  if (auto_filled_forms && it != auto_filled_forms->end() &&
      !it->second.empty()) {
    std::string login_digest = crypto::SHA256HashString(
        base::UTF16ToUTF8(info.username_value) + HASH_SALT +
        base::UTF16ToUTF8(info.password_value));
    if (it->second == login_digest) {
      auto_filled_forms->erase(it);
      return true;
    }
  }
  return false;
}

void OhPasswordManagerClient::UpdateLastRequestFilledItems(
    const autofill::InputFillRequestData& username_data,
    const autofill::InputFillRequestData& password_data) {
  last_request_fill_username_ = username_data;
  last_request_fill_password_ = password_data;
}

void OhPasswordManagerClient::NotifyAutofillPopupShow(bool is_show) {
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  auto driver = autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver) {
    LOG(ERROR) << "autofill_driver is nullptr";
    return;
  }
  auto autofill_manager =
      static_cast<autofill::OhAutofillManager*>(driver->oh_autofill_manager());
  if (!autofill_manager) {
    LOG(ERROR) << "autofill_manager is nullptr";
    return;
  }
  autofill_manager->SetPasswordPopupShow(is_show);

  auto autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents());
  if (autofill_client) {
    autofill_client->SetPasswordPopupStatus(is_show);
  }
}

void OhPasswordManagerClient::FillAccountSuggestion(
    const GURL& page_url,
    const std::u16string& username,
    const std::u16string& password) {
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  if (!driver) {
    return;
  }

  LOG(INFO) << "[Autofill] Try to fill account suggestion.";
  driver->FillAccountSuggestion(page_url, username, password);
}

void OhPasswordManagerClient::OnRequestAutofill(
    PasswordManagerDriver* driver,
    const GURL& page_url,
    autofill::FormRendererId form_id,
    const autofill::mojom::OhosPasswordFormAutofillState state,
    const autofill::InputFillRequestData& username_data,
    const autofill::InputFillRequestData& password_data) {
  LOG(INFO) << "[Autofill] On request autofill"
            << ", state=" << static_cast<int>(state)
            << ", username.field_renderer_id=" << username_data.field_renderer_id
            << ", username.bounds=" << username_data.bounds.ToString()
            << ", username.is_focus=" << username_data.is_focused
            << ", username.type=" << username_data.type
            << ", username.autocomplete_attr="
            << username_data.autocomplete_attr
            << ", password.field_renderer_id=" << password_data.field_renderer_id
            << ", password.bounds=" << password_data.bounds.ToString()
            << ", password.is_focus=" << password_data.is_focused
            << ", password.type=" << password_data.type
            << ", password.autocomplete_attr="
            << password_data.autocomplete_attr;

  auto* autofill_client =
      autofill::OhAutofillClient::FromWebContents(web_contents());
  if (!autofill_client) {
    LOG(ERROR) << "autofill_client is nullptr";
    return;
  }
  form_to_request_url_ = page_url;
  last_fill_form_id_ = form_id;
  last_fill_focus_renderer_id_ = username_data.is_focused
                                     ? username_data.field_renderer_id
                                     : password_data.field_renderer_id;

  if (!base::ohos::IsPcDevice()) {
    if (state == OhosPasswordFormAutofillState::kNotRequested) {
      auto json_str = PasswordFormToJsonForRequest(
          EVENT_FILL, page_url, username_data, password_data);
      if (json_str.has_value()) {
        LOG(INFO) << "call autofill for request from system, form_id="
                  << form_id;
        bool result = autofill_client->OnAutofillEvent(json_str.value());
        if (!result) {
          LOG(ERROR) << "failed to call autofill for request";
          return;
        }
        SuppressKeyboard();
        UpdateLastRequestFilledItems(username_data, password_data);
      }
    }
  } else {
    auto event = (state == OhosPasswordFormAutofillState::kTextChanged)
                     ? EVENT_UPDATE
                     : EVENT_FILL;
    auto json_str = PasswordFormToJsonForRequest(event, page_url, username_data,
                                                 password_data);
    if (json_str.has_value()) {
      LOG(INFO) << "call autofill for request from system, state="
                << static_cast<int32_t>(state);
      bool result = autofill_client->OnAutofillEvent(json_str.value());
      if (!result) {
        LOG(ERROR) << "failed to call autofill for request";
        return;
      }
      NotifyAutofillPopupShow(true);
      UpdateLastRequestFilledItems(username_data, password_data);
    }
  }
}
#endif

void OhPasswordManagerClient::UpdateFormManagers() {
  password_manager_.UpdateFormManagers();
}

void OhPasswordManagerClient::NavigateToManagePasswordsPage(
    password_manager::ManagePasswordsReferrer referrer) {}

bool OhPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  // TODO(crbug.com/862989): Move the following function (and the feature) to
  // the password component. Then remove IsIsolationForPasswordsSitesEnabled()
  // from the PasswordManagerClient interface.
  return false;
}
bool OhPasswordManagerClient::IsNewTabPage() const {
  return false;
}

// static
void OhPasswordManagerClient::BindCredentialManager(
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (!web_contents) {
    return;
  }

  if (web_contents->GetPrimaryMainFrame() != render_frame_host) {
    return;
  }

  OhPasswordManagerClient* instance =
      OhPasswordManagerClient::FromWebContents(web_contents);

  // Try to bind to the driver, but if driver is not available for this render
  // frame host, the request will be just dropped. This will cause the message
  // pipe to be closed, which will raise a connection error on the peer side.
  if (!instance) {
    return;
  }
}

// static
bool OhPasswordManagerClient::CanShowBubbleOnURL(const GURL& url) {
  std::string scheme = url.scheme();
  return (content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
      scheme));
}

void OhPasswordManagerClient::GenerationResultAvailable(
    PasswordGenerationType type,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    const absl::optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (!ui_data || !driver) {
    return;
  }
  // Check the data because it's a Mojo callback and the input isn't trusted.
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          driver->render_frame_host(), ui_data->form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP)) {
    return;
  }

  ShowPasswordGenerationPopup(type, driver.get(), *ui_data);
}

void OhPasswordManagerClient::ShowPasswordGenerationPopup(
    PasswordGenerationType type,
    password_manager::ContentPasswordManagerDriver* driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  DCHECK(driver);

  if (!driver) {
    return;
  }

  gfx::RectF element_bounds_in_top_frame_space =
      TransformToRootCoordinates(driver->render_frame_host(), ui_data.bounds);

  // Only show password suggestions iff the field is of password type.
  bool show_password_suggestions = ui_data.is_generation_element_password_type;
  if (driver->GetPasswordAutofillManager()
          ->MaybeShowPasswordSuggestionsWithGeneration(
              element_bounds_in_top_frame_space, ui_data.text_direction,
              show_password_suggestions)) {
    return;
  }
}

void OhPasswordManagerClient::FocusedInputChanged(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {}

gfx::RectF OhPasswordManagerClient::TransformToRootCoordinates(
    content::RenderFrameHost* frame_host,
    const gfx::RectF& bounds_in_frame_coordinates) {
  content::RenderWidgetHostView* rwhv = frame_host->GetView();
  if (!rwhv) {
    return bounds_in_frame_coordinates;
  }
  return gfx::RectF(rwhv->TransformPointToRootCoordSpaceF(
                        bounds_in_frame_coordinates.origin()),
                    bounds_in_frame_coordinates.size());
}

void OhPasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move) {}

bool OhPasswordManagerClient::IsCommittedMainFrameSecure() const {
  return network::IsOriginPotentiallyTrustworthy(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

const GURL& OhPasswordManagerClient::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

url::Origin OhPasswordManagerClient::GetLastCommittedOrigin() const {
  DCHECK(web_contents());
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhPasswordManagerClient);
