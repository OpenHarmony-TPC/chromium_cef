// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oh_gin_javascript_bridge_dispatcher_host.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "libcef/browser/javascript/oh_gin_javascript_bridge_message_filter.h"
#include "libcef/browser/javascript/oh_javascript_injector.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_messages.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_value.h"
#include "libcef/common/values_impl.h"
#include "oh_gin_javascript_bridge_object_deletion_message_filter.h"

namespace {
#define JS_BRIDGE_BINARY_ARGS_COUNT 2
// For the sake of the storage API, make this quite large.
const size_t MAX_RECURSION_DEPTH = 11;
const size_t MAX_DATA_LENGTH = 10000;

class ValueConvertState {
 public:
  // Level scope which updates the current depth of some ValueConvertState.
  class Level {
   public:
    explicit Level(ValueConvertState* state) : state_(state) {
      state_->maxRecursionDepth_--;
    }
    ~Level() {
      state_->maxRecursionDepth_++;
    }

   private:
    ValueConvertState* state_;
  };

  explicit ValueConvertState()
      : maxRecursionDepth_(MAX_RECURSION_DEPTH) {}

  ValueConvertState(const ValueConvertState&) = delete;
  ValueConvertState& operator=(const ValueConvertState&) = delete;

  bool HasReachedMaxRecursionDepth() {
    return maxRecursionDepth_ <= 0;
  }

 private:
  size_t maxRecursionDepth_;
};

void StringSplit(std::string str,
                 const char split,
                 std::vector<std::string>& result) {
  std::istringstream iss(str);
  std::string token;
  while (getline(iss, token, split)) {
    result.push_back(token);
  }
}
}  // namespace

namespace NWEB {
OhGinJavascriptBridgeDispatcherHost::OhGinJavascriptBridgeDispatcherHost(
    content::WebContents* web_contents,
    CefRefPtr<CefClient> client)
    : content::WebContentsObserver(web_contents), client_(client) {}

OhGinJavascriptBridgeDispatcherHost::~OhGinJavascriptBridgeDispatcherHost() {
  LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::~"
                "OhGinJavascriptBridgeDispatcherHost called";
  client_.reset();
  std::unique_lock<std::shared_mutex> lock(share_mutex_);
  method_map_.clear();
}

// Run on the UI thread.
void OhGinJavascriptBridgeDispatcherHost::
    InstallFilterAndRegisterAllRoutingIds() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  {
    std::shared_lock<std::shared_mutex> lock(share_mutex_);
    if (method_map_.empty() ||
        !web_contents()->GetPrimaryMainFrame()->GetProcess()->GetChannel()) {
      return;
    }
  }

  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](content::RenderFrameHostImpl* frame) {
        content::AgentSchedulingGroupHost& agent_scheduling_group =
            frame->GetAgentSchedulingGroup();

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
          process_global_filter->AddRoutingIdForHost(this, frame);
        }

        per_asg_filter->AddRoutingIdForHost(this, frame);
      });
}

content::WebContentsImpl* OhGinJavascriptBridgeDispatcherHost::web_contents()
    const {
  return static_cast<content::WebContentsImpl*>(
      content::WebContentsObserver::web_contents());
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
  std::shared_lock<std::shared_mutex> lock(share_mutex_);
  for (ObjectMethodMap::const_iterator iter = method_map_.begin();
       iter != method_map_.end(); ++iter) {
    render_frame_host->Send(new OhGinJavascriptBridgeMsg_AddNamedObject(
        render_frame_host->GetRoutingID(), iter->second.first, iter->first));
  }
}

void OhGinJavascriptBridgeDispatcherHost::WebContentsDestroyed() {
  // Unretained() is safe because ForEachFrame() is synchronous.
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](content::RenderFrameHostImpl* frame) {
        content::AgentSchedulingGroupHost& agent_scheduling_group =
            frame->GetAgentSchedulingGroup();
        scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
            OhGinJavascriptBridgeMessageFilter::FromHost(
                agent_scheduling_group, /*create_if_not_exists=*/false);

        if (filter) {
          filter->RemoveHost(this);
        }
      });
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
  if (!filter) {
    InstallFilterAndRegisterAllRoutingIds();
  }
}

