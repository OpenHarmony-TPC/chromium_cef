/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_SSL_PLATFORM_U_OHOS_KEY_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_SSL_PLATFORM_U_OHOS_KEY_
#pragma once

#include <stdio.h>

#include <string>

#include "crypto/openssl_util.h"
#include "include/cef_request_handler.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

class SSLPlatformUKeyOHOS : public net::ThreadedSSLPrivateKey::Delegate {
 public:
  explicit SSLPlatformUKeyOHOS(const std::string& uri, size_t key_size) : uri_(uri), key_size_(key_size) {
    std::unique_ptr<OHOS::NWeb::CertManagerAdapter> rootCertDataAdapter =
        OHOS::NWeb::OhosAdapterHelper::GetInstance().GetRootCertDataAdapter();
    if (rootCertDataAdapter == nullptr) {
      LOG(ERROR) << "root cert data adapter is null";
      return;
    }
    rootCertDataAdapter->OpenUKeyRemoteHandle(uri);
  }

  SSLPlatformUKeyOHOS(const SSLPlatformUKeyOHOS&) = delete;
  SSLPlatformUKeyOHOS& operator=(const SSLPlatformUKeyOHOS&) = delete;

  ~SSLPlatformUKeyOHOS() override {
    std::unique_ptr<OHOS::NWeb::CertManagerAdapter> rootCertDataAdapter =
        OHOS::NWeb::OhosAdapterHelper::GetInstance().GetRootCertDataAdapter();
    if (rootCertDataAdapter == nullptr) {
      LOG(ERROR) << "root cert data adapter is null";
      return;
    }
    rootCertDataAdapter->CloseUKeyRemoteHandle(uri_);
  }

  std::string GetProviderName() override { return "OHOS ukey cert manager"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return {
        // Only SHA-1 if the server supports no other hashes, but otherwise
        // prefer smaller SHA-2 hashes. SHA-256 is considered fine and more
        // likely to be supported by smartcards, etc.
        SSL_SIGN_RSA_PKCS1_SHA256,
        SSL_SIGN_RSA_PKCS1_SHA384,
        SSL_SIGN_RSA_PKCS1_SHA512,

        // Order PSS last so we preferentially use the more conservative
        // option. While the platform APIs may support RSA-PSS, the key may
        // not. Ideally the SSLPrivateKey would query this, but smartcards
        // often do not support such queries well.
        SSL_SIGN_RSA_PSS_SHA256,
        SSL_SIGN_RSA_PSS_SHA384,
        SSL_SIGN_RSA_PSS_SHA512,
        SSL_SIGN_ECDSA_SECP256R1_SHA256,
        SSL_SIGN_ECDSA_SECP384R1_SHA384,
        SSL_SIGN_ECDSA_SECP521R1_SHA512,
    };
  }

  net::Error Sign(uint16_t algorithm,
                  base::span<const uint8_t> input,
                  std::vector<uint8_t>* signature) override {
    LOG(DEBUG) << "SSLPlatformUKeyOHOS Sign algorithm: " << algorithm;

    if (!signature) {
        LOG(ERROR) << "SSLPlatformUKeyOHOS Sign signature is nullptr";
        return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    
    std::unique_ptr<OHOS::NWeb::CertManagerAdapter> rootCertDataAdapter =
        OHOS::NWeb::OhosAdapterHelper::GetInstance().GetRootCertDataAdapter();
    if (rootCertDataAdapter == nullptr) {
      LOG(ERROR)
          << "OHOS cert manager sign failed, root cert data adapter is null";
      return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    constexpr int32_t max_length = 1000;
    signature->resize(max_length, 0);

    uint32_t size = signature->size();
    auto ret =
        rootCertDataAdapter->SignUsingHuks(uri_, input.data(), input.size(),
                                  signature->data(), &size, algorithm, key_size_);
    signature->resize(size);
    LOG(DEBUG) << "SSLPlatformUKeyOHOS Sign SignHuks size: " << size;

    if (ret != 0) {
      LOG(ERROR) << "OHOS ukey cert manager sign failed, ret = " << ret;
      return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    return net::OK;
  }

 private:
  std::string uri_;
  size_t key_size_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_SSL_PLATFORM_U_OHOS_KEY_
