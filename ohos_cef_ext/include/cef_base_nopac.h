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

#include "arkweb/build/features/features.h"

#ifndef CEF_INCLUDE_CEF_BASE_NOPAC_H_
#define CEF_INCLUDE_CEF_BASE_NOPAC_H_
#pragma once

#if BUILDFLAG(ARKWEB_ENABLE_PAC)
class __attribute__((nopac)) CefBaseRefCountedNopac {
#else
class CefBaseRefCountedNopac {
#endif
 public:
  ///
  /// Called to increment the reference count for the object. Should be called
  /// for every new copy of a pointer to a given object.
  ///
  virtual void AddRef() const = 0;

  ///
  /// Called to decrement the reference count for the object. Returns true if
  /// the reference count is 0, in which case the object should self-delete.
  ///
  virtual bool Release() const = 0;

  ///
  /// Returns true if the reference count is 1.
  ///
  virtual bool HasOneRef() const = 0;

  ///
  /// Returns true if the reference count is at least 1.
  ///
  virtual bool HasAtLeastOneRef() const = 0;

 protected:
  virtual ~CefBaseRefCountedNopac() {}
};

#endif

