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
 *
 * Based on android_autofill_manager.h originally written by
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CEF_LIBCEF_BROWSER_AUTOFILL_OH_AUTOFILL_MANAGER_H_
#define CEF_LIBCEF_BROWSER_AUTOFILL_OH_AUTOFILL_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/dense_set.h"
#include "libcef/browser/autofill/oh_autofill_provider.h"

namespace autofill {

class AutofillProvider;
class ContentAutofillDriver;
// Creates an OhAutofillManager and attaches it to the `driver`.
//
// This hook is to be passed to CreateForWebContentsAndDelegate().
// It is the glue between ContentAutofillDriver[Factory] and
// OhAutofillManager.
//
// Other embedders (which don't want to use OhAutofillManager) shall use
// other implementations.
void OhDriverInitHook(AutofillClient* client, ContentAutofillDriver* driver);
// This class forwards AutofillManager calls to AutofillProvider.
class OhAutofillManager : public AutofillManager {
 public:
  OhAutofillManager(const OhAutofillManager&) = delete;
  OhAutofillManager& operator=(const OhAutofillManager&) = delete;

  ~OhAutofillManager() override;

  base::WeakPtr<OhAutofillManager> GetWeakPtrToLeafClass() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  base::WeakPtr<AutofillManager> GetWeakPtr() override;
  CreditCardAccessManager* GetCreditCardAccessManager() override;

  bool ShouldClearPreviewedForm() override;

  void FillCreditCardFormImpl(const FormData& form,
                              const FormFieldData& field,
                              const CreditCard& credit_card,
                              const std::u16string& cvc,
                              AutofillTriggerSource trigger_source) override;
  void FillProfileFormImpl(const FormData& form,
                           const FormFieldData& field,
                           const autofill::AutofillProfile& profile,
                           AutofillTriggerSource trigger_source) override;

  void OnFocusNoLongerOnFormImpl(bool had_interacted_form) override;

  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;

  void OnDidPreviewAutofillFormDataImpl() override {}
  void OnDidEndTextFieldEditingImpl() override {}
  void OnHidePopupImpl() override;
  void OnSelectFieldOptionsDidChangeImpl(const FormData& form) override {}

  void Reset() override;
  void OnContextMenuShownInField(const FormGlobalId& form_global_id,
                                 const FieldGlobalId& field_global_id) override;

  void ReportAutofillWebOTPMetrics(bool used_web_otp) override {}

  bool has_server_prediction() const { return has_server_prediction_; }

  // Send the |form| to the renderer for the specified |action|.
  //
  // |triggered_origin| is the origin of the field from which the autofill is
  // triggered; this affects the security policy for cross-frame fills. See
  // AutofillDriver::FillOrPreviewForm() for further details.
  void FillOrPreviewForm(mojom::RendererFormDataAction action,
                         const FormData& form,
                         const FieldTypeGroup field_type_group,
                         const url::Origin& triggered_origin);

  absl::optional<std::string> FormDataToJson(const FormData& form,
                                             const FormFieldData& field,
                                             const std::string& event);
  absl::optional<std::string> FormDataToJsonForSave(const FormData& form);
  void FillData(const std::string& json_str);
  std::string GetAttributeOrUniqueId(const FormFieldData& field);
  void FillDataWithId(const base::Value::Dict* dict);
  bool isFocusField(const FormFieldData& field_data,
                    const FormFieldData& field);

#if defined(OHOS_PASSWORD_AUTOFILL)
  void ForwardDataToPasswordManager(const std::string& page_url,
                                    const std::string& username,
                                    const std::string& password,
                                    bool is_other_account);

  bool IsUsernamePasswordFormField(FormRendererId form_id,
                                   FieldRendererId field_id);
  void SetPasswordPopupShow(bool is_show) { is_password_popup_show_ = is_show; }

  absl::optional<std::string> QueryPopupShowAndGetHideStr();
#endif

 protected:
  friend void OhDriverInitHook(AutofillClient* client,
                               ContentAutofillDriver* driver);

  OhAutofillManager(AutofillDriver* driver, AutofillClient* client);

  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;

  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;

  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;

  void OnAskForValuesToFillImpl(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;

  void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override {}
  bool ShouldParseForms() override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;

  void OnAfterProcessParsedForms(
      const DenseSet<FormType>& form_types) override {}

  void PropagateAutofillPredictions(
      const std::vector<FormStructure*>& forms) override;

  void OnServerRequestError(FormSignature form_signature,
                            AutofillDownloadManager::RequestType request_type,
                            int http_error) override;

 protected:
#ifdef UNIT_TEST
  // For the unit tests where WebContents isn't available.
  void set_autofill_provider_for_testing(AutofillProvider* autofill_provider) {
    autofill_provider_for_testing_ = autofill_provider;
  }
#endif  // UNIT_TEST

 private:
  AutofillProvider* GetAutofillProvider();

  void StartNewLoggingSession();
  bool has_server_prediction_ = false;
  bool is_show_ = false;
  bool is_password_popup_show_ = false;
  std::unique_ptr<FormData> form_;
  base::WeakPtrFactory<OhAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif
