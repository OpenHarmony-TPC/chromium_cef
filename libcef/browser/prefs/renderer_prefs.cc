// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/prefs/renderer_prefs.h"

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "cef/libcef/common/cef_switches.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_EXCEPTION_LIST)
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_switches.h"
#endif

#if BUILDFLAG(IS_ARKWEB)
#include "base/ohos/sys_info_utils_ext.h"
#include "content/public/browser/render_widget_host.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#endif

namespace renderer_prefs {

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

#if BUILDFLAG(IS_ARKWEB)
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

#if BUILDFLAG(ARKWEB_COPY_OPTION)
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
#endif  // BUILDFLAG(ARKWEB_COPY_OPTION)

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
#if BUILDFLAG(ARKWEB_PINCH_SMOOTH)
  web.pinch_smooth_mode = cef.pinch_smooth_mode;
#endif

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
  web.supports_multiple_windows = cef.supports_multiple_windows;
#endif
#if BUILDFLAG(ARKWEB_ACTIVE_POLICY)
  web.delay_for_background_tab_freezing = cef.delay_for_background_tab_freezing;
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
#if !BUILDFLAG(IS_ARKWEB)
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

#if BUILDFLAG(IS_ARKWEB)
  /* ohos webview begin*/
  SET_STATE(cef.image_loading, web.images_enabled);
  SET_STATE(cef.force_dark_mode_enabled, web.force_dark_mode_enabled);
#if BUILDFLAG(ARKWEB_DARKMODE)
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
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  SET_STATE(cef.hide_vertical_scrollbars, web.hide_vertical_scrollbars);
  SET_STATE(cef.hide_horizontal_scrollbars, web.hide_horizontal_scrollbars);
  web.scroll_enabled = cef.scroll_enabled;
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
#if BUILDFLAG(ARKWEB_SCROLLBAR)
  web.scrollbar_color = cef.scrollbar_color;
#endif  // ARKWEB_SCROLLBAR
  if (cef.viewport_meta_enabled.has_value()) {
    web.viewport_meta_enabled = cef.viewport_meta_enabled.value();
  }
  SET_STATE(cef.text_autosizing_enabled, web.text_autosizing_enabled);
  /* ohos webview end */
#endif  // BUILDFLAG(IS_ARKWEB)
#if BUILDFLAG(ARKWEB_SAME_LAYER)
  SET_STATE(cef.native_embed_mode_enabled, web.native_embed_mode_enabled);
  web.embed_tag = CefString(&cef.embed_tag);
  web.embed_tag_type = CefString(&cef.embed_tag_type);
#endif
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  web.draw_mode = cef.draw_mode;
#endif
#if BUILDFLAG(ARKWEB_FIT_CONTENT)
  web.force_zero_layout_height = cef.force_zero_layout_height;
#endif
#if BUILDFLAG(ARKWEB_CSS_FONT)
  web.font_weight_scale = cef.font_weight_scale;
#endif
#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
  web.record_whole_document = cef.record_whole_document;
#endif
#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
  SET_STATE(cef.universal_access_from_file_urls,
            web.allow_universal_access_from_file_urls);
#endif  // BUILDFLAG(ARKWEB_NETWORK_LOAD)
#if BUILDFLAG(ARKWEB_MEDIA_CAPABILITIES_ENHANCE)
  web.usage_scenario = cef.usage_scenario;
#endif
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  web.contextmenu_customization_enabled = cef.contextmenu_customization_enabled;
#endif
#if BUILDFLAG(ARKWEB_MEDIA)
  if (!base::ohos::IsPcDevice() && cef.viewport_meta_enabled.has_value()) {
    web.viewport_meta_enabled = cef.viewport_meta_enabled.value();
  }
  web.autoplay_policy =
      cef.user_gesture_required
          ? blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired
          : blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
#endif
#if BUILDFLAG(ARKWEB_COPY_OPTION)
  SetCopyOptionToWeb(cef, web);
#endif  // BUILDFLAG(ARKWEB_COPY_OPTION)
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
  web.custom_video_player_enable = cef.custom_video_player_enable;
  web.custom_video_player_overlay = cef.custom_video_player_overlay;
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER
}

}  // namespace renderer_prefs
