// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_DISPATCHER_DELEGATE_H_
#define CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_DISPATCHER_DELEGATE_H_

#include "extensions/renderer/dispatcher_delegate.h"
#if defined(OHOS_ARKWEB_EXTENSIONS)
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#endif

namespace extensions {

class CefExtensionsDispatcherDelegate : public DispatcherDelegate {
 public:
  CefExtensionsDispatcherDelegate();

  CefExtensionsDispatcherDelegate(const CefExtensionsDispatcherDelegate&) =
      delete;
  CefExtensionsDispatcherDelegate& operator=(
      const CefExtensionsDispatcherDelegate&) = delete;

  ~CefExtensionsDispatcherDelegate() override;

  void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) override;
#if defined(OHOS_ARKWEB_EXTENSIONS)
  void InitializeBindingsSystem(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) override;

  void RegisterNativeHandlers(
      Dispatcher* dispatcher,
      ModuleSystem* module_system,
      NativeExtensionBindingsSystem* bindings_system,
      ScriptContext* context) override;
#endif
};

}  // namespace extensions

#endif  // CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_DISPATCHER_DELEGATE_H_
