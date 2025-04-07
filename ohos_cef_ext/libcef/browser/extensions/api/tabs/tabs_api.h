// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include "chrome/common/extensions/api/tabs.h"
#include "extensions/browser/extension_function.h"

#include "ohos_nweb/src/capi/nweb_extension_api_struct_info.h"
#include "ohos_nweb/src/capi/web_extension_tab_items.h"
#include "ohos_cef_ext/libcef/browser/extensions/extension_function_details.h"
// The contents of this file are extracted from
// chrome/browser/extensions/api/tabs/tabs_api.h.

namespace content {
class WebContents;
}

namespace extensions {
namespace cef {

class TabsGetFunction : public ExtensionFunction {
  ~TabsGetFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};

class TabsCreateFunction : public ExtensionFunction {
 public:
  TabsCreateFunction();
  ~TabsCreateFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)

 private:

  static void OnTabCreated(const base::WeakPtr<TabsCreateFunction>& function,
                           const NWebExtensionTab* tab);

  void GetCreateParams(std::optional<api::tabs::Create::Params>&,
                       NWebTabCreateInfo& create_info);

  bool call_create_tab_ = false;

  base::WeakPtrFactory<TabsCreateFunction> weak_ptr_factory_{this};
};

class BaseAPIFunction : public ExtensionFunction {
 public:
  BaseAPIFunction();

 protected:
  ~BaseAPIFunction() override {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set |error_|.
  content::WebContents* GetWebContents(int tab_id);

  std::string error_;
  const CefExtensionFunctionDetails cef_details_;
};

class TabsUpdateFunction : public BaseAPIFunction {
 private:
  ~TabsUpdateFunction() override {}

  ResponseAction Run() override;

  bool UpdateURL(const std::string& url, int tab_id, std::string* error);
  ResponseValue GetResult();

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)

  int tab_id_ = -1;
  content::WebContents* web_contents_ = nullptr;
};

// Based on ChromeAsyncExtensionFunction.
class ZoomAPIFunction : public ExtensionFunction {
 public:
  ZoomAPIFunction();

 protected:
  ~ZoomAPIFunction() override {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set |error_|.
  content::WebContents* GetWebContents(int tab_id);

  std::string error_;

 private:
  const CefExtensionFunctionDetails cef_details_;
};

class TabsSetZoomFunction : public BaseAPIFunction {
 private:
  ~TabsSetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public BaseAPIFunction {
 private:
  ~TabsGetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public BaseAPIFunction {
 private:
  ~TabsSetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public BaseAPIFunction {
 private:
  ~TabsGetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

class TabsCaptureVisibleTabFunction : public ExtensionFunction {
 public:
  ~TabsCaptureVisibleTabFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)
 private:
  std::optional<int> window_id_;
};

class TabsGetCurrentFunction : public BaseAPIFunction {
 public:
  ~TabsGetCurrentFunction() override {}
 
  ResponseAction Run() override;
 
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};

class TabsDiscardFunction : public ExtensionFunction {
 public:
  ~TabsDiscardFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.discard", TABS_DISCARD)
 private:
  std::optional<int> tab_id_;
};

class TabsDuplicateFunction : public ExtensionFunction {
 public:
  ~TabsDuplicateFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
private:
  int tab_id_;
};

class TabsGoBackFunction : public ExtensionFunction {
 public:
  ~TabsGoBackFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.goBack", TABS_GOBACK)
private:
 std::optional<int> tab_id;
};

class TabsGoForwardFunction : public ExtensionFunction {
public:
  ~TabsGoForwardFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.goForward", TABS_GOFORWARD)
  private:
  std::optional<int> tab_id;
};

class TabsGroupFunction : public ExtensionFunction {
public:
  ~TabsGroupFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.group", TABS_GROUP)
};

class TabsHighlightFunction : public ExtensionFunction {
  ~TabsHighlightFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};

class TabsMoveFunction : public ExtensionFunction {
  ~TabsMoveFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};

class TabsQueryFunction : public ExtensionFunction {
public:
  ~TabsQueryFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};

class TabsRemoveFunction : public ExtensionFunction {
  ~TabsRemoveFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};

class TabsUngroupFunction : public ExtensionFunction {
  ~TabsUngroupFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.ungroup", TABS_UNGROUP)
};

class TabsReloadFunction : public BaseAPIFunction {
private:
  ~TabsReloadFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};

}  // namespace cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
