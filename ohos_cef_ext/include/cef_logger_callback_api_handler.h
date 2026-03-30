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

#ifndef CEF_LOGGER_CALLBACK_API_HANDLER_H_
#define CEF_LOGGER_CALLBACK_API_HANDLER_H_
#pragma once

#include "include/cef_base.h"

///
/// Implement this interface to handle loggercallback's api events. The methods of
///  this class will be called on the UI thread.
///
/*--cef(source=client)--*/
class CefLoggerCallbackApiHandler : public virtual CefBaseRefCounted {
 public:
  virtual void logFeedback(const CefString& tag,
                           int level,
                           const CefString& message) {}

  virtual void logUrl(const CefString& url) {}
};

#endif
