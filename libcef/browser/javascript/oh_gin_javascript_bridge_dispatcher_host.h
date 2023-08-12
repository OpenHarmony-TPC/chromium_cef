// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H
#define OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H

#include <mutex>
#include <unordered_set>
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_contents_observer.h"
#include "include/cef_client.h"
#include "libcef/browser/browser_info.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_errors.h"

namespace NWEB {
class OhGinJavascriptBridgeDispatcherHost
    : public base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>,
      public content::WebContentsObserver {
 public:
  OhGinJavascriptBridgeDispatcherHost(content::WebContents* web_contents,
                                      CefRefPtr<CefClient> client);
  OhGinJavascriptBridgeDispatcherHost(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;
  OhGinJavascriptBridgeDispatcherHost& operator=(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;

  void AddNamedObject(const std::string& classname,
                      const std::vector<std::string>& method_list);
  void RemoveNamedObject(const std::string& classname,
                         const std::vector<std::string>& method_list);

  // WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void DocumentAvailableInMainFrame(
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  // Run on the background thread.
  void OnGetMethods(int32_t object_id,
                    std::set<std::string>* returned_method_names);
  void OnHasMethod(int32_t object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(int routing_id,
                      int32_t object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      base::ListValue* result,
                      OhGinJavascriptBridgeError* error_code);

 private:
  typedef int32_t ObjectID;
  friend class base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>;

  // object id and ace js function name
  typedef std::map<ObjectID, std::string> ObjectMap;

  ~OhGinJavascriptBridgeDispatcherHost();

  // Run on the UI thread.
  void InstallFilterAndRegisterAllRoutingIds();

  // js property name and object id
  using MethodPair = std::pair<std::string, std::unordered_set<std::string>>;
  using ObjectMethodMap = std::map<ObjectID, MethodPair>;
  ObjectMethodMap method_map_;
  int32_t object_id_ = 0;

  std::mutex object_mtx_;
  CefRefPtr<CefClient> client_;
};
}  // namespace NWEB
#endif