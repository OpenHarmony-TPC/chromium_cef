// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oh_gin_javascript_bridge_dispatcher_host.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "libcef/browser/javascript/oh_gin_javascript_bridge_message_filter.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_messages.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_value.h"
#include "libcef/common/values_impl.h"
#include "oh_gin_javascript_bridge_object_deletion_message_filter.h"

namespace NWEB {
OhGinJavascriptBridgeDispatcherHost::OhGinJavascriptBridgeDispatcherHost(
    content::WebContents* web_contents,
    CefRefPtr<CefClient> client)
    : content::WebContentsObserver(web_contents), client_(client) {}

OhGinJavascriptBridgeDispatcherHost::~OhGinJavascriptBridgeDispatcherHost() {
  LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::~"
                "OhGinJavascriptBridgeDispatcherHost called";
  client_.reset();
  method_map_.clear();
}

// Run on the UI thread.
void OhGinJavascriptBridgeDispatcherHost::
    InstallFilterAndRegisterAllRoutingIds() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (method_map_.empty() ||
      !web_contents()->GetMainFrame()->GetProcess()->GetChannel()) {
    return;
  }

  // Unretained() is safe because ForEachFrame() is synchronous.
  web_contents()->ForEachFrame(base::BindRepeating(
      [](OhGinJavascriptBridgeDispatcherHost* host,
         content::RenderFrameHost* frame) {
        content::AgentSchedulingGroupHost& agent_scheduling_group =
            static_cast<content::RenderFrameHostImpl*>(frame)
                ->GetAgentSchedulingGroup();

        scoped_refptr<OhGinJavascriptBridgeMessageFilter> per_asg_filter =
            OhGinJavascriptBridgeMessageFilter::FromHost(
                agent_scheduling_group,
                /*create_if_not_exists=*/true);
        if (base::FeatureList::IsEnabled(features::kMBIMode)) {
          scoped_refptr<OhGinJavascriptBridgeObjectDeletionMessageFilter>
              process_global_filter =
                  OhGinJavascriptBridgeObjectDeletionMessageFilter::FromHost(
                      agent_scheduling_group.GetProcess(),
                      /*create_if_not_exists=*/true);
          process_global_filter->AddRoutingIdForHost(host, frame);
        }

        per_asg_filter->AddRoutingIdForHost(host, frame);
      },
      base::Unretained(this)));
}

void OhGinJavascriptBridgeDispatcherHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::AgentSchedulingGroupHost& agent_scheduling_group =
      static_cast<content::RenderFrameHostImpl*>(render_frame_host)
          ->GetAgentSchedulingGroup();
  if (scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
          OhGinJavascriptBridgeMessageFilter::FromHost(
              agent_scheduling_group, /*create_if_not_exists=*/false)) {
    filter->AddRoutingIdForHost(this, render_frame_host);
  } else {
    InstallFilterAndRegisterAllRoutingIds();
  }
  for (ObjectMethodMap::const_iterator iter = method_map_.begin();
       iter != method_map_.end(); ++iter) {
    render_frame_host->Send(new OhGinJavascriptBridgeMsg_AddNamedObject(
        render_frame_host->GetRoutingID(), iter->second.first, iter->first));
  }
}

void OhGinJavascriptBridgeDispatcherHost::WebContentsDestroyed() {
  // Unretained() is safe because ForEachFrame() is synchronous.
  web_contents()->ForEachFrame(base::BindRepeating(
      [](OhGinJavascriptBridgeDispatcherHost* host,
         content::RenderFrameHost* frame) {
        content::AgentSchedulingGroupHost& agent_scheduling_group =
            static_cast<content::RenderFrameHostImpl*>(frame)
                ->GetAgentSchedulingGroup();
        scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
            OhGinJavascriptBridgeMessageFilter::FromHost(
                agent_scheduling_group, /*create_if_not_exists=*/false);

        if (filter)
          filter->RemoveHost(host);
      },
      base::Unretained(this)));
}

void OhGinJavascriptBridgeDispatcherHost::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  content::AgentSchedulingGroupHost& agent_scheduling_group =
      static_cast<content::RenderViewHostImpl*>(new_host)
          ->GetAgentSchedulingGroup();
  scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
      OhGinJavascriptBridgeMessageFilter::FromHost(
          agent_scheduling_group,
          /*create_if_not_exists=*/false);
  if (!filter)
    InstallFilterAndRegisterAllRoutingIds();
}

