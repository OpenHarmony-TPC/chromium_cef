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

#ifndef CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_REQUEST_OPEN_DEVTOOLS_HANDLER_H_
#define CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_REQUEST_OPEN_DEVTOOLS_HANDLER_H_

#include "base/memory/singleton.h"
#include "arkweb/ohos_nweb/src/capi/nweb_devtools_message_handler.h"

class RequestOpenDevToolsHandler {
 public:
  static RequestOpenDevToolsHandler* GetInstance();
  void OnRequestOpenDevTools(const RequestOpenDevToolsParams& params);

  virtual ~RequestOpenDevToolsHandler();

 private:
  friend struct base::DefaultSingletonTraits<RequestOpenDevToolsHandler>;
  RequestOpenDevToolsHandler() = default;
  RequestOpenDevToolsHandler(const RequestOpenDevToolsHandler&) = delete;
  RequestOpenDevToolsHandler& operator=(const RequestOpenDevToolsHandler&) = delete;
};
#endif  // CEF_OHOS_CEF_EXT_LIBCEF_BROWSER_DEVTOOLS_ARKWEB_REQUEST_OPEN_DEVTOOLS_HANDLER_H_
