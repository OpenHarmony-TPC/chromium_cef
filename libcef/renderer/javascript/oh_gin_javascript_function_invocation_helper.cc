// Copyright (c) 2022 Huawei Device Co., Ltd.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/javascript/oh_gin_javascript_function_invocation_helper.h"

#include <utility>

#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_errors.h"
#include "libcef/common/javascript/oh_gin_javascript_bridge_value.h"
#include "libcef/renderer/javascript/oh_gin_javascript_bridge_object.h"
#include "libcef/renderer/javascript/oh_gin_javascript_bridge_value_converter.h"

namespace {

const char kMethodInvocationAsConstructorDisallowed[] =
    "Javascript bridge method can't be invoked as a constructor";
const char kMethodInvocationOnNonInjectedObjectDisallowed[] =
    "Javascript bridge method can't be invoked on a non-injected object";
const char kMethodInvocationErrorMessage[] =
    "Javascript bridge method invocation error";

}  // namespace

namespace NWEB {
OhGinJavascriptFunctionInvocationHelper::
    OhGinJavascriptFunctionInvocationHelper(
        const std::string& method_name,
        const base::WeakPtr<OhGinJavascriptBridgeDispatcher>& dispatcher)
    : method_name_(method_name),
      dispatcher_(dispatcher),
      converter_(new OhGinJavascriptBridgeValueConverter(dispatcher)) {}

OhGinJavascriptFunctionInvocationHelper::
    ~OhGinJavascriptFunctionInvocationHelper() {}

v8::Local<v8::Value> OhGinJavascriptFunctionInvocationHelper::Invoke(
    gin::Arguments* args) {
  LOG(INFO) << "OhGinJavascriptFunctionInvocationHelper Invoke";
  if (!dispatcher_) {
    args->isolate()->ThrowException(v8::Exception::Error(
        gin::StringToV8(args->isolate(), kMethodInvocationErrorMessage)));
    return v8::Undefined(args->isolate());
  }

  if (args->IsConstructCall()) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationAsConstructorDisallowed)));
    return v8::Undefined(args->isolate());
  }

  OhGinJavascriptBridgeObject* object = NULL;
  if (!args->GetHolder(&object) || !object) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationOnNonInjectedObjectDisallowed)));
    return v8::Undefined(args->isolate());
  }

  base::ListValue arguments;
  {
    v8::HandleScope handle_scope(args->isolate());
    v8::Local<v8::Context> context = args->isolate()->GetCurrentContext();
    v8::Local<v8::Value> val;
    while (args->GetNext(&val)) {
      std::unique_ptr<base::Value> arg(converter_->FromV8Value(val, context));
      LOG(DEBUG) << "OhGinJavascriptFunctionInvocationHelper::Invoke call FromV8Value end";
      if (arg.get())
        arguments.Append(std::move(arg));
      else
        arguments.Append(std::make_unique<base::Value>());
    }
  }

  OhGinJavascriptBridgeError error;
  std::unique_ptr<base::Value> result = dispatcher_->InvokeJavascriptMethod(
      object->object_id(), method_name_, arguments, &error);
  LOG(DEBUG) << "OhGinJavascriptFunctionInvocationHelper::Invoke call InvokeJavascriptMethod end";
  if (!result.get()) {
    LOG(DEBUG)
        << "OhGinJavascriptFunctionInvocationHelper::Invoke result is null";
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), OhGinJavascriptBridgeErrorToString(error))));
    return v8::Undefined(args->isolate());
  }
  if (!result->is_blob()) {
    LOG(DEBUG)
        << "OhGinJavascriptFunctionInvocationHelper::Invoke result is not blob";
    return converter_->ToV8Value(result.get(),
                                 args->isolate()->GetCurrentContext());
  }

  std::unique_ptr<const OhGinJavascriptBridgeValue> gin_value =
      OhGinJavascriptBridgeValue::FromValue(result.get());
  if (gin_value->IsType(OhGinJavascriptBridgeValue::TYPE_OBJECT_ID)) {
    OhGinJavascriptBridgeObject* object_result = NULL;
    OhGinJavascriptBridgeDispatcher::ObjectID object_id;
    if (gin_value->GetAsObjectID(&object_id)) {
      object_result = dispatcher_->GetObject(object_id);
    }
    if (object_result) {
      LOG(DEBUG) << "OhGinJavascriptFunctionInvocationHelper::Invoke result is "
                    "blob, object_id = "
                 << (uint32_t)object_id;
      gin::Handle<OhGinJavascriptBridgeObject> controller =
          gin::CreateHandle(args->isolate(), object_result);
      if (controller.IsEmpty())
        return v8::Undefined(args->isolate());
      return controller.ToV8();
    }
  } else if (gin_value->IsType(OhGinJavascriptBridgeValue::TYPE_NONFINITE)) {
    float float_value;
    gin_value->GetAsNonFinite(&float_value);
    return v8::Number::New(args->isolate(), float_value);
  }
  return v8::Undefined(args->isolate());
}
}  // namespace NWEB