void OhGinJavascriptBridgeDispatcherHost::AddNamedObjectForWebController(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  {
    ObjectMethodMap::iterator it;
    for (it = method_map_.begin(); it != method_map_.end(); ++it) {
      if (it->second.first == object_name) {
        std::unordered_set<std::string> method_set = it->second.second;
        for (std::string method : method_list) {
          method_set.emplace(method);
        }
        it->second.second = method_set;
        return;
      }
    }
    ++object_id_;

    MethodPair object_pair;
    std::unordered_set<std::string> method_set;
    for (std::string s : method_list) {
      method_set.emplace(s);
    }
    object_pair.first = object_name;
    object_pair.second = method_set;
    method_map_[object_id_] = object_pair;
  }

  InstallFilterAndRegisterAllRoutingIds();

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());

  web_contents_impl->SendToAllFramesIncludingPending(
      new OhGinJavascriptBridgeMsg_AddNamedObject(MSG_ROUTING_NONE, object_name,
                                                  object_id_));
}

void OhGinJavascriptBridgeDispatcherHost::AddNamedObjectForWebViewController(
    const std::string& object_name,
    const std::vector<std::string>& method_list,
    const ObjectID object_id) {
  if (object_id <= 0) {
    LOG(ERROR) << "AddNamedObject:" << object_name
               << " failed due to object_id == " << object_id;
    return;
  }
  RemoveNamedObject(object_name, method_list);
  {
    MethodPair object_pair;
    std::unordered_set<std::string> method_set;
    for (std::string s : method_list) {
      method_set.emplace(s);
    }
    object_pair.first = object_name;
    object_pair.second = method_set;
    method_map_[object_id] = object_pair;
  }

  InstallFilterAndRegisterAllRoutingIds();

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());

  web_contents_impl->SendToAllFramesIncludingPending(
      new OhGinJavascriptBridgeMsg_AddNamedObject(MSG_ROUTING_NONE, object_name,
                                                  object_id));
}

void OhGinJavascriptBridgeDispatcherHost::AddNamedObject(
    const std::string& object_name,
    const std::vector<std::string>& method_list,
    const ObjectID object_id) {
  LOG(INFO) << "AddNamedObject::" << object_name;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In order to be compatible with older webcotroller,
  // Currently, the webcontroller save the object_id on the core side, get object from ace side by object_name
  // the webviewcontroller save the object_id on the web_webview side, get object from web_webview side by object_id
  if (object_id ==
      static_cast<ObjectID>(JavaScriptObjIdErrorCode::WEBCONTROLLERERROR)) {
    AddNamedObjectForWebController(object_name, method_list);
  } else {
    AddNamedObjectForWebViewController(object_name, method_list, object_id);
  }
}

void OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject(
    const std::string& object_name,
    const std::vector<std::string>&
        method_list) {  // todo: method_list 参数删除
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (method_map_.empty()) {
    LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject:Map "
                 "is empty!";
    return;
  }
  {
    ObjectMethodMap::iterator it;
    for (it = method_map_.begin(); it != method_map_.end(); ++it) {
      if (!(object_name == it->second.first) &&
          std::next(it) == method_map_.end()) {
        LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject:"
                     "object_name:"
                  << object_name << " is not exist!";
        return;
      }
      if (!(object_name == it->second.first)) {
        continue;
      }
      method_map_.erase(it);
      break;
    }
  }

  // |name| may come from |named_objects_|. Make a copy of name so that if
  // |name| is from |named_objects_| it'll be valid after the remove below.
  const std::string copied_name(object_name);

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());
  web_contents_impl->SendToAllFramesIncludingPending(
      new OhGinJavascriptBridgeMsg_RemoveNamedObject(MSG_ROUTING_NONE,
                                                     copied_name));
}

void OhGinJavascriptBridgeDispatcherHost::DocumentAvailableInMainFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  (void)render_frame_host;
  if (!client_) {
    LOG(INFO)
        << "OhGinJavascriptBridgeDispatcherHost::DocumentAvailableInMainFrame: "
           "client_ is null";
    return;
  }
  LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                "DocumentAvailableInMainFrame called";
  client_->RemoveTransientJavaScriptObject();
}

void OhGinJavascriptBridgeDispatcherHost::OnGetMethods(
    int32_t object_id,
    std::set<std::string>* returned_method_names) {
  LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::OnGetMethods::"
            << object_id;
  // get from web_webview side
  if (method_map_.find(object_id) == method_map_.end()) {
    if (!client_) {
      LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                    "OnGetMethods: client_ is null";
      return;
    }
    CefRefPtr<CefValue> cefValue;
    client_->GetJavaScriptObjectMethods(object_id, cefValue);
    if (!cefValue) {
      LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                    "OnGetMethods: cefValue is null";
      return;
    }
    size_t length = cefValue->GetList()->GetSize();
    for (size_t i = 0; i < length; ++i) {
      returned_method_names->insert(
          cefValue->GetList()->GetValue(i)->GetString().ToString());
    }
    return;
  }
  MethodPair p = method_map_[object_id];
  for (auto iter = p.second.begin(); iter != p.second.end(); ++iter) {
    returned_method_names->emplace(*iter);
  }
}

