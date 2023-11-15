// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oh_gin_javascript_bridge_message_filter.h"

#include <thread>
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/javascript/oh_gin_javascript_bridge_messages.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "oh_gin_javascript_bridge_dispatcher_host.h"

namespace {

const char kOhGinJavascriptBridgeMessageFilterKey[] =
    "OhGinJavascriptBridgeMessageFilter";

}  // namespace

namespace NWEB {
OhGinJavascriptBridgeMessageFilter::OhGinJavascriptBridgeMessageFilter(
    base::PassKey<OhGinJavascriptBridgeMessageFilter> pass_key,
    content::AgentSchedulingGroupHost& agent_scheduling_group)
    : BrowserMessageFilter(OhGinJavascriptBridgeMsgStart),
      agent_scheduling_group_(agent_scheduling_group),
      current_routing_id_(MSG_ROUTING_NONE) {}

OhGinJavascriptBridgeMessageFilter::~OhGinJavascriptBridgeMessageFilter() {
  LOG(INFO) << "OhGinJavascriptBridgeMessageFilter dtor";
}

void OhGinJavascriptBridgeMessageFilter::OnDestruct() const {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    delete this;
  } else {
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

bool OhGinJavascriptBridgeMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(
                base::IgnoreResult(
                &OhGinJavascriptBridgeMessageFilter::OnMessageReceivedThread),
                base::WrapRefCounted(this), message));
  return true;
}

bool OhGinJavascriptBridgeMessageFilter::OnMessageReceivedThread(
    const IPC::Message& message) {
  base::AutoReset<int32_t> routing_id(&current_routing_id_,
                                      message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(OhGinJavascriptBridgeMessageFilter, message)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeHostMsg_GetMethods, OnGetMethods)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeHostMsg_HasMethod, OnHasMethod)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeHostMsg_InvokeMethod,
                        OnInvokeMethod)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeHostMsg_ObjectWrapperDeleted,
                        OnObjectWrapperDeleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void OhGinJavascriptBridgeMessageFilter::AddRoutingIdForHost(
    OhGinJavascriptBridgeDispatcherHost* host,
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::AutoLock locker(hosts_lock_);
  hosts_[render_frame_host->GetRoutingID()] = host;
}

void OhGinJavascriptBridgeMessageFilter::RemoveHost(
    const OhGinJavascriptBridgeDispatcherHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.begin();
  while (iter != hosts_.end()) {
    if (iter->second == host)
      hosts_.erase(iter++);
    else
      ++iter;
  }
}

void OhGinJavascriptBridgeMessageFilter::RenderProcessExited(
    content::RenderProcessHost* rph,
    const content::ChildProcessTerminationInfo& info) {
#if DCHECK_IS_ON()
  {
    scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
        base::UserDataAdapter<OhGinJavascriptBridgeMessageFilter>::Get(
            &agent_scheduling_group_, kOhGinJavascriptBridgeMessageFilterKey);
    DCHECK_EQ(this, filter.get());
  }
#endif

  rph->RemoveObserver(this);

  agent_scheduling_group_.RemoveUserData(
      kOhGinJavascriptBridgeMessageFilterKey);
}

scoped_refptr<OhGinJavascriptBridgeMessageFilter>
OhGinJavascriptBridgeMessageFilter::FromHost(
    content::AgentSchedulingGroupHost& agent_scheduling_group,
    bool create_if_not_exists) {
  scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter =
      base::UserDataAdapter<OhGinJavascriptBridgeMessageFilter>::Get(
          &agent_scheduling_group, kOhGinJavascriptBridgeMessageFilterKey);

  if (!filter && create_if_not_exists) {
    filter = base::MakeRefCounted<OhGinJavascriptBridgeMessageFilter>(
        base::PassKey<OhGinJavascriptBridgeMessageFilter>(),
        agent_scheduling_group);
    agent_scheduling_group.AddFilter(filter.get());
    agent_scheduling_group.GetProcess()->AddObserver(filter.get());

    agent_scheduling_group.SetUserData(
        kOhGinJavascriptBridgeMessageFilterKey,
        std::make_unique<
            base::UserDataAdapter<OhGinJavascriptBridgeMessageFilter>>(
            filter.get()));
  }
  return filter;
}

scoped_refptr<OhGinJavascriptBridgeDispatcherHost>
OhGinJavascriptBridgeMessageFilter::FindHost() {
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.find(current_routing_id_);
  if (iter != hosts_.end())
    return iter->second;

  return nullptr;
}

void OhGinJavascriptBridgeMessageFilter::OnGetMethods(
    int32_t object_id,
    std::set<std::string>* returned_method_names) {
  scoped_refptr<OhGinJavascriptBridgeDispatcherHost> host = FindHost();
  if (host) {
    host->OnGetMethods(object_id, returned_method_names);
  } else {
    *returned_method_names = std::set<std::string>();
  }
}

void OhGinJavascriptBridgeMessageFilter::OnHasMethod(
    int32_t object_id,
    const std::string& method_name,
    bool* result) {
  scoped_refptr<OhGinJavascriptBridgeDispatcherHost> host = FindHost();
  if (host) {
    host->OnHasMethod(object_id, method_name, result);
  } else {
    *result = false;
  }
}

void OhGinJavascriptBridgeMessageFilter::OnInvokeMethod(
    int32_t object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    base::ListValue* wrapped_result,
    OhGinJavascriptBridgeError* error_code) {
  scoped_refptr<OhGinJavascriptBridgeDispatcherHost> host = FindHost();
  if (host) {
    host->OnInvokeMethod(current_routing_id_, object_id, method_name, arguments,
                         wrapped_result, error_code);
  } else {
    wrapped_result->Append(std::make_unique<base::Value>());
    *error_code = kOhGinJavascriptBridgeRenderFrameDeleted;
  }
}

void OhGinJavascriptBridgeMessageFilter::OnObjectWrapperDeleted(int object_id) {
  (void)object_id;
}
}  // namespace NWEB