// Copyright (c) 2025 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cef/ohos_cef_ext/libcef/browser/javascript/oh_gin_javascript_bridge_message_filter.h"

#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace NWEB {
namespace {

using testing::_;
using testing::Return;

// Helper class to test private methods through friend declaration
// Note: This class is declared as a friend in the implementation file
class OhGinJavascriptBridgeMessageFilterTestHelper {
 public:
  static void SetCurrentRoutingId(int32_t routing_id) {
    extern thread_local int32_t current_routing_id;
    current_routing_id = routing_id;
  }

  static int32_t GetCurrentRoutingId() {
    extern thread_local int32_t current_routing_id;
    return current_routing_id;
  }
};

class OhGinJavascriptBridgeMessageFilterTest : public testing::Test {
 protected:
  void SetUp() override {
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    render_process_host_ = std::make_unique<content::MockRenderProcessHost>(
        browser_context_.get());
    filter_ = base::MakeRefCounted<OhGinJavascriptBridgeMessageFilter>(
        base::PassKey<OhGinJavascriptBridgeMessageFilter>(),
        render_process_host_->GetAgentSchedulingGroup());
  }

  void TearDown() override {
    filter_ = nullptr;
    render_process_host_.reset();
    browser_context_.reset();
  }

  // Helper to create a test IPC message with a platform file attachment
  IPC::Message CreateMessageWithPlatformFileAttachment(int32_t routing_id,
                                                       int fd) {
    // Use the correct message type - OnInvokeMethodFlowbuf handles
    // OhGinJavascriptBridgeHostMsg_InvokeMethod with attachments
    IPC::Message message(routing_id,
                         OhGinJavascriptBridgeHostMsg_InvokeMethod);
    // Write message parameters according to the message definition
    IPC::ParamTraits<int32_t>::Write(&message, 123);  // object_id
    IPC::ParamTraits<std::string>::Write(&message, "https://example.com");  // document_url
    IPC::ParamTraits<std::string>::Write(&message, "testMethod");  // method_name
    base::Value::List arguments;
    IPC::ParamTraits<base::Value::List>::Write(&message, arguments);  // arguments

    // Add a platform file attachment
    auto attachment =
        base::MakeRefCounted<IPC::internal::PlatformFileAttachment>(fd);
    message.AddAttachment(attachment);
    return message;
  }

  // Helper to create a test IPC message with a mojo handle attachment
  IPC::Message CreateMessageWithMojoHandleAttachment(int32_t routing_id) {
    IPC::Message message(routing_id,
                         OhGinJavascriptBridgeHostMsg_InvokeMethod);
    // Write message parameters according to the message definition
    IPC::ParamTraits<int32_t>::Write(&message, 123);  // object_id
    IPC::ParamTraits<std::string>::Write(&message, "https://example.com");  // document_url
    IPC::ParamTraits<std::string>::Write(&message, "testMethod");  // method_name
    base::Value::List arguments;
    IPC::ParamTraits<base::Value::List>::Write(&message, arguments);  // arguments

    // Create a dummy mojo handle attachment (invalid handle for testing)
    auto attachment = IPC::MessageAttachment::CreateFromMojoHandle(
        mojo::ScopedHandle(mojo::Handle()),
        IPC::MessageAttachment::Type::MOJO_HANDLE);
    message.AddAttachment(attachment);
    return message;
  }

