// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CLIENT_CERT_IDENTITY_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CLIENT_CERT_IDENTITY_H_
#pragma once

#include <string>

#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"

// Simple ClientCertIdentity implementation.
// Note: this implementation of AcquirePrivateKey will always call the callback
// synchronously.
class ClientCertIdentityOhos : public net::ClientCertIdentity {
 public:
  ClientCertIdentityOhos(scoped_refptr<net::X509Certificate> cert,
                         scoped_refptr<net::SSLPrivateKey> key)
      : net::ClientCertIdentity(std::move(cert)), key_(std::move(key)) {}

  ~ClientCertIdentityOhos() override {}

  // ClientCertIdentity implementation:
  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override{};

 private:
  scoped_refptr<net::SSLPrivateKey> key_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CLIENT_CERT_IDENTITY_H_