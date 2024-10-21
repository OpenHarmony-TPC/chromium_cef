// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/alloy_content_settings_client.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if defined(OHOS_EX_EXCEPTION_LIST)
#include "base/command_line.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "url/origin.h"

namespace {
GURL GetOriginOrURL(const blink::WebFrame* frame) {
  url::Origin top_origin = url::Origin(frame->Top()->GetSecurityOrigin());
  // The |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  // TODO(alexmos): This is broken for --site-per-process, since top() can be a
  // WebRemoteFrame which does not have a document(), and the WebRemoteFrame's
  // URL is not replicated.  See https://crbug.com/628759.
  if (top_origin.opaque() && frame->Top()->IsWebLocalFrame())
    return frame->Top()->ToWebLocalFrame()->GetDocument().Url();
  return top_origin.GetURL();
}

// Allow passing both WebURL and GURL here, so that we can early return
// without allocating a new backing string if only the default rule matches.
template <typename URL>
ContentSetting GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const blink::WebFrame* frame,
    const URL& secondary_url) {
  // If there is only one rule, it's the default rule and we don't need to match
  // the patterns.
  if (rules.size() == 1) {
    DCHECK(rules[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(rules[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return rules[0].GetContentSetting();
  }
  const GURL& primary_url = GetOriginOrURL(frame);
  const GURL& secondary_gurl = secondary_url;
  for (const auto& rule : rules) {
    if (rule.primary_pattern.Matches(primary_url) &&
        rule.secondary_pattern.Matches(secondary_gurl)) {
      return rule.GetContentSetting();
    }
  }
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}
}  // namespace
#endif

AlloyContentSettingsClient::AlloyContentSettingsClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
}

AlloyContentSettingsClient::~AlloyContentSettingsClient() {}

bool AlloyContentSettingsClient::ShouldAutoupgradeMixedContent() {
  return render_frame()->GetBlinkPreferences().allow_mixed_content_upgrades;
}

void AlloyContentSettingsClient::OnDestruct() {
  delete this;
}

#if defined(OHOS_EX_EXCEPTION_LIST)
bool AlloyContentSettingsClient::ShouldAllowlistForContentSettings() const {
  return render_frame()->GetWebFrame()->GetDocument().Url().GetString() ==
         content::kUnreachableWebDataURL;
}

bool AlloyContentSettingsClient::AllowScript(bool enabled_per_settings) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForBrowser)) {
    return enabled_per_settings;
  }

  if (render_frame() && render_frame()->GetWebFrame()) {
    blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
    const auto it = cached_script_permissions_.find(frame);
    if (it != cached_script_permissions_.end())
      return it->second;

    // Evaluate the content setting rules before
    // IsWhitelistedForContentSettings(); if there is only the default rule
    // allowing all scripts, it's quicker this way.
    bool allow = true;
    if (content_setting_rules_) {
      ContentSetting setting = GetContentSettingFromRules(
          content_setting_rules_->script_rules, frame,
          url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL());
      allow = setting != CONTENT_SETTING_BLOCK;
    }
    allow = allow || IsWhitelistedForContentSettings();

    cached_script_permissions_[frame] = allow;
    return allow;
  }
  if (ShouldAllowlistForContentSettings()) {
    return true;
  }
  return blink::WebContentSettingsClient::AllowScript(enabled_per_settings);
}

void AlloyContentSettingsClient::SetContentSettingRules(
    const RendererContentSettingRules* content_setting_rules) {
  content_setting_rules_ = content_setting_rules;
}

const RendererContentSettingRules*
AlloyContentSettingsClient::GetContentSettingRules() {
  return content_setting_rules_;
}

void AlloyContentSettingsClient::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (!render_frame()) {
    return;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame || frame->Parent())
    return;  // Not a top-level navigation.

  cached_script_permissions_.clear();
}

// static
bool AlloyContentSettingsClient::IsWhitelistedForContentSettings() const {
  if (!render_frame() || !render_frame()->GetWebFrame()) {
    return false;
  }
  const blink::WebDocument& document =
      render_frame()->GetWebFrame()->GetDocument();
  return IsWhitelistedForContentSettings(document.GetSecurityOrigin(),
                                         document.Url());
}

bool AlloyContentSettingsClient::IsWhitelistedForContentSettings(
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& document_url) {
  if (document_url.GetString() == content::kUnreachableWebDataURL)
    return true;

  if (origin.IsOpaque())
    return false;  // Uninitialized document?

  blink::WebString protocol = origin.Protocol();

  if (protocol == content::kChromeUIScheme)
    return true;  // Browser UI elements should still work.

  if (protocol == content::kChromeDevToolsScheme)
    return true;  // DevTools UI elements should still work.

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (protocol == url::kFileScheme &&
      document_url.ProtocolIs(url::kFileScheme)) {
    return GURL(document_url).ExtractFileName().empty();
  }
  return false;
}
#endif