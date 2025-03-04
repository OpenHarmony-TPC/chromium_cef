// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/renderer_prefs.h"

#include <string>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/command_line.h"
#include "base/i18n/character_encoding.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_OHOS)
#include "base/ohos/sys_info_utils.h"
#include "content/public/browser/render_widget_host.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#endif

#if defined(OHOS_EX_EXCEPTION_LIST)
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_switches.h"
#endif

namespace renderer_prefs {

namespace {

// Chrome preferences.
// Should match ChromeContentBrowserClient::OverrideWebkitPrefs.
void SetChromePrefs(Profile* profile, blink::web_pref::WebPreferences& web) {
  PrefService* prefs = profile->GetPrefs();

  // Fill per-script font preferences.
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitStandardFontFamilyMap,
                                     &web.standard_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitFixedFontFamilyMap,
                                     &web.fixed_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitSerifFontFamilyMap,
                                     &web.serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitSansSerifFontFamilyMap,
                                     &web.sans_serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitCursiveFontFamilyMap,
                                     &web.cursive_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitFantasyFontFamilyMap,
                                     &web.fantasy_font_family_map);

  web.default_font_size = prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web.default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web.minimum_font_size = prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web.minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web.default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web.dom_paste_enabled = prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web.tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    web.javascript_enabled = false;
  }
  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled)) {
    web.web_security_enabled = false;
  }
  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled)) {
    web.plugins_enabled = false;
  }
  web.loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs)) {
    web.webgl1_enabled = false;
    web.webgl2_enabled = false;
  }

  web.allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);

  web.password_echo_enabled = browser_defaults::kPasswordEchoEnabled;

  web.text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web.hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

  if (extensions::ExtensionsEnabled()) {
    std::string image_animation_policy =
        prefs->GetString(prefs::kAnimationPolicy);
    if (image_animation_policy == kAnimationPolicyOnce) {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce;
    } else if (image_animation_policy == kAnimationPolicyNone) {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyNoAnimation;
    } else {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
    }
  }

  // Make sure we will set the default_encoding with canonical encoding name.
  web.default_encoding =
      base::GetCanonicalEncodingNameByAliasName(web.default_encoding);
  if (web.default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web.default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web.default_encoding.empty());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePotentiallyAnnoyingSecurityFeatures)) {
    web.disable_reading_from_canvas = true;
    web.strict_mixed_content_checking = true;
    web.strict_powerful_feature_restrictions = true;
  }
}

// Extension preferences.
// Should match ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs.
void SetExtensionPrefs(content::WebContents* web_contents,
                       content::RenderViewHost* rvh,
                       blink::web_pref::WebPreferences& web) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(
          rvh->GetProcess()->GetBrowserContext());
  if (!registry) {
    return;
  }

  // Note: it's not possible for kExtensionsScheme to change during the lifetime
  // of the process.
  //
  // Ensure that we are only granting extension preferences to URLs with the
  // correct scheme. Without this check, chrome-guest:// schemes used by webview
  // tags as well as hosts that happen to match the id of an installed extension
  // would get the wrong preferences.
  const GURL& site_url =
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL();
  if (!site_url.SchemeIs(extensions::kExtensionScheme)
#if defined(OHOS_ARKWEB_EXTENSIONS)
      && !site_url.SchemeIs(extensions::kArkwebExtensionScheme)
#endif
  ) {
    return;
  }

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(site_url.host());
  extension_webkit_preferences::SetPreferences(extension, &web);
}

