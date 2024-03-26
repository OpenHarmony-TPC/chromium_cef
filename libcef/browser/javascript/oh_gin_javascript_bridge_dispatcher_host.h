// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H
#define OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H

#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_contents_observer.h"
#include "include/cef_client.h"
#include "libcef/browser/browser_info.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_errors.h"

namespace content {
class WebContentsImpl;
}

namespace NWEB {
class OhGinJavascriptBridgeDispatcherHost
    : public base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>,
      public content::WebContentsObserver {
 public:
  // 为了兼容老版本webcontroller, 要保持跟ace和core侧定义一致
  enum class JavaScriptObjIdErrorCode : int32_t {
    WEBCONTROLLERERROR = -2,
    WEBVIEWCONTROLLERERROR = -1,
    END = 0
  };
  typedef int32_t ObjectID;
  static const int32_t MIN_NATIVE_OBJ_ID = INT32_MAX >> 1;

  OhGinJavascriptBridgeDispatcherHost(content::WebContents* web_contents,
                                      CefRefPtr<CefClient> client);
  OhGinJavascriptBridgeDispatcherHost(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;
  OhGinJavascriptBridgeDispatcherHost& operator=(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;

  void AddNamedObject(const std::string& classname,
                      const std::vector<std::string>& method_list,
                      const ObjectID object_id);
  void RemoveNamedObject(const std::string& classname,
                         const std::vector<std::string>& method_list);

  // WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
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
                      const std::string& document_url,
                      const std::string& method_name,
                      const base::Value::List& arguments,
                      base::Value::List* result,
                      OhGinJavascriptBridgeError* error_code);
  void OnInvokeMethodFlowbuf(int routing_id,
                      int32_t object_id,
                      const std::string& document_url,
                      const std::string& method_name,
                      const base::Value::List& arguments,
                      int fd,
                      base::Value::List* result,
                      OhGinJavascriptBridgeError* error_code);
  void OnObjectWrapperDeleted(int routing_id, ObjectID object_id);
  void ClearMethodMap() {
    method_map_.clear();
  }

  void DoCallH5Function(int32_t routing_id,
                        int32_t h5_object_id,
                        const std::string& h5_method_name,
                        const std::vector<CefRefPtr<CefValue>>& args);

 private:
  friend class base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>;

  // object id and ace js function name
  typedef std::map<ObjectID, std::string> ObjectMap;

  ~OhGinJavascriptBridgeDispatcherHost();

  // Run on the UI thread.
  void InstallFilterAndRegisterAllRoutingIds();
  void AddNamedObjectForWebController(
      const std::string& classname,
      const std::vector<std::string>& method_list);

  void AddNamedObjectForWebViewController(
      const std::string& classname,
      const std::vector<std::string>& method_list,
      const ObjectID object_id);
  content::WebContentsImpl* web_contents() const;

  // js property name and object id
  using MethodPair = std::pair<std::string, std::unordered_set<std::string>>;
  using ObjectMethodMap = std::map<ObjectID, MethodPair>;
  ObjectMethodMap method_map_;
  std::shared_mutex share_mutex_;
  int32_t object_id_ = MIN_NATIVE_OBJ_ID;

  CefRefPtr<CefClient> client_;
};
}  // namespace NWEB
#endif
