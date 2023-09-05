/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 */

#include "libcef/browser/password/oh_password_manager_client.h"

#include <memory>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/version_info/version_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/clipboard/clipboard.h"
#include "url/url_constants.h"

#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
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

// static
void OhPasswordManagerClient::CreateForWebContentsWithAutofillClient(
    content::WebContents* contents,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(contents))
    return;

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

bool OhPasswordManagerClient::IsFillingFallbackEnabled(const GURL& url) const {
  return IsFillingEnabled(url);
}

bool OhPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
#if defined(OHOS_NWEB_EX)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser)) {
    return false;
  }
  std::string full_url;
  if (form_to_save != nullptr) {
    full_url = form_to_save->GetPendingCredentials().url.spec();
  }
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());
  if (!web_contents_impl->GetSavePassword()) {
    return false;
  }
  // The save password infobar and the password bubble prompt in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return false;

  if (form_to_save->IsBlocklisted())
    return false;

  web_contents_impl->PromptSaveOrUpdatePassword(update_password ? true : false,
                                                std::move(form_to_save));
  return true;
#else
  return false;
#endif
}

password_manager::PasswordStore*
OhPasswordManagerClient::GetProfilePasswordStore() const {
  return OhPasswordStoreFactory::GetPasswordStoreForContext(
             context_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStore*
OhPasswordManagerClient::GetAccountPasswordStore() const {
  return nullptr;
}

password_manager::PasswordReuseManager*
OhPasswordManagerClient::GetPasswordReuseManager() const {
  return nullptr;
}

password_manager::PasswordScriptsFetcher*
OhPasswordManagerClient::GetPasswordScriptsFetcher() {
  return nullptr;
}

password_manager::PasswordChangeSuccessTracker*
OhPasswordManagerClient::GetPasswordChangeSuccessTracker() {
  return nullptr;
}

password_manager::FieldInfoManager*
OhPasswordManagerClient::GetFieldInfoManager() const {
  return nullptr;
}

void OhPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;
}

void OhPasswordManagerClient::HideManualFallbackForSaving() {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;
}

bool OhPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  return true;
}

void OhPasswordManagerClient::ShowTouchToFill(PasswordManagerDriver* driver) {}

void OhPasswordManagerClient::OnPasswordSelected(const std::u16string& text) {}

scoped_refptr<device_reauth::BiometricAuthenticator>
OhPasswordManagerClient::GetBiometricAuthenticator() {
  scoped_refptr<device_reauth::BiometricAuthenticator> biometric_authenticator;
  return biometric_authenticator;
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
    const std::vector<const password_manager::PasswordForm*>& best_matches,
    const url::Origin& origin,
    const std::vector<const PasswordForm*>* federated_matches) {}

void OhPasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const password_manager::PasswordFormManagerForUI* form_manager) {
  httpauth_manager_.Autofill(preferred_match, form_manager);
  DCHECK(!form_manager->GetBestMatches().empty());
  PasswordWasAutofilled(form_manager->GetBestMatches(),
                        url::Origin::Create(form_manager->GetURL()), nullptr);
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

void OhPasswordManagerClient::LogPasswordReuseDetectedEvent() {}

void OhPasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {}

ukm::SourceId OhPasswordManagerClient::GetUkmSourceId() {
  return ukm::GetSourceIdForWebContentsDocument(web_contents());
}

PasswordManagerMetricsRecorder* OhPasswordManagerClient::GetMetricsRecorder() {
  return base::OptionalOrNullptr(metrics_recorder_);
}

void OhPasswordManagerClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

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
  if (!entry)
    return false;
  int http_status_code = entry->GetHttpStatusCode();

  if (logger)
    logger->LogNumber(Logger::STRING_HTTP_STATUS_CODE, http_status_code);

  if (http_status_code >= 400 && http_status_code < 600) {
    //#ifdef HW_BUILD_PASSWORD
    LOG(WARNING) << "[PasswordSave] Last navigation occur http error, "
                    "http_status_code is: "
                 << http_status_code;
    //#endif
    return true;
  }
  return false;
}

net::CertStatus OhPasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return 0;
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
        factory->DriverForFrame(web_contents()->GetMainFrame());
    // |driver| can be NULL if the tab is being closed.
    if (driver) {
      autofill::AutofillManager* autofill_manager = driver->autofill_manager();
      if (autofill_manager)
        return autofill_manager->download_manager();
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
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED))
    return;

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
          BadMessageReason::CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD))
    return;

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
          BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED))
    return;

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
  if (!entry)
    return;

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

void OhPasswordManagerClient::CheckProtectedPasswordEntry(
    PasswordType password_type,
    const std::string& username,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists) {}

const autofill::LogManager* OhPasswordManagerClient::GetLogManager() const {
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
  if (render_frame_host->GetParent())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (!web_contents)
    return;

  if (web_contents->GetMainFrame() != render_frame_host)
    return;

  OhPasswordManagerClient* instance =
      OhPasswordManagerClient::FromWebContents(web_contents);

  // Try to bind to the driver, but if driver is not available for this render
  // frame host, the request will be just dropped. This will cause the message
  // pipe to be closed, which will raise a connection error on the peer side.
  if (!instance)
    return;
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
  if (!ui_data || !driver)
    return;
  // Check the data because it's a Mojo callback and the input isn't trusted.
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          driver->render_frame_host(), ui_data->form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP))
    return;

  ShowPasswordGenerationPopup(type, driver.get(), *ui_data);
}

void OhPasswordManagerClient::ShowPasswordGenerationPopup(
    PasswordGenerationType type,
    password_manager::ContentPasswordManagerDriver* driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  DCHECK(driver);

  if (!driver)
    return;

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
  if (!rwhv)
    return bounds_in_frame_coordinates;
  return gfx::RectF(rwhv->TransformPointToRootCoordSpaceF(
                        bounds_in_frame_coordinates.origin()),
                    bounds_in_frame_coordinates.size());
}

void OhPasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move) {}

bool OhPasswordManagerClient::IsAutofillAssistantUIVisible() const {
  return false;
}

const GURL& OhPasswordManagerClient::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

url::Origin OhPasswordManagerClient::GetLastCommittedOrigin() const {
  DCHECK(web_contents());
  return web_contents()->GetMainFrame()->GetLastCommittedOrigin();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhPasswordManagerClient);