void SetString(CommandLinePrefStore* prefs,
               const std::string& key,
               const std::string& value) {
  prefs->SetValue(key, base::Value(value),
                  WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void SetBool(CommandLinePrefStore* prefs, const std::string& key, bool value) {
  prefs->SetValue(key, base::Value(value),
                  WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

#ifndef OHOS_DARKMODE
blink::mojom::PreferredColorScheme ToBlinkPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme native_theme_scheme) {
  switch (native_theme_scheme) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      return blink::mojom::PreferredColorScheme::kDark;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      return blink::mojom::PreferredColorScheme::kLight;
  }

  DCHECK(false);
}
#endif

// From chrome/browser/chrome_content_browser_client.cc
// Returns true if preferred color scheme is modified based on at least one of
// the following -
// |url| - Last committed url.
// |native_theme| - For other platforms based on native theme scheme.
bool UpdatePreferredColorScheme(blink::web_pref::WebPreferences* web_prefs,
                                const GURL& url,
                                const ui::NativeTheme* native_theme) {
  auto old_preferred_color_scheme = web_prefs->preferred_color_scheme;

  // Update based on native theme scheme.
#ifndef OHOS_DARKMODE
  web_prefs->preferred_color_scheme =
      ToBlinkPreferredColorScheme(native_theme->GetPreferredColorScheme());
#endif

  // Force a light preferred color scheme on certain URLs if kWebUIDarkMode is
  // disabled; some of the UI is not yet correctly themed.
  if (!base::FeatureList::IsEnabled(features::kWebUIDarkMode)) {
    // Update based on last committed url.
    bool force_light = url.SchemeIs(content::kChromeUIScheme);
    if (!force_light) {
      force_light = (url.SchemeIs(extensions::kExtensionScheme)
#if defined(OHOS_ARKWEB_EXTENSIONS)
                     || url.SchemeIs(extensions::kArkwebExtensionScheme)
#endif
                         ) &&
                    url.host_piece() == extension_misc::kPdfExtensionId;
    }
    if (force_light) {
      web_prefs->preferred_color_scheme =
          blink::mojom::PreferredColorScheme::kLight;
    }
  }

  return old_preferred_color_scheme != web_prefs->preferred_color_scheme;
}

#if BUILDFLAG(IS_OHOS)
void SetCefSpecialPrefs(content::RenderViewHost* rvh,
                        CefRefPtr<AlloyBrowserHostImpl> browser,
                        blink::web_pref::WebPreferences& web) {
  if (browser->settings().text_size_percent > 0) {
    web.force_enable_zoom = false;
    web.text_zoom_factor = browser->settings().text_size_percent / 100.0f;
    auto frame = browser->GetMainFrame();
    if (frame && frame->IsValid()) {
      static_cast<CefFrameHostImpl*>(frame.get())
          ->PutZoomingForTextFactor(web.text_zoom_factor);
    }
  }
  if (!rvh) {
    return;
  }
  content::RenderWidgetHostViewBase* rwhvb =
      static_cast<content::RenderWidgetHostViewBase*>(
          rvh->GetWidget()->GetView());
  if (rwhvb && rwhvb->IsRenderWidgetHostViewChildFrame()) {
    return;
  }

  if (rwhvb) {
    rwhvb->SetDoubleTapSupportEnabled(
        browser->settings().supports_double_tap_zoom);
    rwhvb->SetMultiTouchZoomSupportEnabled(
        browser->settings().supports_multi_touch_zoom);
  }
}
#endif

}  // namespace

void SetCommandLinePrefDefaults(CommandLinePrefStore* prefs) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kDefaultEncoding)) {
    SetString(prefs, prefs::kDefaultCharset,
              command_line->GetSwitchValueASCII(switches::kDefaultEncoding));
  }

  if (command_line->HasSwitch(switches::kDisableJavascriptDomPaste)) {
    SetBool(prefs, prefs::kWebKitDomPasteEnabled, false);
  }
  if (command_line->HasSwitch(switches::kDisableImageLoading)) {
    SetBool(prefs, prefs::kWebKitLoadsImagesAutomatically, false);
  }
  if (command_line->HasSwitch(switches::kDisableTabToLinks)) {
    SetBool(prefs, prefs::kWebkitTabsToLinks, false);
  }
}

void SetDefaultPrefs(blink::web_pref::WebPreferences& web) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  web.javascript_enabled =
      !command_line->HasSwitch(switches::kDisableJavascript);
  web.allow_scripts_to_close_windows =
      !command_line->HasSwitch(switches::kDisableJavascriptCloseWindows);
  web.javascript_can_access_clipboard =
      !command_line->HasSwitch(switches::kDisableJavascriptAccessClipboard);
  web.allow_universal_access_from_file_urls =
      command_line->HasSwitch(switches::kAllowUniversalAccessFromFileUrls);
  web.shrinks_standalone_images_to_fit =
      command_line->HasSwitch(switches::kImageShrinkStandaloneToFit);
  web.text_areas_are_resizable =
      !command_line->HasSwitch(switches::kDisableTextAreaResize);

