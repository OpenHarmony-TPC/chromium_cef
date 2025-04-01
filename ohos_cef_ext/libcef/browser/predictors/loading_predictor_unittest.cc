// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_cef_ext/libcef/browser/predictors/loading_predictor.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/test/test_network_context.h"

namespace predictors {

namespace {

// First two are preconnectable, last one is not (see SetUp()).
const char kUrl[] = "http://localhost/";

class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() = default;
  ~MockNetworkContext() override = default;

  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {
    LookUpProxyForURLInner();
    mojo::Remote<network::mojom::ProxyLookupClient> ptr(
        std::move(proxy_lookup_client));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &network::mojom::ProxyLookupClient::OnProxyLookupComplete,
            std::move(ptr), net::OK, absl::nullopt));
  }

  void ResolveHost(
      const net::HostPortPair& host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override {
    ResolveHostInner();
    mojo::Remote<network::mojom::ResolveHostClient> ptr(
        std::move(response_client));
    net::ResolveErrorInfo resolve_error_info;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&network::mojom::ResolveHostClient::OnComplete,
                       std::move(ptr), net::OK, resolve_error_info,
                       absl::nullopt));
  }

  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      bool allow_credentials,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    PreconnectSocketsInner(url);
  }

  MOCK_METHOD(void, LookUpProxyForURLInner, ());
  MOCK_METHOD(void, ResolveHostInner, ());
  MOCK_METHOD(void, PreconnectSocketsInner, (const GURL& url));
};

}  // namespace

class HwLoadingPredictorTest : public testing::Test {
 public:
  void SetUp() override {
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
    aw_feature_list_creator_ =
        std::make_unique<android_webview::AwFeatureListCreator>();
    aw_feature_list_creator_->CreateLocalState();
    browser_process_ = std::make_unique<android_webview::AwBrowserProcess>(
        aw_feature_list_creator_.get());
    aw_browser_context_ = std::make_unique<android_webview::AwBrowserContext>();
    mock_network_context_ = std::make_unique<MockNetworkContext>();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    predictor_ = std::make_unique<LoadingPredictor>(LoadingPredictorConfig(),
                                                    browser_context_.get());
    predictor_->StartInitialization();

    PreconnectManager* preconnect_manager = predictor_->preconnect_manager();
    preconnect_manager->SetNetworkContextForTesting(
        mock_network_context_.get());
  }
  void TearDown() override {
    predictor_->Shutdown();
    predictor_.reset();
    browser_context_.reset();
    mock_network_context_.reset();

    base::RunLoop().RunUntilIdle();
    aw_browser_context_.reset();
    test_content_client_initializer_.reset();
    browser_process_.reset();
  }

 protected:
  // Create the TestBrowserThreads.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockNetworkContext> mock_network_context_;
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<LoadingPredictor> predictor_;

  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<android_webview::AwFeatureListCreator>
      aw_feature_list_creator_;
  std::unique_ptr<android_webview::AwBrowserProcess> browser_process_;
  std::unique_ptr<android_webview::AwBrowserContext> aw_browser_context_;
};

TEST_F(HwLoadingPredictorTest, TestPrepareForPageLoad) {
  GURL url = GURL(kUrl);
  url::Origin origin = url::Origin::Create(url);
  net::NetworkAnonymizationKey network_anonymization_key(origin, origin);

  EXPECT_CALL(*mock_network_context_, LookUpProxyForURLInner());
  EXPECT_CALL(*mock_network_context_, ResolveHostInner());
  EXPECT_CALL(*mock_network_context_, PreconnectSocketsInner(url));

  predictor_->StartInitialization();

  bool preconnectable = true;
  predictor_->PrepareForPageLoad(url, HintOrigin::OMNIBOX, preconnectable);

  base::RunLoop().RunUntilIdle();
}

}  // namespace predictors
