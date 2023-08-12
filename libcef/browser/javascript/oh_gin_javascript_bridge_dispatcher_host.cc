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
#include "libcef/common/values_impl.h"
#include "oh_gin_javascript_bridge_object_deletion_message_filter.h"

namespace NWEB {
OhGinJavascriptBridgeDispatcherHost::OhGinJavascriptBridgeDispatcherHost(
    content::WebContents* web_contents,
    CefRefPtr<CefClient> client)
    : content::WebContentsObserver(web_contents), client_(client) {}

OhGinJavascriptBridgeDispatcherHost::~OhGinJavascriptBridgeDispatcherHost() {
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

void OhGinJavascriptBridgeDispatcherHost::AddNamedObject(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  LOG(INFO) << "AddNamedObject::" << object_name;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  {
    std::unique_lock<std::mutex> lk(object_mtx_);
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

void OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (method_map_.empty()) {
    LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::RemoveNamedObject:Map "
                 "is empty!";
    return;
  }
  {
    std::unique_lock<std::mutex> lk(object_mtx_);
    ObjectMethodMap::iterator it;
    for (it = method_map_.begin(); it != method_map_.end(); ++it) {
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
}

// OhGinJavascriptBridgeDispatcherHost::FindObject(

void OhGinJavascriptBridgeDispatcherHost::OnGetMethods(
    int32_t object_id,
    std::set<std::string>* returned_method_names) {
  LOG(INFO) << "OhGinJavascriptBridgeDispatcherHost::OnGetMethods::"
            << object_id;
  if (method_map_.find(object_id) == method_map_.end()) {
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
  if (method_map_.find(object_id) == method_map_.end()) {
    *result = false;
    return;
  }

  MethodPair p = method_map_[object_id];
  if (p.second.find(method_name) == p.second.end()) {
    *result = false;
    return;
  }

  *result = true;
}

std::unique_ptr<base::Value> ParseValueTONWebValue(
    CefRefPtr<CefListValue> result) {
  std::unique_ptr<base::Value> value = std::make_unique<base::Value>();
  if (!result.get() || result->GetSize() == 0) {
    return value;
  }

  CefRefPtr<CefValue> argument = result->GetValue(0);

  switch (argument->GetType()) {
    case CefValueType::VTYPE_INT:
      value = std::make_unique<base::Value>(result->GetInt(0));
      break;
    case CefValueType::VTYPE_DOUBLE: {
      value = std::make_unique<base::Value>(result->GetDouble(0));
      break;
    }
    case CefValueType::VTYPE_BOOL:
      value = std::make_unique<base::Value>(result->GetBool(0));
      break;
    case CefValueType::VTYPE_STRING:
      value = std::make_unique<base::Value>(result->GetString(0).ToString());
      break;
    case CefValueType::VTYPE_INVALID:
      value = std::make_unique<base::Value>();
      break;
    default:
      value = std::make_unique<base::Value>();
      break;
  }

  return value;
}

void OhGinJavascriptBridgeDispatcherHost::OnInvokeMethod(
    int routing_id,
    int32_t object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    base::ListValue* wrapped_result,
    OhGinJavascriptBridgeError* error_code) {
  (void)routing_id;
  LOG(INFO) << "OnInvokeMethod method_name : " << method_name.c_str();
  if (method_map_.find(object_id) == method_map_.end()) {
    *error_code = kOhGinJavascriptBridgeUnknownObjectId;
    return;
  }
  base::ListValue* argument = const_cast<base::ListValue*>(&arguments);

  CefRefPtr<CefListValueImpl> ceflistvalue =
      new CefListValueImpl(argument, false, false);

  MethodPair object_pair = method_map_[object_id];

  std::string method = method_name;
  std::string classname = object_pair.first;

  CefRefPtr<CefListValue> result = CefListValue::Create();

  int error =
      client_->NotifyJavaScriptResult(ceflistvalue, method, classname, result);
  *error_code = OhGinJavascriptBridgeError(error);
  if (error != 0) {
    return;
  }

  wrapped_result->Append(ParseValueTONWebValue(result));
}
}  // namespace NWEB