#if BUILDFLAG(IS_OHOS)
  // These OHOS default prefs defined in web_preferences.cc is only used for
  // none PC device, PC device has different prefs as below.
  if (base::ohos::IsPcDevice()) {
    web.viewport_meta_enabled = false;
    web.auto_zoom_focused_editable_to_legible_scale = false;
    web.shrinks_viewport_contents_to_fit = false;
    web.viewport_style = blink::mojom::ViewportStyle::kDefault;
    web.always_show_context_menu_on_touch = true;
    web.smooth_scroll_for_find_enabled = false;
    web.main_frame_resizes_are_orientation_changes = false;

    web.double_tap_to_zoom_enabled = false;

    web.text_autosizing_enabled = false;

    web.default_minimum_page_scale_factor = 1.f;
    web.default_maximum_page_scale_factor = 4.f;
  }
#endif
}

// Helper macro for setting a WebPreferences variable based on the value of a
// CefBrowserSettings variable.
#define SET_STATE(cef_var, web_var)   \
  if (cef_var == STATE_ENABLED)       \
    web_var = true;                   \
  else if (cef_var == STATE_DISABLED) \
    web_var = false;

void SetCopyOptionToWeb(const CefBrowserSettings& cef,
                 blink::web_pref::WebPreferences& web) {
  switch (cef.copy_option) {
    case 0:
      web.copy_option = blink::mojom::CopyOptionMode::NONE;
      break;
    case 1:
      web.copy_option = blink::mojom::CopyOptionMode::IN_APP;
      break;
    case 2:
      web.copy_option = blink::mojom::CopyOptionMode::LOCAL_DEVICE;
      break;
    case 3:
      web.copy_option = blink::mojom::CopyOptionMode::CROSS_DEVICE;
      break;
    default:
      web.copy_option = blink::mojom::CopyOptionMode::CROSS_DEVICE;
  }
}

void SetCefPrefs(const CefBrowserSettings& cef,
                 blink::web_pref::WebPreferences& web) {
  if (cef.standard_font_family.length > 0) {
    web.standard_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.standard_font_family);
  }
  if (cef.fixed_font_family.length > 0) {
    web.fixed_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.fixed_font_family);
  }
  if (cef.serif_font_family.length > 0) {
    web.serif_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.serif_font_family);
  }
  if (cef.sans_serif_font_family.length > 0) {
    web.sans_serif_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.sans_serif_font_family);
  }
  if (cef.cursive_font_family.length > 0) {
    web.cursive_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.cursive_font_family);
  }
  if (cef.fantasy_font_family.length > 0) {
    web.fantasy_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.fantasy_font_family);
  }

#if BUILDFLAG(IS_OHOS)
  web.pinch_smooth_mode = cef.pinch_smooth_mode;
#endif

  if (cef.default_font_size > 0) {
    web.default_font_size = cef.default_font_size;
  }
  if (cef.default_fixed_font_size > 0) {
    web.default_fixed_font_size = cef.default_fixed_font_size;
  }
  if (cef.minimum_font_size > 0) {
    web.minimum_font_size = cef.minimum_font_size;
  }
  if (cef.minimum_logical_font_size > 0) {
    web.minimum_logical_font_size = cef.minimum_logical_font_size;
  }

  if (cef.default_encoding.length > 0) {
    web.default_encoding = CefString(&cef.default_encoding);
  }

  SET_STATE(cef.remote_fonts, web.remote_fonts_enabled);
  SET_STATE(cef.javascript, web.javascript_enabled);
  SET_STATE(cef.javascript_close_windows, web.allow_scripts_to_close_windows);
  SET_STATE(cef.javascript_access_clipboard,
            web.javascript_can_access_clipboard);
  SET_STATE(cef.javascript_dom_paste, web.dom_paste_enabled);