void OhGinJavascriptBridgeDispatcherHost::OnHasMethod(
    int32_t object_id,
    const std::string& method_name,
    bool* result) {
  LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::OnHasMethod::"
            << method_name.c_str();
  *result = false;
  if (method_map_.find(object_id) != method_map_.end()) {
    MethodPair p = method_map_[object_id];
    if (p.second.find(method_name) != p.second.end()) {
      *result = true;
      return;
    }
  }

  if (!client_) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "OnHasMethod: client_ is null";
    return;
  }

  *result = client_->HasJavaScriptObjectMethods(object_id, method_name);
}

std::unique_ptr<base::Value> ParseCefValueTObaseValueHelper(
    CefRefPtr<CefValue> result) {
  std::unique_ptr<base::Value> baseValue = std::make_unique<base::Value>();
  if (!result.get()) {
    LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                 "ParseCefValueTObaseValueHelper: result is null";
    return baseValue;
  }

  switch (result->GetType()) {
    case CefValueType::VTYPE_NULL:
      baseValue = std::make_unique<base::Value>();
      break;
    case CefValueType::VTYPE_INT:
      baseValue = std::make_unique<base::Value>(result->GetInt());
      break;
    case CefValueType::VTYPE_DOUBLE: {
      baseValue = std::make_unique<base::Value>(result->GetDouble());
      break;
    }
    case CefValueType::VTYPE_BOOL:
      baseValue = std::make_unique<base::Value>(result->GetBool());
      break;
    case CefValueType::VTYPE_STRING:
      baseValue = std::make_unique<base::Value>(result->GetString().ToString());
      break;
    case CefValueType::VTYPE_LIST: {
      auto cefArr = result->GetList();
      baseValue = std::make_unique<base::ListValue>();
      if (!cefArr) {
        LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                     "ParseCefValueTObaseValueHelper: cefArr is null";
        break;
      }
      int size = static_cast<int>(cefArr->GetSize());
      for (int i = 0; i < size; ++i) {
        auto cefValTmp = cefArr->GetValue(i);
        auto baseValTmp = ParseCefValueTObaseValueHelper(cefValTmp);
        baseValue->Append(std::move(*baseValTmp));
      }
      break;
    }
    case CefValueType::VTYPE_DICTIONARY: {
      auto cefDict = result->GetDictionary();
      baseValue = std::make_unique<base::DictionaryValue>();
      if (!cefDict) {
        LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                     "ParseCefValueTObaseValueHelper: cefDict is null";
        break;
      }
      CefDictionaryValue::KeyList cefKeys;
      cefDict->GetKeys(cefKeys);
      for (auto& key : cefKeys) {
        auto cefValTmp = cefDict->GetValue(key);
        auto baseValTmp = ParseCefValueTObaseValueHelper(cefValTmp);
        baseValue->SetKey(key.ToString(), std::move(*baseValTmp));
      }
      break;
    }
    case CefValueType::VTYPE_BINARY: {
      auto size = result->GetBinary()->GetSize();
      auto buff = std::make_unique<char[]>(size);
      result->GetBinary()->GetData(buff.get(), size, 0);
      int32_t objId;
      std::string str(buff.get());
      std::istringstream ss(str);
      ss >> objId;
      if (objId <= 0) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                      "ParseCefValueTObaseValueHelper: objId == "
                   << objId << " is error";
      }

      baseValue = OhGinJavascriptBridgeValue::CreateObjectIDValue(objId);
      break;
    }
    case CefValueType::VTYPE_INVALID:
      baseValue = std::make_unique<base::Value>();
      break;
    default:
      LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                   "ParseCefValueTObaseValueHelper: not support value type";
      baseValue = std::make_unique<base::Value>();
      break;
  }

  return baseValue;
}

