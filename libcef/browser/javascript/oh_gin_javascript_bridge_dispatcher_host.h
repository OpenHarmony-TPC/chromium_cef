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

  void AddNamedObject(const std::string& object_name,
                      const std::vector<std::string>& method_list,
                      const std::vector<std::string>& async_method_list,
                      const ObjectID object_id);
  void AddNativeNamedObject(const std::string& object_name,
                      const std::vector<std::string>& method_list,
                      const ObjectID object_id,
                      bool is_async);
  void RemoveNamedObject(const std::string& object_name,
                         const std::vector<std::string>& method_list);
  bool RemoveNamedObjectInternal(const std::string& object_name, bool is_async);

  // WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void PrimaryPageChanged(content::Page& page) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

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
  void OnInvokeMethodAsync(int routing_id,
                           int32_t object_id,
                           const std::string& document_url,
                           const std::string& method_name,
                           const base::Value::List& arguments);
  void OnInvokeMethodFlowbuf(int routing_id,
                             int32_t object_id,
                             const std::string& document_url,
                             const std::string& method_name,
                             const base::Value::List& arguments,
                             int fd,
                             base::Value::List* result,
                             OhGinJavascriptBridgeError* error_code);
  void OnInvokeMethodFlowbufAsync(int routing_id,
                                  int32_t object_id,
                                  const std::string& document_url,
                                  const std::string& method_name,
                                  const base::Value::List& arguments,
                                  int fd);
  void OnObjectWrapperDeleted(int routing_id, ObjectID object_id);
  void ClearMethodMap() {
    sync_method_map_.clear();
    async_method_map_.clear();
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

  // When registering an Arkts object, all the registered methods will be cleared.
  void AddNamedObjectForWebController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const std::vector<std::string>& async_method_list);

  void AddNamedObjectForWebViewController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const std::vector<std::string>& async_method_list,
      const ObjectID object_id);

  // When registering a native object, only the registered methods
  // of the same type (sync/async) as the registration methods will be cleared.
  void AddNativeNamedObjectForWebController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      bool is_async);

  void AddNativeNamedObjectForWebViewController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const ObjectID object_id,
      bool is_async);
  content::WebContentsImpl* web_contents() const;

  // js property name and object id
  using MethodPair = std::pair<std::string, std::unordered_set<std::string>>;
  using ObjectMethodMap = std::map<ObjectID, MethodPair>;
  ObjectMethodMap sync_method_map_;
  ObjectMethodMap async_method_map_;
  std::shared_mutex share_mutex_;
  int32_t object_id_ = MIN_NATIVE_OBJ_ID;
  std::unordered_map<int32_t, std::unordered_set<std::string>> object_id_map_;

  CefRefPtr<CefClient> client_;
  bool install_filter_when_render_process_gone_ = false;
};
}  // namespace NWEB
#endif
