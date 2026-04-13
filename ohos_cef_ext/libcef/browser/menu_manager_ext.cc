  /*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
 
#include "menu_manager_ext.h"

#include "base/logging.h"
#include "cef/libcef/browser/menu_manager.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
 
 
#if BUILDFLAG(ARKWEB_DEVTOOLS)
CefMenuManagerExt::CefMenuManagerExt(raw_ptr<CefMenuManager> menu_manager)
    : menu_manager_(menu_manager) {}

void CefMenuManagerExt::SetMenuItems(content::WebContents* contents,
                                     const content::ContextMenuParams& params) {
  LOG(INFO) << "CefMenuManagerExt::SetMenuItems";
  contents_ = contents;
  link_followed_ = params.link_followed;
  if (model_ == nullptr) {
    model_ = new CefMenuModelImpl(menu_manager_, nullptr, false);
  }

  for (auto& item : params.custom_items) {
    auto new_item = item->Clone();
    model_->AddMenuItem(*new_item);
  }
}

CefRefPtr<CefMenuModelImpl> CefMenuManagerExt::GetContextMenuModel() {
  LOG(INFO) << "CefMenuManagerExt::GetContextMenuModel model size: " << model_->GetCount();
  return model_;
}
 
void CefMenuManagerExt::onContextMenuSelected(int command_id) {
  LOG(INFO) << "CefMenuManagerExt::onContextMenuSelected command_id: " << command_id;
  if (contents_) {
    contents_->ExecuteCustomContextMenuCommand(command_id, link_followed_);
  }
  onContextMenuClosed();
}
 
void CefMenuManagerExt::onContextMenuClosed() {
  LOG(INFO) << "CefMenuManagerExt::onContextMenuClosed";
  if (model_) {
    model_->Clear();
    model_->set_delegate(nullptr);
  }
  menu_manager_ = nullptr;
  contents_ = nullptr;
  link_followed_ = GURL();
}
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)