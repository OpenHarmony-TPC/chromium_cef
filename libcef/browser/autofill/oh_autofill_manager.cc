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
#include "libcef/browser/autofill/oh_autofill_manager.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

using base::TimeTicks;

void OhDriverInitHook(AutofillClient* client, ContentAutofillDriver* driver) {
  driver->set_autofill_manager(
      base::WrapUnique(new OhAutofillManager(driver, client)));
  driver->GetAutofillAgent()->SetUserGestureRequired(false);
  driver->GetAutofillAgent()->SetSecureContextRequired(true);
  driver->GetAutofillAgent()->SetFocusRequiresScroll(false);
  driver->GetAutofillAgent()->SetQueryPasswordSuggestion(true);
}

OhAutofillManager::OhAutofillManager(AutofillDriver* driver,
                                     AutofillClient* client)
    : AutofillManager(driver, client) {
  StartNewLoggingSession();
}

OhAutofillManager::~OhAutofillManager() = default;

base::WeakPtr<AutofillManager> OhAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager* OhAutofillManager::GetCreditCardAccessManager() {
  return nullptr;
}

bool OhAutofillManager::ShouldClearPreviewedForm() {
  return false;
}

void OhAutofillManager::FillCreditCardFormImpl(
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card,
    const std::u16string& cvc,
    AutofillTriggerSource trigger_source) {
  NOTREACHED();
}

void OhAutofillManager::FillProfileFormImpl(
    const FormData& form,
    const FormFieldData& field,
    const autofill::AutofillProfile& profile,
    AutofillTriggerSource trigger_source) {
  NOTREACHED();
}

void OhAutofillManager::OnFormSubmittedImpl(const FormData& form,
                                            bool known_success,
                                            mojom::SubmissionSource source) {
  LOG(INFO) << "OnFormSubmittedImpl";
}

void OhAutofillManager::OnTextFieldDidChangeImpl(const FormData& form,
                                                 const FormFieldData& field,
                                                 const gfx::RectF& bounding_box,
                                                 const TimeTicks timestamp) {
  LOG(INFO) << "OnTextFieldDidChangeImpl";
}

void OhAutofillManager::OnTextFieldDidScrollImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  LOG(INFO) << "OnTextFieldDidScrollImpl";
}

void OhAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    FormElementWasClicked form_element_was_clicked) {
  LOG(INFO) << "OnAskForValuesToFillImpl";
}

void OhAutofillManager::OnFocusOnFormFieldImpl(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  LOG(INFO) << "OnFocusOnFormFieldImpl";
}

void OhAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  LOG(INFO) << "OnSelectControlDidChangeImpl";
}

bool OhAutofillManager::ShouldParseForms() {
  LOG(INFO) << "ShouldParseForms";
  return true;
}

void OhAutofillManager::OnFocusNoLongerOnFormImpl(bool had_interacted_form) {
  LOG(INFO) << "OnFocusNoLongerOnFormImpl";
}

void OhAutofillManager::OnDidFillAutofillFormDataImpl(
    const FormData& form,
    const base::TimeTicks timestamp) {
  LOG(INFO) << "OnDidFillAutofillFormDataImpl";
}

void OhAutofillManager::OnHidePopupImpl() {
  LOG(INFO) << "OnHidePopupImpl";
}

void OhAutofillManager::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
  LOG(INFO) << "PropagateAutofillPredictions";
}

void OhAutofillManager::OnFormProcessed(const FormData& form,
                                        const FormStructure& form_structure) {
  LOG(INFO) << "OnFormProcessed";
}

void OhAutofillManager::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {
  LOG(INFO) << "OnServerRequestError";
}

void OhAutofillManager::Reset() {
  AutofillManager::Reset();
  has_server_prediction_ = false;
}

void OhAutofillManager::OnContextMenuShownInField(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  // Not relevant for Android. Only called via context menu in Desktop.
  NOTREACHED();
}

AutofillProvider* OhAutofillManager::GetAutofillProvider() {
  if (auto* rfh =
          static_cast<ContentAutofillDriver*>(driver())->render_frame_host()) {
    if (rfh->IsActive()) {
      if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
        return AutofillProvider::FromWebContents(web_contents);
      }
    }
  }
  return nullptr;
}

void OhAutofillManager::FillOrPreviewForm(mojom::RendererFormDataAction action,
                                          const FormData& form,
                                          FieldTypeGroup field_type_group,
                                          const url::Origin& triggered_origin) {
  LOG(INFO) << "FillOrPreviewForm";
}

void OhAutofillManager::StartNewLoggingSession() {}

}  // namespace autofill
