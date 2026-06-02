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

// This file is included by browser_platform_delegate_native_linux.cc
// and must not be compiled independently.
#ifndef CEF_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_CC_
#error This file must be included from browser_platform_delegate_native_linux.cc
#endif

// When XKeySymToDomKey returns DomKey::NONE (e.g. IME VKEY_PROCESSKEY with
// character=0), replace with DomKey::UNIDENTIFIED per W3C UI Events spec.
// DomKey::NONE causes Blink DomKeyToKeyString to return empty string "",
// but the spec requires "Unidentified" for unrecognized keys.
#if BUILDFLAG(IS_OZONE_X11) || (BUILDFLAG(ARKWEB_INPUT_EVENTS) && !defined(COMPONENT_BUILD))
  if (!dom_key.IsValid() || dom_key == ui::DomKey::NONE) {
    dom_key = ui::DomKey::UNIDENTIFIED;
  }
#endif