void OhGinJavascriptBridgeDispatcherHost::AddNamedObjectForWebController(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  {
    std::unique_lock<std::shared_mutex> lock(share_mutex_);
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
    object_id_++;

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

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&object_name,
           this](content::RenderFrameHostImpl* render_frame_host) {
            render_frame_host->Send(new OhGinJavascriptBridgeMsg_AddNamedObject(
                render_frame_host->GetRoutingID(), object_name,
                this->object_id_));
          });
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
    std::unique_lock<std::shared_mutex> lock(share_mutex_);
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

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&object_name,
           object_id](content::RenderFrameHostImpl* render_frame_host) {
            render_frame_host->Send(new OhGinJavascriptBridgeMsg_AddNamedObject(
                render_frame_host->GetRoutingID(), object_name, object_id));
          });
}

void OhGinJavascriptBridgeDispatcherHost::AddNamedObject(
    const std::string& object_name,
    const std::vector<std::string>& method_list,
    const ObjectID object_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In order to be compatible with older webcotroller,
  // Currently, the webcontroller save the object_id on the core side, get
  // object from ace side by object_name the webviewcontroller save the
  // object_id on the web_webview side, get object from web_webview side by
  // object_id
  if (object_id ==
      static_cast<ObjectID>(JavaScriptObjIdErrorCode::WEBCONTROLLERERROR)) {
    AddNamedObjectForWebController(object_name, method_list);
  } else {
    AddNamedObjectForWebViewController(object_name, method_list, object_id);
  }
}

void OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_lock<std::shared_mutex> lock(share_mutex_);
  if (method_map_.empty()) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject:Map "
                  "is empty!";
    return;
  }
  {
    ObjectMethodMap::iterator it;
    for (it = method_map_.begin(); it != method_map_.end(); ++it) {
      if (!(object_name == it->second.first) &&
          std::next(it) == method_map_.end()) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject:"
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

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&copied_name](content::RenderFrameHostImpl* render_frame_host) {
            render_frame_host->Send(
                new OhGinJavascriptBridgeMsg_RemoveNamedObject(
                    render_frame_host->GetRoutingID(), copied_name));
          });
}