#if !BUILDFLAG(IS_OHOS)
  SET_STATE(cef.image_loading, web.loads_images_automatically);
#endif

  SET_STATE(cef.image_shrink_standalone_to_fit,
            web.shrinks_standalone_images_to_fit);
  SET_STATE(cef.text_area_resize, web.text_areas_are_resizable);
  SET_STATE(cef.tab_to_links, web.tabs_to_links);
  SET_STATE(cef.local_storage, web.local_storage_enabled);
  SET_STATE(cef.databases, web.databases_enabled);

  // Never explicitly enable GPU-related functions in this method because the
  // GPU blacklist is not being checked here.
  if (cef.webgl == STATE_DISABLED) {
    web.webgl1_enabled = false;
    web.webgl2_enabled = false;
  }

#if BUILDFLAG(IS_OHOS)
  /* ohos webview begin*/
  SET_STATE(cef.image_loading, web.images_enabled);
  SET_STATE(cef.force_dark_mode_enabled, web.force_dark_mode_enabled);
  SET_STATE(cef.native_embed_mode_enabled, web.native_embed_mode_enabled);
  web.embed_tag = CefString(&cef.embed_tag);
  web.embed_tag_type = CefString(&cef.embed_tag_type);
  web.draw_mode = cef.draw_mode;
  SET_STATE(cef.text_autosizing_enabled, web.text_autosizing_enabled);
  web.force_zero_layout_height = cef.force_zero_layout_height;
#if defined(OHOS_DARKMODE)
  if (cef.dark_prefer_color_scheme_enabled == STATE_ENABLED) {
    web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }
#endif
  SET_STATE(cef.loads_images_automatically, web.loads_images_automatically);
  SET_STATE(cef.allow_running_insecure_content,
            web.allow_running_insecure_content);
  SET_STATE(cef.strict_mixed_content_checking,
            web.strict_mixed_content_checking);
  SET_STATE(cef.allow_mixed_content_upgrades, web.allow_mixed_content_upgrades);
  SET_STATE(cef.initialize_at_minimum_page_scale,
            web.initialize_at_minimum_page_scale);
#if defined(OHOS_INPUT_EVENTS)
  SET_STATE(cef.hide_vertical_scrollbars, web.hide_vertical_scrollbars);
  SET_STATE(cef.hide_horizontal_scrollbars, web.hide_horizontal_scrollbars);
  web.scroll_enabled = cef.scroll_enabled;
#endif  // defined(OHOS_INPUT_EVENTS)
#ifdef OHOS_SCROLLBAR
  web.scrollbar_color = cef.scrollbar_color;
#endif // OHOS_SCROLLBAR
#ifdef OHOS_EX_FREE_COPY
  web.contextmenu_customization_enabled = cef.contextmenu_customization_enabled;
#endif
  if (cef.viewport_meta_enabled.has_value())
    web.viewport_meta_enabled = cef.viewport_meta_enabled.value();
  web.autoplay_policy =
      cef.user_gesture_required
          ? blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired
          : blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  /* ohos webview end */
#endif
#if defined(OHOS_CLIPBOARD)
  SetCopyOptionToWeb(cef, web);
#endif // defined(OHOS_CLIPBOARD)
#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
  web.custom_video_player_enable = cef.custom_video_player_enable;
  web.custom_video_player_overlay = cef.custom_video_player_overlay;
#endif // OHOS_CUSTOM_VIDEO_PLAYER
#if defined(OHOS_MULTI_WINDOW)
  web.supports_multiple_windows = cef.supports_multiple_windows;
#endif // OHOS_MULTI_WINDOW

#if defined(OHOS_SOFTWARE_COMPOSITOR)
  web.record_whole_document = cef.record_whole_document;
#endif

#ifdef OHOS_NETWORK_LOAD
  SET_STATE(cef.universal_access_from_file_urls, web.allow_universal_access_from_file_urls);
#endif

#ifdef OHOS_MEDIA_NETWORK_TRAFFIC_PROMPT
  web.enable_media_network_traffic_prompt =
      cef.enable_media_network_traffic_prompt;
