// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H
#define OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHEER_HOST_H

#include <mutex>
#include <shared_mutex>
#include <unordered_set>

#include "arkweb/build/features/features.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "cef/ohos_cef_ext/include/arkweb_client_ext.h"
#include "cef/ohos_cef_ext/libcef/common/mojom/oh_gin_javascript_bridge.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "libcef/browser/browser_info.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_errors.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ohos_nweb/src/capi/nweb_extension_javascript_item.h"

namespace content {
class WebContentsImpl;
class RenderFrameHostImpl;
}  // namespace content

namespace NWEB {
struct JsProxyPermissionConfigData {
  JsProxyPermissionConfigData() = default;
  JsProxyPermissionConfigData(const JsProxyPermissionConfigData& other) =
      default;
  JsProxyPermissionConfigData& operator=(
      const JsProxyPermissionConfigData& other) = default;
  ~JsProxyPermissionConfigData() = default;

  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  std::string method_name;
};

#if BUILDFLAG(ARKWEB_SECURE_JAVASCRIPT_PROXY)
typedef struct LastCallingFrameUrlInfo {
  std::string url;
} LastCallingFrameUrlInfo;
#endif

class OhGinJavascriptBridgeDispatcherHost
    : public base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>,
      public content::WebContentsObserver,
      public content::mojom::OhGinJavascriptBridgeHost,
      public content::mojom::OhGinJavascriptBridgeRemoteObject {
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
                                      CefRefPtr<ArkWebClientExt> client);
  OhGinJavascriptBridgeDispatcherHost(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;
  OhGinJavascriptBridgeDispatcherHost& operator=(
      const OhGinJavascriptBridgeDispatcherHost&) = delete;

  void AddNamedObject(const std::string& object_name,
                      const std::vector<std::string>& method_list,
                      const std::vector<std::string>& async_method_list,
                      const ObjectID object_id,
                      const std::string& permission);
  void AddNativeNamedObject(const std::string& object_name,
                            const std::vector<std::string>& method_list,
                            const ObjectID object_id,
                            bool is_async,
                            const std::string& permission);
  void RemoveNamedObject(const std::string& object_name,
                         const std::vector<std::string>& method_list);
  void RemoveNamedObjectInternal(const std::string& object_name, bool is_async);

#if BUILDFLAG(ARKWEB_SECURE_JAVASCRIPT_PROXY)
  static const char* GetLastCallingFrameUrlTLS();
#endif

  // WebContentsObserver
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // Run on the background thread.
  void OnGetMethods(int32_t object_id,
                    std::set<std::string>* returned_method_names);
  void OnHasMethod(int32_t object_id,
                   const std::string& method_name,
                   bool* result);
  void OnHasAsyncThreadMethod(int32_t object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(int routing_id,
                      int32_t object_id,
                      const std::string& document_url,
                      const std::string& method_name,
                      const base::Value::List& arguments,
                      base::Value::List* wrapped_result,
                      content::mojom::OhGinJavascriptBridgeError* error_code);

  void OnInvokeMethodCallback(content::mojom::OhGinJavascriptBridgeError error_code,
                              base::Value::List wrapped_result,
                              InvokeMethodCallback callback);

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
                             base::Value::List* wrapped_result,
                             content::mojom::OhGinJavascriptBridgeError* error_code);
  
  void OnInvokeMethodFlowbufCallback(content::mojom::OhGinJavascriptBridgeError error_code,
                                     base::Value::List wrapped_result,
                                     InvokeMethodFlowbufCallback callback);

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
    javascript_sync_permission_map_.clear();
    javascript_async_permission_map_.clear();
    object_id_map_.clear();
  }

  void DoCallH5Function(int32_t routing_id,
                        int32_t h5_object_id,
                        const std::string& h5_method_name,
                        const std::vector<CefRefPtr<CefValue>>& args);

  // mojom::GinJavaBridgeHost overrides:
  void GetObject(int32_t object_id,
                 mojo::PendingReceiver<content::mojom::OhGinJavascriptBridgeRemoteObject>
                     receiver) override;
  void ObjectWrapperDeleted(int32_t object_id) override;

  // mojom::OhGinJavascriptBridgeRemoteObject overrides:
  void GetMethods(int32_t object_id,
                  GetMethodsCallback callback) override;
  void HasMethod(int32_t object_id,
                 const std::string& method_name,
                 HasMethodCallback callback) override;
  void InvokeMethod(const std::string& method_name,
                    const std::string& document_url,
                    base::Value::List arguments,
                    InvokeMethodCallback callback) override;
  void InvokeMethodInternal(int routing_id,
                            int32_t object_id,
                            const std::string& method_name,
                            const std::string& document_url,
                            base::Value::List arguments,
                            InvokeMethodCallback callback);
  void InvokeMethodAsync(const std::string& method_name,
                         const std::string& document_url,
                    base::Value::List arguments) override;

