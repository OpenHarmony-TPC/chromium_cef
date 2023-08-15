// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_SETTINGS_CLIENT_H_
#define CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_SETTINGS_CLIENT_H_

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#if BUILDFLAG(IS_OHOS)
#include "base/containers/flat_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/base/page_transition_types.h"
#endif

// NWeb implementation of blink::WebContentSettingsClient.
class AlloyContentSettingsClient : public content::RenderFrameObserver,
                                   public blink::WebContentSettingsClient {
 public:
  AlloyContentSettingsClient(const AlloyContentSettingsClient&) = delete;
  AlloyContentSettingsClient& operator=(const AlloyContentSettingsClient&) =
      delete;

  explicit AlloyContentSettingsClient(content::RenderFrame* render_view);

#if BUILDFLAG(IS_OHOS)
  // Sets the content setting rules which back |allowScript()| and
  // |allowScriptFromSource()|. |content_setting_rules|
  // must outlive this |AwContentSettingsClient|.
  void SetContentSettingRules(
      const RendererContentSettingRules* content_setting_rules);
  const RendererContentSettingRules* GetContentSettingRules();
#endif

 private:
  ~AlloyContentSettingsClient() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  bool ShouldAutoupgradeMixedContent() override;
#if BUILDFLAG(IS_OHOS)
  bool ShouldAllowlistForContentSettings() const;

  // blink::WebContentSettingsClient implementation.
  bool AllowScript(bool enabled_per_settings) override;

  // content::RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  // Helpers.
  // True if |render_frame()| contains content that is white-listed for content
  // settings.
  bool IsWhitelistedForContentSettings() const;

  // Exposed for unit tests.
  static bool IsWhitelistedForContentSettings(
      const blink::WebSecurityOrigin& origin,
      const blink::WebURL& document_url);

  // A pointer to content setting rules stored by the renderer. Normally, the
  // |RendererContentSettingRules| object is owned by
  // |ChromeRenderThreadObserver|. In the tests it is owned by the caller of
  // |SetContentSettingRules|.
  const RendererContentSettingRules* content_setting_rules_ = nullptr;

  // Caches the result of AllowScript.
  base::flat_map<blink::WebFrame*, bool> cached_script_permissions_;
#endif
};

#endif  // CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_SETTINGS_CLIENT_H_
