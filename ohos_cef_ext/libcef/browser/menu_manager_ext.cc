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
 
#include "cef/libcef/browser/menu_manager.h"
 
 
#if BUILDFLAG(ARKWEB_DEVTOOLS)
CefRefPtr<CefMenuModelImpl> CefMenuManager::GetContextMenuModel() {
  LOG(INFO) << "CefMenuManager::GetContextMenuModel model size: " << model_->GetCount();
  return model_;
}
 
void CefMenuManager::onContextMenuSelected(int command_id) {
  ExecuteDefaultCommand(command_id);
}
 
void CefMenuManager::onContextMenuClosed() {
  MenuClosed(model_);
}
#endif // BUILDFLAG(ARKWEB_DEVTOOLS)