/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#if BUILDFLAG(ARKWEB_CLIPBOARD)
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_MENU)
#include "content/public/common/referrer.h"
#include "third_party/blink/public/common/loader/network_utils.h"

using blink::mojom::ContextMenuDataMediaType;
#endif

namespace {

#if BUILDFLAG(ARKWEB_CLIPBOARD)
constexpr cef_context_menu_edit_state_flags_t kMenuCommands[] = {
    CM_EDITFLAG_CAN_CUT, CM_EDITFLAG_CAN_COPY, CM_EDITFLAG_CAN_PASTE,
    CM_EDITFLAG_CAN_DELETE, CM_EDITFLAG_CAN_SELECT_ALL};
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_MENU)
content::Referrer CreateReferrer(const GURL& url,
                                 const content::ContextMenuParams& params) {
  const GURL& referring_url = params.frame_url;
  return content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
}
#endif

}  // namespace

#if BUILDFLAG(ARKWEB_MENU)
void CefMenuManager::ExecuteSaveImage() {
  if (!web_contents()) {
    LOG(ERROR) << "web_contents is nullptr";
    return;
  }

  content::RenderFrameHost* frame_host = web_contents()->GetFocusedFrame();
  bool is_large_data_url = params_.has_image_contents && params_.src_url.is_empty();
  if (params_.media_type == ContextMenuDataMediaType::kCanvas ||
      (params_.media_type == ContextMenuDataMediaType::kImage && is_large_data_url)) {
    if (!frame_host) {
      LOG(ERROR) << "frame_host is nullptr";
      return;
    }
    LOG(INFO) << "save canvas image or large image";
    frame_host->SaveImageAt(params_.x, params_.y);
    return;
  }

  bool is_plugin = params_.media_type == ContextMenuDataMediaType::kPlugin;
  content::RenderFrameHost* target_frame_host = is_plugin ?
      web_contents()->GetOuterWebContentsFrame() : frame_host;
  if (!target_frame_host) {
    LOG(ERROR) << "target_frame_host is nullptr, is_plugin: " << is_plugin;
    return;
  }

  if (params_.media_type == ContextMenuDataMediaType::kImage) {
    LOG(INFO) << "save small image";
    GURL url = params_.src_url;
    content::Referrer referrer = CreateReferrer(url, params_);
    net::HttpRequestHeaders headers;
    headers.SetHeaderIfMissing(net::HttpRequestHeaders::kAccept,
                               blink::network_utils::ImageAcceptHeader());
    bool is_subresource = params_.media_type != ContextMenuDataMediaType::kNone &&
                          !params_.is_image_media_plugin_document;
    web_contents()->SaveFrameWithHeaders(url, referrer, headers.ToString(),
                                         params_.suggested_filename,
                                         target_frame_host, is_subresource);
  }
}

void CefMenuManager::ExecuteCopyImageAt() {
  if (!web_contents()) {
    LOG(ERROR) << "web_contents is nullptr";
    return;
  }

  content::RenderFrameHost* frame_host = web_contents()->GetFocusedFrame();
  if (!frame_host) {
    LOG(ERROR) << "frame_host is nullptr";
    return;
  }
  LOG(INFO) << "copy image at (" << params_.x << params_.y << ", " << ")";
  frame_host->CopyImageAt(params_.x, params_.y);
}
#endif

#if BUILDFLAG(ARKWEB_CLIPBOARD)
bool CefMenuManager::IsCommandIdEnabled(
    int command_id,
    content::ContextMenuParams& params) const {
  bool editable = params.is_editable;
  switch (command_id) {
    case CM_EDITFLAG_CAN_CUT:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_CUT);
    case CM_EDITFLAG_CAN_DELETE:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_DELETE);
    case CM_EDITFLAG_CAN_COPY:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_COPY);
    case CM_EDITFLAG_CAN_PASTE: {
      bool can_paste = false;
      if (!editable) {
        LOG(INFO) << "This area is not editable.";
        return can_paste;
      }
      can_paste = ui::Clipboard::GetForCurrentThread()->HasPasteData();
      return can_paste;
    }
    case CM_EDITFLAG_CAN_SELECT_ALL:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_SELECT_ALL);
    default:
      return false;
  }
}

void CefMenuManager::UpdateMenuEditStateFlags(
    content::ContextMenuParams& params) {
  int menu_flags = 0;
  for (const auto& command : kMenuCommands) {
    if (IsCommandIdEnabled(command, params)) {
      menu_flags |= command;
    }
  }

  params.edit_flags = menu_flags;
}
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)