void OhGinJavascriptBridgeDispatcherHost::OnGetMethods(
    int32_t object_id,
    std::set<std::string>* returned_method_names) {
  // get from web_webview side
  std::shared_lock<std::shared_mutex> lock(share_mutex_);
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
  *result = false;
  std::shared_lock<std::shared_mutex> lock(share_mutex_);
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
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "ParseCefValueTObaseValueHelper: result is null";
    return baseValue;
  }

  switch (result->GetType()) {
    case CefValueType::VTYPE_NULL: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_NULL";
      return std::make_unique<base::Value>();
    }
    case CefValueType::VTYPE_INT: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_INT = "
                 << result->GetInt();
      return std::make_unique<base::Value>(result->GetInt());
    }
    case CefValueType::VTYPE_DOUBLE: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_DOUBLE = "
                 << result->GetDouble();
      return std::make_unique<base::Value>(result->GetDouble());
    }
    case CefValueType::VTYPE_BOOL: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_BOOL = "
                 << result->GetBool();
      return std::make_unique<base::Value>(result->GetBool());
    }
    case CefValueType::VTYPE_STRING: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_STRING";
      return std::make_unique<base::Value>(result->GetString().ToString());
    }
    case CefValueType::VTYPE_LIST: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_LIST";
      auto cefArr = result->GetList();
      std::unique_ptr<base::Value::List> value =
          std::make_unique<base::Value::List>();
      if (!cefArr) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                      "ParseCefValueTObaseValueHelper: cefArr is null";
        break;
      }
      int size = static_cast<int>(cefArr->GetSize());
      for (int i = 0; i < size; ++i) {
        auto cefValTmp = cefArr->GetValue(i);
        auto baseValTmp = ParseCefValueTObaseValueHelper(cefValTmp);
        value->Append(std::move(*baseValTmp));
      }
      return std::make_unique<base::Value>(std::move(*value));
    }
    case CefValueType::VTYPE_DICTIONARY: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_DICTIONARY";
      auto cefDict = result->GetDictionary();
      std::unique_ptr<base::Value::Dict> value =
          std::make_unique<base::Value::Dict>();
      if (!cefDict) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                      "ParseCefValueTObaseValueHelper: cefDict is null";
        break;
      }
      CefDictionaryValue::KeyList cefKeys;
      cefDict->GetKeys(cefKeys);
      for (auto& key : cefKeys) {
        auto cefValTmp = cefDict->GetValue(key);
        auto baseValTmp = ParseCefValueTObaseValueHelper(cefValTmp);
        value->Set(key.ToString(), std::move(*baseValTmp));
      }
      return std::make_unique<base::Value>(std::move(*value));
    }
    case CefValueType::VTYPE_BINARY: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_BINARY";
      auto size = result->GetBinary()->GetSize();
      auto buff = std::make_unique<char[]>(size);
      result->GetBinary()->GetData(buff.get(), size, 0);
      int32_t objId;
      std::string str(buff.get());
      std::vector<std::string> strList;
      StringSplit(str, ';', strList);
      if (strList.size() != JS_BRIDGE_BINARY_ARGS_COUNT) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                      "ParseCefValueTObaseValueHelper: strList.size() == "
                   << strList.size() << " is error, str=" << str
                   << ", size=" << size;
        baseValue = OhGinJavascriptBridgeValue::CreateObjectIDValue(-1);
        break;
      }
      std::istringstream ss(strList[1]);
      ss >> objId;
      if (objId <= 0) {
        LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                      "ParseCefValueTObaseValueHelper: objId == "
                   << objId << " is error";
        baseValue = OhGinJavascriptBridgeValue::CreateObjectIDValue(-1);
        break;
      }
      if (strList[0] == "TYPE_OBJECT_ID") {
        baseValue = OhGinJavascriptBridgeValue::CreateObjectIDValue(objId);
        break;
      }

      return OhGinJavascriptBridgeValue::CreateObjectIDValue(objId);
    }
    case CefValueType::VTYPE_INVALID: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseCefValueTObaseValueHelper: VTYPE_INVALID";
      return std::make_unique<base::Value>();
    }
    default: {
      LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::"
                   "ParseCefValueTObaseValueHelper: not support value type";
      return std::make_unique<base::Value>();
    }
  }

  return baseValue;
}