  // Helper to create a test IPC message without attachments
  IPC::Message CreateMessageWithoutAttachment(int32_t routing_id) {
    IPC::Message message(routing_id,
                         OhGinJavascriptBridgeHostMsg_GetMethods);
    IPC::ParamTraits<int32_t>::Write(&message, 123);  // object_id
    return message;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter_;
};

// ========================================================================
// Test: OnMessageReceivedThreadFlowbuf with valid attachment type
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnMessageReceivedThreadFlowbuf_ValidPlatformFileAttachment_NoCrash) {
  // Arrange
  base::ScopedFD fd(dup(0));  // Duplicate stdin for testing
  ASSERT_TRUE(fd.is_valid());
  IPC::Message message = CreateMessageWithPlatformFileAttachment(1, fd.get());

  // Act & Assert
  // The message should be handled without crashing when attachment type is valid
  bool result = filter_->OnMessageReceivedThreadFlowbuf(message);
  // Since we don't have a registered host, it should return false (unhandled)
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: OnMessageReceivedThreadFlowbuf with invalid attachment type
// This tests the type safety fix at lines 163-171
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnMessageReceivedThreadFlowbuf_InvalidMojoHandleAttachment_NoCrash) {
  // Arrange
  IPC::Message message = CreateMessageWithMojoHandleAttachment(1);

  // Act & Assert
  // The message should handle the invalid attachment type gracefully
  // After the fix, it should not crash due to type validation
  bool result = filter_->OnMessageReceivedThreadFlowbuf(message);
  // Should return false (handled but no matching message or invalid attachment)
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: OnMessageReceivedThread (without attachment)
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnMessageReceivedThread_NoAttachment_HandledCorrectly) {
  // Arrange
  IPC::Message message = CreateMessageWithoutAttachment(1);

  // Act
  bool result = filter_->OnMessageReceivedThread(message);

  // Assert
  // Message should be processed but no host registered
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: IsSameSite with same URL
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_SameUrl_ReturnsTrue) {
  // Arrange
  GURL site_instance_url("https://example.com");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("https://example.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_TRUE(result);
}

// ========================================================================
// Test: IsSameSite with subdomain
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_Subdomain_ReturnsTrue) {
  // Arrange
  GURL site_instance_url("https://example.com");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("https://subdomain.example.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_TRUE(result);
}

// ========================================================================
// Test: IsSameSite with different host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_DifferentHost_ReturnsFalse) {
  // Arrange
  GURL site_instance_url("https://example.com");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("https://other.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: IsSameSite with different scheme
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_DifferentScheme_ReturnsFalse) {
  // Arrange
  GURL site_instance_url("https://example.com");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("http://example.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: IsSameSite with non-HTTP scheme
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_NonHttpScheme_ReturnsTrue) {
  // Arrange
  GURL site_instance_url("data:text/html,<html></html>");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("https://any-site.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_TRUE(result);  // Non-HTTP schemes are always considered same site
}

// ========================================================================
// Test: IsSameSite with unisolated invalid URL
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       IsSameSite_UnisolatedInvalid_ReturnsTrue) {
  // Arrange
  GURL site_instance_url("http://unisolated.invalid/");
  filter_->SetSiteInstanceGurl(site_instance_url);
  GURL document_url("https://any-site.com");

  // Act
  bool result = filter_->IsSameSite(document_url);

  // Assert
  EXPECT_TRUE(result);  // Unisolated.invalid is always considered same site
}

// ========================================================================
// Test: SetSiteInstanceGurl
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       SetSiteInstanceGurl_EmptyUrl_SetsUrl) {
  // Arrange
  GURL site_instance_url("https://test.com");

  // Act
  filter_->SetSiteInstanceGurl(site_instance_url);

  // Assert
  GURL document_url("https://test.com");
  EXPECT_TRUE(filter_->IsSameSite(document_url));
}

// ========================================================================
// Test: SetSiteInstanceGurl idempotent
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       SetSiteInstanceGurl_AlreadySet_DoesNotChange) {
  // Arrange
  GURL first_url("https://first.com");
  GURL second_url("https://second.com");
  filter_->SetSiteInstanceGurl(first_url);

  // Act - Try to set a different URL (should be ignored)
  filter_->SetSiteInstanceGurl(second_url);

  // Assert - Should still use the first URL
  GURL document_url("https://first.com");
  EXPECT_TRUE(filter_->IsSameSite(document_url));
  GURL other_url("https://second.com");
  EXPECT_FALSE(filter_->IsSameSite(other_url));
}

// ========================================================================
// Test: OnGetMethods with no host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnGetMethods_NoHost_ReturnsEmptySet) {
  // Arrange
  OhGinJavascriptBridgeMessageFilterTestHelper::SetCurrentRoutingId(999);

  // Act
  std::set<std::string> returned_methods;
  filter_->OnGetMethods(1000, &returned_methods);

  // Assert
  EXPECT_TRUE(returned_methods.empty());
}

// ========================================================================
// Test: OnHasMethod with no host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnHasMethod_NoHost_ReturnsFalse) {
  // Arrange
  OhGinJavascriptBridgeMessageFilterTestHelper::SetCurrentRoutingId(999);

  // Act
  bool result = true;
  filter_->OnHasMethod(2000, "testMethod", &result);

  // Assert
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: Filter creation and basic operations
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       FilterCreation_CreatesValidFilter) {
  // Arrange & Act & Assert
  EXPECT_NE(filter_, nullptr);
}

// ========================================================================
// Test: OnDestruct called on correct thread
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnDestruct_OnUIThread_DeletesThis) {
  // Arrange & Act & Assert
  // The filter should be able to be destructed properly
  // This test mainly ensures no crashes during destruction
  auto test_filter = base::MakeRefCounted<OhGinJavascriptBridgeMessageFilter>(
      base::PassKey<OhGinJavascriptBridgeMessageFilter>(),
      render_process_host_->GetAgentSchedulingGroup());
  test_filter = nullptr;
  SUCCEED();
}

}  // namespace
}  // namespace NWEB
