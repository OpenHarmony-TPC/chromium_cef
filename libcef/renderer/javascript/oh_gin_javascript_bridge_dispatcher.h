// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHER_H
#define OH_GIN_JAVASCRIPT_BRIDGE_DISPATCHER_H

#include <map>
#include <memory>
#include <set>
#include "base/containers/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cef/libcef/common/javascript/oh_gin_javascript_bridge_errors.h"
#include "content/public/renderer/render_frame_observer.h"

namespace NWEB {
class OhGinJavascriptBridgeObject;
class OhGinJavascriptBridgeDispatcher
    : public base::SupportsWeakPtr<OhGinJavascriptBridgeDispatcher>,
      public content::RenderFrameObserver {
 public:
  OhGinJavascriptBridgeDispatcher(const OhGinJavascriptBridgeDispatcher&) =
      delete;
  OhGinJavascriptBridgeDispatcher& operator=(
      const OhGinJavascriptBridgeDispatcher&) = delete;

  using ObjectMap = base::IDMap<OhGinJavascriptBridgeObject*>;
  using ObjectID = ObjectMap::KeyType;

  explicit OhGinJavascriptBridgeDispatcher(content::RenderFrame* render_frame);

  ~OhGinJavascriptBridgeDispatcher() override;

  // RenderFrameObserver override:
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidClearWindowObject() override;

  void GetJavascriptMethods(ObjectID object_id, std::set<std::string>* methods);
  bool HasJavascriptMethod(ObjectID object_id, const std::string& method_name);

  std::unique_ptr<base::Value> InvokeJavascriptMethod(
      ObjectID object_id,
      const std::string& method_name,
      const base::ListValue& arguments,
      OhGinJavascriptBridgeError* error);

  OhGinJavascriptBridgeObject* GetObject(const ObjectID object_id);
  void OnOhGinJavascriptBridgeObjectDeleted(
      const OhGinJavascriptBridgeObject* object);

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;
  void OnAddNamedObject(const std::string& name, ObjectID object_id);
  void OnRemoveNamedObject(const std::string& name);

  typedef std::map<std::string, ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;
  ObjectMap objects_;
  bool inside_did_clear_window_object_;
};
}  // namespace NWEB
#endif