CefRefPtr<CefValue> ParseBaseValueTOCefValueHelper(base::Value* value) {
  CefRefPtr<CefValue> cefValue = CefValue::Create();
  if (!value) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "ParseBaseValueTOCefValueHelper: value is null ";
    return cefValue;
  }
  switch (value->type()) {
    case base::Value::Type::INTEGER:
      cefValue->SetInt(value->GetInt());
      return cefValue;
    case base::Value::Type::DOUBLE: {
      cefValue->SetDouble(value->GetDouble());
      return cefValue;
    }
    case base::Value::Type::BOOLEAN:
      cefValue->SetBool(value->GetBool());
      return cefValue;
    case base::Value::Type::STRING:
      cefValue->SetString(CefString(*value->GetIfString()));
      return cefValue;
    case base::Value::Type::LIST: {
      size_t length = value->GetList().size();
      auto cefList = CefListValue::Create();
      for (size_t i = 0; i < length; i++) {
        base::Value& child = value->GetList()[i];
        auto cefVal = ParseBaseValueTOCefValueHelper(&child);
        cefList->SetValue(i, cefVal);
      }
      cefValue->SetList(cefList);
      return cefValue;
    }
    case base::Value::Type::DICTIONARY: {
      auto cefDict = CefDictionaryValue::Create();
      auto ptr = static_cast<const base::DictionaryValue*>(value);
      for (base::DictionaryValue::Iterator iter(*ptr); !iter.IsAtEnd();
           iter.Advance()) {
        const std::string& key = iter.key();
        auto found = value->FindKey(key);
        auto cefVal = ParseBaseValueTOCefValueHelper(found);
        cefDict->SetValue(CefString(key), cefVal);
      }
      cefValue->SetDictionary(cefDict);
      return cefValue;
    }
    case base::Value::Type::BINARY: {
      std::unique_ptr<const OhGinJavascriptBridgeValue> gin_value(
          OhGinJavascriptBridgeValue::FromValue(value));
      if (gin_value &&
          gin_value->GetType() == OhGinJavascriptBridgeValue::TYPE_OBJECT_ID) {
        if (OhGinJavascriptBridgeValue::ContainsOhGinJavascriptBridgeValue(
                value)) {
          OhGinJavascriptBridgeDispatcherHost::ObjectID object_id;
          if (gin_value->GetAsObjectID(&object_id)) {
            if (object_id <= 0) {
              LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                            "ParseBaseValueTOCefValueHelper: object_id == "
                         << object_id << " is error";
            }
            std::string bin = std::to_string(object_id);
            auto cefDict = CefBinaryValue::Create(bin.c_str(), bin.size());
            cefValue->SetBinary(cefDict);
          }
        }
      }
      return cefValue;
    }
    case base::Value::Type::NONE:
      break;
    default:
      LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                   "ParseBaseValueTOCefValueHelper: not support value type";
      break;
  }
  return cefValue;
}

std::unique_ptr<base::Value> ParseCefValueTOBaseValue(
    CefRefPtr<CefListValue> result) {
  std::unique_ptr<base::Value> value = std::make_unique<base::Value>();
  if (!result.get() || result->GetSize() == 0) {
    LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                 "ParseCefValueTOBaseValue: result is null or empty";
    return value;
  }

  CefRefPtr<CefValue> argument = result->GetValue(0);
  value = ParseCefValueTObaseValueHelper(argument);

  return value;
}

CefRefPtr<CefListValue> ParseBaseValueTOCefValue(base::ListValue* value) {
  auto cefList = CefListValue::Create();
  if (!value) {
    LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                 "ParseBaseValueTOCefValue: value is null";
    return cefList;
  }
  for (size_t i = 0; i < value->GetList().size(); ++i) {
    base::Value& child = value->GetList()[i];
    auto cefVal = ParseBaseValueTOCefValueHelper(&child);
    cefList->SetValue(i, cefVal);
  }
  return cefList;
}

void OhGinJavascriptBridgeDispatcherHost::OnInvokeMethod(
    int routing_id,
    int32_t object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    base::ListValue* wrapped_result,
    OhGinJavascriptBridgeError* error_code) {
  LOG(INFO) << "OnInvokeMethod method_name : " << method_name.c_str();

  base::ListValue* argument = const_cast<base::ListValue*>(&arguments);

  CefRefPtr<CefListValue> ceflistvalue = ParseBaseValueTOCefValue(argument);

  std::string classname = "";
  if (method_map_.find(object_id) != method_map_.end()) {
    MethodPair object_pair = method_map_[object_id];
    classname = object_pair.first;
  }

  std::string method = method_name;

  CefRefPtr<CefListValue> result = CefListValue::Create();

  if (!client_) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::OnInvokeMethod: "
                  "client_ is null";
    return;
  }
  // 为了兼容老版本webcotroller方式, classname可能为空
  int error = client_->NotifyJavaScriptResult(ceflistvalue, method, classname,
                                              result, routing_id, object_id);
  *error_code = static_cast<OhGinJavascriptBridgeError>(error);
  if (error != 0) {
    return;
  }

  wrapped_result->Append(ParseCefValueTOBaseValue(result));
}

void OhGinJavascriptBridgeDispatcherHost::OnObjectWrapperDeleted(
    int routing_id,
    ObjectID object_id) {
  LOG(DEBUG)
      << "OhGinJavascriptBridgeDispatcherHost::OnObjectWrapperDeleted called";
  if (!client_) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "OnObjectWrapperDeleted: client_ is null";
    return;
  }
  client_->RemoveJavaScriptObjectHolder(routing_id, object_id);
}
}  // namespace NWEB