  void InvokeMethodFlowbuf(const std::string& method_name,
                           const std::string& document_url,
                           base::Value::List arguments,
                           int fd,
                           InvokeMethodFlowbufCallback callback) override;
  void InvokeMethodFlowbufInternal(int routing_id,
                                   int32_t object_id,
                                   const std::string& method_name,
                                   const std::string& document_url,
                                   base::Value::List arguments,
                                   int fd,
                                   InvokeMethodFlowbufCallback callback);

 private:
  friend class base::RefCountedThreadSafe<OhGinJavascriptBridgeDispatcherHost>;

  // object id and ace js function name
  typedef std::map<ObjectID, std::string> ObjectMap;

  ~OhGinJavascriptBridgeDispatcherHost();
  void ClearAllReceivers();
  void ObjectDisconnected();

  // Run on the UI thread.
  content::mojom::OhGinJavascriptBridge* GetJavaBridge(content::RenderFrameHost* frame_host,
                                                       bool should_create);

  // Run on the UI thread.
  content::mojom::OhGinJavascriptBridge* GetJavascriptBridge(content::RenderFrameHost* frame_host,
                                      bool should_create);

  // When registering an Arkts object, all the registered methods will be
  // cleared.
  void AddNamedObjectForWebController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const std::vector<std::string>& async_method_list,
      const std::string& permission);

  void AddNamedObjectForWebViewController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const std::vector<std::string>& async_method_list,
      const ObjectID object_id,
      const std::string& permission);

  // When registering a native object, only the registered methods
  // of the same type (sync/async) as the registration methods will be cleared.
  void AddNativeNamedObjectForWebController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      bool is_async,
      const std::string& permission);

  void AddNativeNamedObjectForWebViewController(
      const std::string& object_name,
      const std::vector<std::string>& method_list,
      const ObjectID object_id,
      bool is_async,
      const std::string& permission);
  content::WebContentsImpl* web_contents() const;
  void RemoteDisconnected(const content::GlobalRenderFrameHostId& routing_id);

  void ParseJson(const std::string& json_data,
                 const int32_t& object_id,
                 bool is_async);

  void ParseJsProxyPermissionJson(
      std::optional<base::Value>& root,
      const std::string& path,
      std::map<std::string, std::map<std::string, JsProxyPermissionConfigData>>*
          data_ptr);

  void ParseJsProxyPermissionJsonConfigMap(
      const base::Value::Dict* dict_val,
      std::map<std::string, JsProxyPermissionConfigData*>* config_ptr,
      JsProxyPermissionConfigData* data_ptr);

  bool CheckIsInJsPermission(const std::string& document_url,
                             const std::string& method_name,
                             int32_t object_id,
                             bool is_async);

  // Run on the UI thread.
  FrameInfos GetLastCallingFrameInfos(int32_t routing_id);
  void SetLastCallingFrameInfosOnUIThread(int32_t routing_id, const std::string& document_url);

  // js property name and object id
  using MethodPair = std::pair<std::string, std::unordered_set<std::string>>;
  using ObjectMethodMap = std::map<ObjectID, MethodPair>;
  using PermissionMap =
      std::map<int32_t,
               std::map<std::string,
                        std::map<std::string, JsProxyPermissionConfigData>>>;
  ObjectMethodMap sync_method_map_;
  ObjectMethodMap async_method_map_;
  PermissionMap javascript_sync_permission_map_;
  PermissionMap javascript_async_permission_map_;
  std::shared_mutex share_mutex_;
  int32_t object_id_ = MIN_NATIVE_OBJ_ID;

  std::unordered_map<int32_t, std::unordered_set<std::string>> object_id_map_;
  std::unordered_map<int32_t, std::unordered_set<std::string>>
      async_object_id_map_;
  CefRefPtr<ArkWebClientExt> client_;
  scoped_refptr<base::SingleThreadTaskRunner> async_task_runner_;

  mojo::ReceiverSet<content::mojom::OhGinJavascriptBridgeHost, content::GlobalRenderFrameHostId>
      receivers_;
  mojo::ReceiverSet<
      content::mojom::OhGinJavascriptBridgeRemoteObject,
      std::pair<content::GlobalRenderFrameHostId, int>>
      object_receivers_;
  std::map<content::GlobalRenderFrameHostId,
           mojo::AssociatedRemote<content::mojom::OhGinJavascriptBridge>>
      remotes_;
};
}  // namespace NWEB
#endif
