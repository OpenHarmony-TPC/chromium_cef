// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oh_gin_javascript_bridge_dispatcher.h"
#include "base/auto_reset.h"
#include "cef/libcef/common/javascript/oh_gin_javascript_bridge_messages.h"
#include "cef/libcef/renderer/javascript/oh_gin_javascript_bridge_object.h"
#include "content/public/renderer/render_frame.h"

#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
namespace NWEB {
OhGinJavascriptBridgeDispatcher::OhGinJavascriptBridgeDispatcher(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      inside_did_clear_window_object_(false) {}

OhGinJavascriptBridgeDispatcher::~OhGinJavascriptBridgeDispatcher() {}

bool OhGinJavascriptBridgeDispatcher::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(OhGinJavascriptBridgeDispatcher, msg)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeMsg_AddNamedObject,
                        OnAddNamedObject)
    IPC_MESSAGE_HANDLER(OhGinJavascriptBridgeMsg_RemoveNamedObject,
                        OnRemoveNamedObject)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void OhGinJavascriptBridgeDispatcher::DidClearWindowObject() {
  if (inside_did_clear_window_object_)
    return;
  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);
  for (NamedObjectMap::const_iterator iter = named_objects_.begin();
       iter != named_objects_.end(); ++iter) {
    if (objects_.Lookup(iter->second))
      objects_.Remove(iter->second);

    OhGinJavascriptBridgeObject* object =
        OhGinJavascriptBridgeObject::InjectNamed(render_frame()->GetWebFrame(),
                                                 AsWeakPtr(), iter->first,
                                                 iter->second);
    if (object) {
      objects_.AddWithID(object, iter->second);
    } else {
      LOG(DEBUG) << "OhGinJavascriptBridgeDispatcher::DidClearWindowObject "
                    "ObjectWrapperDeleted called";
      // Inform the host about wrapper creation failure.
      render_frame()->Send(
          new OhGinJavascriptBridgeHostMsg_ObjectWrapperDeleted(routing_id(),
                                                                iter->second));
    }
  }
}

void OhGinJavascriptBridgeDispatcher::OnAddNamedObject(const std::string& name,
                                                       ObjectID object_id) {
  named_objects_.insert(std::make_pair(name, object_id));
}

void OhGinJavascriptBridgeDispatcher::OnRemoveNamedObject(
    const std::string& name) {
  // Removal becomes in effect on next reload. We simply removing the entry
  // from the map here.
  NamedObjectMap::iterator iter = named_objects_.find(name);
  if (iter == named_objects_.end()) {
    return;
  }
  named_objects_.erase(iter);
}

void OhGinJavascriptBridgeDispatcher::GetJavascriptMethods(
    ObjectID object_id,
    std::set<std::string>* methods) {
  render_frame()->Send(new OhGinJavascriptBridgeHostMsg_GetMethods(
      routing_id(), object_id, methods));
}

bool OhGinJavascriptBridgeDispatcher::HasJavascriptMethod(
    ObjectID object_id,
    const std::string& method_name) {
  bool result;
  render_frame()->Send(new OhGinJavascriptBridgeHostMsg_HasMethod(
      routing_id(), object_id, method_name, &result));
  return result;
}

std::unique_ptr<base::Value>
OhGinJavascriptBridgeDispatcher::InvokeJavascriptMethod(
    ObjectID object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    OhGinJavascriptBridgeError* error) {
  base::ListValue result_wrapper;
  render_frame()->Send(new OhGinJavascriptBridgeHostMsg_InvokeMethod(
      routing_id(), object_id, method_name, arguments, &result_wrapper, error));
  base::Value* result;
  if (result_wrapper.Get(0, &result)) {
    return std::unique_ptr<base::Value>(result->CreateDeepCopy().release());
  } else {
    return std::unique_ptr<base::Value>();
  }
}

OhGinJavascriptBridgeObject* OhGinJavascriptBridgeDispatcher::GetObject(
    const ObjectID object_id) {
  OhGinJavascriptBridgeObject* result = objects_.Lookup(object_id);
  if (!result) {
    result =
        OhGinJavascriptBridgeObject::InjectAnonymous(AsWeakPtr(), object_id);
    if (result)
      objects_.AddWithID(result, object_id);
  }
  return result;
}

void OhGinJavascriptBridgeDispatcher::OnOhGinJavascriptBridgeObjectDeleted(
    const OhGinJavascriptBridgeObject* object) {
  int object_id = object->object_id();
  LOG(DEBUG) << "OhGinJavascriptBridgeDispatcher::"
                "OnOhGinJavascriptBridgeObjectDeleted called, object_id = "
             << object_id;
  // Ignore cleaning up of old object wrappers.
  if (objects_.Lookup(object_id) != object)
    return;
  LOG(DEBUG)
      << "OhGinJavascriptBridgeDispatcher::"
         "OnOhGinJavascriptBridgeObjectDeleted called, object_id = "
      << object_id
      << " ,new OhGinJavascriptBridgeHostMsg_ObjectWrapperDeleted called";
  objects_.Remove(object_id);
  render_frame()->Send(new OhGinJavascriptBridgeHostMsg_ObjectWrapperDeleted(
      routing_id(), object_id));
}

void OhGinJavascriptBridgeDispatcher::OnDestruct() {
  delete this;
}
}  // namespace NWEB
