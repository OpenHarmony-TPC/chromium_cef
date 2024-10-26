// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_
#define CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"

#if defined(OHOS_EX_EXCEPTION_LIST)
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#endif

// This class sends and receives control messages in the renderer process.
class AlloyRenderThreadObserver : public content::RenderThreadObserver,
                                  public chrome::mojom::RendererConfiguration {
 public:
  AlloyRenderThreadObserver();

  AlloyRenderThreadObserver(const AlloyRenderThreadObserver&) = delete;
  AlloyRenderThreadObserver& operator=(const AlloyRenderThreadObserver&) =
      delete;

  ~AlloyRenderThreadObserver() override;

  bool IsIncognitoProcess() const { return is_incognito_process_; }

  // Return a copy of the dynamic parameters - those that may change while the
  // render process is running.
  chrome::mojom::DynamicParamsPtr GetDynamicParams() const;

#if defined(OHOS_EX_EXCEPTION_LIST)
  const RendererContentSettingRules* content_setting_rules() const;
#endif

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(
      bool is_incognito_process,
      mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
          chromeos_listener_receiver,
      mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
          content_settings_manager,
      mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledListener>
          bound_session_request_throttled_listener) override;
  void SetConfiguration(chrome::mojom::DynamicParamsPtr params) override;

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
          receiver);

#if defined(OHOS_EX_EXCEPTION_LIST)
  void SetContentSettingRules(const RendererContentSettingRules& rules) override;
#endif  // defined(OHOS_EX_EXCEPTION_LIST)

  bool is_incognito_process_ = false;

  RendererContentSettingRules content_setting_rules_;

  mojo::AssociatedReceiverSet<chrome::mojom::RendererConfiguration>
      renderer_configuration_receivers_;

  chrome::mojom::DynamicParamsPtr dynamic_params_
      GUARDED_BY(dynamic_params_lock_);
  mutable base::Lock dynamic_params_lock_;
};

#endif  // CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_
