// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/context_menu_params_impl.h"

#include "base/logging.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

CefContextMenuParamsImpl::CefContextMenuParamsImpl(
    content::ContextMenuParams* value)
    : CefValueBase<CefContextMenuParamsExt, content::ContextMenuParams>(
          value,
          nullptr,
          kOwnerNoDelete,
          true,
          new CefValueControllerNonThreadSafe()) {
  // Indicate that this object owns the controller.
  SetOwnsController();
}

int CefContextMenuParamsImpl::GetXCoord() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().x;
}

int CefContextMenuParamsImpl::GetYCoord() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().y;
}

CefContextMenuParamsImpl::TypeFlags CefContextMenuParamsImpl::GetTypeFlags() {
  CEF_VALUE_VERIFY_RETURN(false, CM_TYPEFLAG_NONE);
  const content::ContextMenuParams& params = const_value();
  int type_flags = CM_TYPEFLAG_NONE;
  if (!params.page_url.is_empty()) {
    type_flags |= CM_TYPEFLAG_PAGE;
  }
  if (!params.frame_url.is_empty()) {
    type_flags |= CM_TYPEFLAG_FRAME;
  }
  if (!params.link_url.is_empty()) {
    type_flags |= CM_TYPEFLAG_LINK;
  }
  if (params.media_type != blink::mojom::ContextMenuDataMediaType::kNone) {
    type_flags |= CM_TYPEFLAG_MEDIA;
  }
  if (!params.selection_text.empty()) {
    type_flags |= CM_TYPEFLAG_SELECTION;
  }
  if (params.is_editable) {
    type_flags |= CM_TYPEFLAG_EDITABLE;
  }
  return static_cast<TypeFlags>(type_flags);
}

CefString CefContextMenuParamsImpl::GetLinkUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().link_url.spec();
}

CefString CefContextMenuParamsImpl::GetUnfilteredLinkUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().unfiltered_link_url.spec();
}

CefString CefContextMenuParamsImpl::GetSourceUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().src_url.spec();
}

bool CefContextMenuParamsImpl::HasImageContents() {
  CEF_VALUE_VERIFY_RETURN(false, true);
  return const_value().has_image_contents;
}

CefString CefContextMenuParamsImpl::GetTitleText() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().title_text;
}

CefString CefContextMenuParamsImpl::GetPageUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().page_url.spec();
}

CefString CefContextMenuParamsImpl::GetFrameUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().frame_url.spec();
}

CefString CefContextMenuParamsImpl::GetFrameCharset() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().frame_charset;
}

CefContextMenuParamsImpl::MediaType CefContextMenuParamsImpl::GetMediaType() {
  CEF_VALUE_VERIFY_RETURN(false, CM_MEDIATYPE_NONE);
  return static_cast<MediaType>(const_value().media_type);
}

CefContextMenuParamsImpl::MediaStateFlags
CefContextMenuParamsImpl::GetMediaStateFlags() {
  CEF_VALUE_VERIFY_RETURN(false, CM_MEDIAFLAG_NONE);
  return static_cast<MediaStateFlags>(const_value().media_flags);
}

CefString CefContextMenuParamsImpl::GetSelectionText() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().selection_text;
}

CefString CefContextMenuParamsImpl::GetMisspelledWord() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().misspelled_word;
}

bool CefContextMenuParamsImpl::GetDictionarySuggestions(
    std::vector<CefString>& suggestions) {
  CEF_VALUE_VERIFY_RETURN(false, false);

  if (!suggestions.empty()) {
    suggestions.clear();
  }

  if (const_value().dictionary_suggestions.empty()) {
    return false;
  }

  std::vector<std::u16string>::const_iterator it =
      const_value().dictionary_suggestions.begin();
  for (; it != const_value().dictionary_suggestions.end(); ++it) {
    suggestions.push_back(*it);
  }

  return true;
}

bool CefContextMenuParamsImpl::IsEditable() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().is_editable;
}

bool CefContextMenuParamsImpl::IsSpellCheckEnabled() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().spellcheck_enabled;
}

CefContextMenuParamsImpl::EditStateFlags
CefContextMenuParamsImpl::GetEditStateFlags() {
  CEF_VALUE_VERIFY_RETURN(false, CM_EDITFLAG_NONE);
  return static_cast<EditStateFlags>(const_value().edit_flags);
}

bool CefContextMenuParamsImpl::IsCustomMenu() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return !const_value().custom_items.empty();
}

#if BUILDFLAG(ARKWEB_CLIPBOARD)
CefContextMenuParamsImpl::InputFieldType
CefContextMenuParamsImpl::ConventInputField(
    blink::mojom::FormControlType formType) {
  switch (formType) {
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputUrl:
      return CefContextMenuParamsImpl::InputFieldType::
          CM_INPUTFIELDTYPE_PLAINTEXT;
    case blink::mojom::FormControlType::kInputPassword:
      return CefContextMenuParamsImpl::InputFieldType::
          CM_INPUTFIELDTYPE_PASSWORD;
    case blink::mojom::FormControlType::kInputNumber:
      return CefContextMenuParamsImpl::InputFieldType::CM_INPUTFIELDTYPE_NUMBER;
    case blink::mojom::FormControlType::kInputTelephone:
      return CefContextMenuParamsImpl::InputFieldType::
          CM_INPUTFIELDTYPE_TELEPHONE;
    default:
      return CefContextMenuParamsImpl::InputFieldType::CM_INPUTFIELDTYPE_OTHER;
  }
}

CefContextMenuParamsImpl::InputFieldType
CefContextMenuParamsImpl::GetInputFieldType() {
  CEF_VALUE_VERIFY_RETURN(false, CM_INPUTFIELDTYPE_NONE);
  if (!const_value().form_control_type.has_value()) {
    return CM_INPUTFIELDTYPE_NONE;
  }
  return ConventInputField(const_value().form_control_type.value());
}

CefContextMenuParamsImpl::SourceType CefContextMenuParamsImpl::GetSourceType() {
  CEF_VALUE_VERIFY_RETURN(false, CM_SOURCETYPE_NONE);
  return static_cast<SourceType>(const_value().source_type);
}
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void CefContextMenuParamsImpl::GetImageRect(int& x, int& y, int& w, int& h) {
  x = static_cast<int32_t>(const_value().image_rect.x());
  y = static_cast<int32_t>(const_value().image_rect.y());
  w = static_cast<int32_t>(const_value().image_rect.width());
  h = static_cast<int32_t>(const_value().image_rect.height());
}
#endif
