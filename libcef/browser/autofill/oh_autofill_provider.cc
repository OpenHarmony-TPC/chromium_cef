// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cef/libcef/browser/autofill/oh_autofill_provider.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhAutofillProvider);

OhAutofillProvider::OhAutofillProvider(content::WebContents* web_contents)
    : content::WebContentsUserData<OhAutofillProvider>(*web_contents) {
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

OhAutofillProvider::~OhAutofillProvider() = default;

void OhAutofillProvider::OnAskForValuesToFill(
    OhAutofillManager* manager,
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  LOG(INFO) << "OhAutofillProvider::OnAskForValuesToFill";
}

void OhAutofillProvider::OnTextFieldDidChange(OhAutofillManager* manager,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box,
                                              const base::TimeTicks timestamp) {
}

void OhAutofillProvider::OnTextFieldDidScroll(OhAutofillManager* manager,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {}

void OhAutofillProvider::OnSelectControlDidChange(
    OhAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {}

void OhAutofillProvider::OnFormSubmitted(OhAutofillManager* manager,
                                         const FormData& form,
                                         bool known_success,
                                         mojom::SubmissionSource source) {}

void OhAutofillProvider::OnFocusNoLongerOnForm(OhAutofillManager* manager,
                                               bool had_interacted_form) {}

void OhAutofillProvider::OnFocusOnFormField(OhAutofillManager* manager,
                                            const FormData& form,
                                            const FormFieldData& field,
                                            const gfx::RectF& bounding_box) {}

void OhAutofillProvider::OnDidFillAutofillFormData(OhAutofillManager* manager,
                                                   const FormData& form,
                                                   base::TimeTicks timestamp) {}

void OhAutofillProvider::OnFormsSeen(OhAutofillManager* manager,
                                     const std::vector<FormData>& forms) {}

void OhAutofillProvider::OnHidePopup(OhAutofillManager* manager) {}

void OhAutofillProvider::OnServerPredictionsAvailable(
    OhAutofillManager* manager) {}

void OhAutofillProvider::OnServerQueryRequestError(
    OhAutofillManager* manager,
    FormSignature form_signature) {}

void OhAutofillProvider::Reset(OhAutofillManager* manager) {}

void OhAutofillProvider::FillOrPreviewForm(OhAutofillManager* manager,
                                           int requestId,
                                           const FormData& formData) {}

void OhAutofillProvider::RendererShouldAcceptDataListSuggestion(
    OhAutofillManager* manager,
    const FieldGlobalId& field_id,
    const std::u16string& value) {}

}  // namespace autofill