#endif // OHOS_MEDIA_NETWORK_TRAFFIC_PROMPT
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  RegisterAnimationPolicyPrefs(registry);

  // From chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserUserPrefs.
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kCaretBrowsingEnabled, false);

  registry->RegisterStringPref(prefs::kWebRTCIPHandlingPolicy,
                               blink::kWebRTCIPHandlingDefault);
  registry->RegisterStringPref(prefs::kWebRTCUDPPortRange, std::string());

#if !BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(prefs::kFullscreenAllowed, true);
#endif

  // From ChromeContentBrowserClient::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
  registry->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing, true);

  // From Profile::RegisterProfilePrefs.
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
}

void PopulateWebPreferences(content::RenderViewHost* rvh,
                            blink::web_pref::WebPreferences& web,
                            SkColor& base_background_color) {
  REQUIRE_ALLOY_RUNTIME();
  CefRefPtr<AlloyBrowserHostImpl> browser = static_cast<AlloyBrowserHostImpl*>(
      extensions::GetOwnerBrowserForHost(rvh, nullptr).get());

  // Set defaults for preferences that are not handled by PrefService.
  SetDefaultPrefs(web);

  // Set preferences based on the context's PrefService.
  if (browser) {
    auto profile = Profile::FromBrowserContext(
        browser->web_contents()->GetBrowserContext());
    SetChromePrefs(profile, web);
  }

  auto* native_theme = ui::NativeTheme::GetInstanceForWeb();
#ifndef OHOS_DARKMODE
  switch (native_theme->GetPreferredColorScheme()) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
      break;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
      break;
  }
#endif

  switch (native_theme->GetPreferredContrast()) {
    case ui::NativeTheme::PreferredContrast::kNoPreference:
      web.preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
      break;
    case ui::NativeTheme::PreferredContrast::kMore:
      web.preferred_contrast = blink::mojom::PreferredContrast::kMore;
      break;
    case ui::NativeTheme::PreferredContrast::kLess:
      web.preferred_contrast = blink::mojom::PreferredContrast::kLess;
      break;
    case ui::NativeTheme::PreferredContrast::kCustom:
      web.preferred_contrast = blink::mojom::PreferredContrast::kCustom;
      break;
  }

  auto web_contents = content::WebContents::FromRenderViewHost(rvh);
  UpdatePreferredColorScheme(
      &web,
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
      native_theme);

  // Set preferences based on the extension.
  SetExtensionPrefs(web_contents, rvh, web);

  if (browser) {
    // Set preferences based on CefBrowserSettings.
    SetCefPrefs(browser->settings(), web);

#if BUILDFLAG(IS_OHOS)
    SetCefSpecialPrefs(rvh, browser, web);
#endif

#if defined(OHOS_EX_EXCEPTION_LIST)
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kForBrowser)) {
      bool javascript_enabled = web.javascript_enabled;

      // Update content setting default value
      ContentSetting setting =
          javascript_enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      if (!web_contents)
        return;
      HostContentSettingsMap* host_content_settings_map =
          HostContentSettingsMapFactory::GetForProfile(
              web_contents->GetBrowserContext());
      host_content_settings_map->SetDefaultContentSetting(
          ContentSettingsType::JAVASCRIPT, setting);
      DVLOG(0)
          << "ExceptionList Update default content setting for JavaScript: "
          << int(setting);
    }
#endif  // defined(OHOS_EX_EXCEPTION_LIST)

    web.picture_in_picture_enabled = browser->IsPictureInPictureSupported();

    // Set the background color for the WebView.
    base_background_color = browser->GetBackgroundColor();
  } else {
    // We don't know for sure that the browser will be windowless but assume
    // that the global windowless state is likely to be accurate.
    base_background_color =
        CefContext::Get()->GetBackgroundColor(nullptr, STATE_DEFAULT);
  }
}

bool PopulateWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences& web) {
  auto* native_theme = ui::NativeTheme::GetInstanceForWeb();
  return UpdatePreferredColorScheme(&web, web_contents->GetLastCommittedURL(),
                                    native_theme);
}

}  // namespace renderer_prefs