CefRefPtr<CefValue> ParseBaseValueTOCefValueHelper(ValueConvertState* state, base::Value* value) {
  CefRefPtr<CefValue> cefValue = CefValue::Create();
  ValueConvertState::Level state_level(state);
  if (state->HasReachedMaxRecursionDepth()) {
      return cefValue;
  }
  if (!value) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "ParseBaseValueTOCefValueHelper: value is null ";
    return cefValue;
  }
  switch (value->type()) {
    case base::Value::Type::INTEGER: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: INTEGER = "
                 << value->GetInt();
      cefValue->SetInt(value->GetInt());
      return cefValue;
    }
    case base::Value::Type::DOUBLE: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: DOUBLE = "
                 << value->GetDouble();
      cefValue->SetDouble(value->GetDouble());
      return cefValue;
    }
    case base::Value::Type::BOOLEAN: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: BOOLEAN = "
                 << value->GetBool();
      cefValue->SetBool(value->GetBool());
      return cefValue;
    }
    case base::Value::Type::STRING: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: STRING";
      cefValue->SetStdString(*value->GetIfString());
      return cefValue;
    }
    case base::Value::Type::LIST: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: LIST";
      size_t length = value->GetList().size();
      length = std::min(length, MAX_DATA_LENGTH);
      auto cefList = CefListValue::Create();
      for (size_t i = 0; i < length; i++) {
        base::Value& child = value->GetList()[i];
        auto cefVal = ParseBaseValueTOCefValueHelper(state, &child);
        cefList->SetValue(i, cefVal);
      }
      cefValue->SetList(cefList);
      return cefValue;
    }
    case base::Value::Type::DICT: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: DICTIONARY";
      auto cefDict = CefDictionaryValue::Create();
      size_t index = 0;
      for (base::Value::Dict::iterator iter = value->GetDict().begin();
           iter != value->GetDict().end(); iter++) {
        index++;
        if (index > MAX_DATA_LENGTH) {
          break;
        }
        const std::string& key = iter->first;
        auto found = value->GetDict().Find(key);
        auto cefVal = ParseBaseValueTOCefValueHelper(state, found);
        cefDict->SetValue(CefString(key), cefVal);
      }
      cefValue->SetDictionary(cefDict);
      return cefValue;
    }
    case base::Value::Type::BINARY: {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: BINARY";
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
            std::string bin = std::string("TYPE_OBJECT_ID") + std::string(";") +
                              std::to_string(object_id);
            auto cefBin = CefBinaryValue::Create(bin.c_str(), bin.size());
            cefValue->SetBinary(cefBin);
          }
        }
      } else if (gin_value &&
                 gin_value->GetType() ==
                     OhGinJavascriptBridgeValue::TYPE_H5_OBJECT_ID) {
        if (OhGinJavascriptBridgeValue::ContainsOhGinJavascriptBridgeValue(
                value)) {
          std::string h5_object_id_with_names;
          if (gin_value->GetAsObjectIDWithMdNames(&h5_object_id_with_names)) {
            LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                          "ParseBaseValueTOCefValueHelper: TYPE_H5_OBJECT_ID "
                          "h5_object_id_with_names .empty()= "
                       << h5_object_id_with_names.empty();
            std::string bin = std::string("TYPE_H5_OBJECT_ID") +
                              std::string(";") + h5_object_id_with_names;
            auto cefBin = CefBinaryValue::Create(bin.c_str(), bin.size());
            cefValue->SetBinary(cefBin);
          }
        }
      } else if (gin_value &&
                 gin_value->GetType() ==
                     OhGinJavascriptBridgeValue::TYPE_H5_FUNCTION_ID) {
        if (OhGinJavascriptBridgeValue::ContainsOhGinJavascriptBridgeValue(
                value)) {
          OhGinJavascriptBridgeDispatcherHost::ObjectID h5_function_id;
          if (gin_value->GetAsObjectID(&h5_function_id)) {
            LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                          "ParseBaseValueTOCefValueHelper: TYPE_H5_FUNCTION_ID "
                          "h5_function_id == "
                       << h5_function_id;
            std::string bin = std::string("TYPE_H5_FUNCTION_ID") +
                              std::string(";") + std::to_string(h5_function_id);
            auto cefBin = CefBinaryValue::Create(bin.c_str(), bin.size());
            cefValue->SetBinary(cefBin);
          }
        }
      }
      return cefValue;
    }
    case base::Value::Type::NONE:
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::"
                    "ParseBaseValueTOCefValueHelper: NONE";
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
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "ParseCefValueTOBaseValue: result is null or empty";
    return value;
  }

  CefRefPtr<CefValue> argument = result->GetValue(0);
  value = ParseCefValueTObaseValueHelper(argument);

  return value;
}

CefRefPtr<CefListValue> ParseBaseValueTOCefValue(base::Value::List* value) {
  auto cefList = CefListValue::Create();
  if (!value) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::"
                  "ParseBaseValueTOCefValue: value is null";
    return cefList;
  }
  for (size_t i = 0; i < value->size(); ++i) {
    base::Value& child = (*value)[i];
    ValueConvertState state;
    auto cefVal = ParseBaseValueTOCefValueHelper(&state, &child);
    cefList->SetValue(i, cefVal);
  }
  return cefList;
}

