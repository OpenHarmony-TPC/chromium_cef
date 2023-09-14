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

#include "base/memory/ptr_util.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

using base::TimeTicks;

// static
std::unique_ptr<AutofillManager> OhAutofillManager::Create(
    AutofillDriver* driver,
    AutofillClient* client,
    const std::string& /*app_locale*/,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  return base::WrapUnique(
      new OhAutofillManager(driver, client, enable_download_manager));
}

OhAutofillManager::OhAutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : AutofillManager(driver,
                      client,
                      enable_download_manager,
                      version_info::Channel::UNKNOWN) {}

OhAutofillManager::~OhAutofillManager() = default;

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
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  LOG(INFO) << "OnAskForValuesToFillImpl";
  auto* provider = GetAutofillProvider();
  if (!provider) {
    LOG(ERROR) << "OhAutofillProvider not found";
    return;
  }

  provider->OnAskForValuesToFill(this, query_id, form, field, bounding_box,
                                 autoselect_first_suggestion);
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

bool OhAutofillManager::ShouldParseForms(const std::vector<FormData>& forms) {
  LOG(INFO) << "ShouldParseForms";

  // Need to parse the |forms| to FormStructure, so heuristic_type can be
  // retrieved later.
  return true;
}

void OhAutofillManager::OnFocusNoLongerOnForm(bool had_interacted_form) {
  LOG(INFO) << "OnFocusNoLongerOnForm";
}

void OhAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  LOG(INFO) << "OnDidFillAutofillFormData";
}

void OhAutofillManager::OnHidePopup() {
  LOG(INFO) << "OnHidePopup";
}

void OhAutofillManager::SelectFieldOptionsDidChange(const FormData& form) {
  LOG(INFO) << "SelectFieldOptionsDidChange";
}

void OhAutofillManager::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  has_server_prediction_ = true;
  LOG(INFO) << "PropagateAutofillPredictions";
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

OhAutofillProvider* OhAutofillManager::GetAutofillProvider() {
  if (auto* rfh =
          static_cast<ContentAutofillDriver*>(driver())->render_frame_host()) {
    if (rfh->IsActive()) {
      if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
        return OhAutofillProvider::FromWebContents(web_contents);
      }
    }
  }
  return nullptr;
}

void OhAutofillManager::FillOrPreviewForm(int query_id,
                                          mojom::RendererFormDataAction action,
                                          const FormData& form) {
  LOG(INFO) << "FillOrPreviewForm";
  driver()->FillOrPreviewForm(query_id, action, form, form.main_frame_origin,
                              {});
}

}  // namespace autofill