void OhGinJavascriptBridgeDispatcherHost::OnInvokeMethod(
    int routing_id,
    int32_t object_id,
    const std::string& document_url,
    const std::string& method_name,
    const base::Value::List& arguments,
    base::Value::List* wrapped_result,
    OhGinJavascriptBridgeError* error_code) {
  OhJavascriptInjector* javascriptInjector =
     OhJavascriptInjector::FromWebContents(web_contents());
  if (javascriptInjector) {
    javascriptInjector->SetLastCallingFrameUrl(document_url);
  }
  base::Value::List* argument = const_cast<base::Value::List*>(&arguments);

  CefRefPtr<CefListValue> ceflistvalue = ParseBaseValueTOCefValue(argument);

  std::string classname = "";
  {
    std::shared_lock<std::shared_mutex> lock(share_mutex_);
    if (method_map_.find(object_id) != method_map_.end()) {
      MethodPair object_pair = method_map_[object_id];
      classname = object_pair.first;
    }
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

  wrapped_result->Append(
      base::Value::FromUniquePtrValue(ParseCefValueTOBaseValue(result)));
}

void OhGinJavascriptBridgeDispatcherHost::OnInvokeMethodFlowbuf(
    int routing_id,
    int32_t object_id,
    const std::string& document_url,
    const std::string& method_name,
    const base::Value::List& arguments,
    int fd,
    base::Value::List* wrapped_result,
    OhGinJavascriptBridgeError* error_code) {
  OhJavascriptInjector* javascriptInjector =
     OhJavascriptInjector::FromWebContents(web_contents());
  if (javascriptInjector) {
    javascriptInjector->SetLastCallingFrameUrl(document_url);
  }
  base::Value::List* argument = const_cast<base::Value::List*>(&arguments);

  CefRefPtr<CefListValue> ceflistvalue = ParseBaseValueTOCefValue(argument);

  std::string classname = "";
  {
    std::shared_lock<std::shared_mutex> lock(share_mutex_);
    if (method_map_.find(object_id) != method_map_.end()) {
      MethodPair object_pair = method_map_[object_id];
      classname = object_pair.first;
    }
  }

  std::string method = method_name;

  CefRefPtr<CefListValue> result = CefListValue::Create();

  if (!client_) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::OnInvokeMethodFlowbuf: "
                  "client_ is null";
    return;
  }
  // 为了兼容老版本webcotroller方式, classname可能为空
  int error = client_->NotifyJavaScriptResultFlowbuf(ceflistvalue, method, classname, fd,
                                                     result, routing_id, object_id);
  *error_code = static_cast<OhGinJavascriptBridgeError>(error);
  if (error != 0) {
    return;
  }

  wrapped_result->Append(
      base::Value::FromUniquePtrValue(ParseCefValueTOBaseValue(result)));
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

void OhGinJavascriptBridgeDispatcherHost::DoCallH5Function(
    int32_t routing_id,
    int32_t h5_object_id,
    const std::string&
        h5_method_name,  // IF h5_method_name empty, call anonymous function
    const std::vector<CefRefPtr<CefValue>>& args) {
  LOG(DEBUG) << "OhGinJavascriptBridgeDispatcherHost::DoCallH5Function in";
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());
  if (!web_contents_impl) {
    LOG(ERROR) << "OhGinJavascriptBridgeDispatcherHost::DoCallH5Function "
                  "web_contents_impl null";
    return;
  }

  content::RenderFrameHost* target_frame =
      web_contents_impl->GetTargetFramesIncludingPending(routing_id);

  if (target_frame) {
    LOG(DEBUG)
        << "OhGinJavascriptBridgeDispatcherHost::DoCallH5Function called";
    base::Value::List base_args;
    std::unique_ptr<base::Value> value = std::make_unique<base::Value>();
    for (auto& item : args) {
      value = ParseCefValueTObaseValueHelper(item);
      base_args.Append(base::Value::FromUniquePtrValue(std::move(value)));
    }

    target_frame->Send(new OhGinJavascriptBridgeMsg_DoCallH5Function(
        target_frame->GetRoutingID(), routing_id, h5_object_id, h5_method_name,
        base_args));
  }
}
}  // namespace NWEB
