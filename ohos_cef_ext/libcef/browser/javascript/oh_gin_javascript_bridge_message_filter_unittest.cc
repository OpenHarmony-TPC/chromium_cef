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

#define private public
#include "cef/ohos_cef_ext/libcef/browser/javascript/oh_gin_javascript_bridge_message_filter.h"
#undef private

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace NWEB {
namespace {

using testing::Return;
using testing::_;

// Mock OhGinJavascriptBridgeDispatcherHost for testing
class MockOhGinJavascriptBridgeDispatcherHost
    : public OhGinJavascriptBridgeDispatcherHost {
 public:
  MockOhGinJavascriptBridgeDispatcherHost() = default;

  MOCK_METHOD(void, OnGetMethods,
              (int32_t object_id, std::set<std::string>* returned_method_names),
              (override));
  MOCK_METHOD(void, OnHasMethod,
              (int32_t object_id, const std::string& method_name, bool* result),
              (override));
  MOCK_METHOD(void, OnInvokeMethod,
              (int32_t routing_id, int32_t object_id,
               const std::string& document_url, const std::string& method_name,
               const base::Value::List& arguments, base::Value::List* result,
               OhGinJavascriptBridgeError* error_code),
              (override));
  MOCK_METHOD(void, OnInvokeMethodAsync,
              (int32_t routing_id, int32_t object_id,
               const std::string& document_url, const std::string& method_name,
               const base::Value::List& arguments),
              (override));
  MOCK_METHOD(void, OnInvokeMethodFlowbuf,
              (int32_t routing_id, int32_t object_id,
               const std::string& document_url, const std::string& method_name,
               const base::Value::List& arguments, int fd,
               base::Value::List* result, OhGinJavascriptBridgeError* error_code),
              (override));
  MOCK_METHOD(void, OnInvokeMethodFlowbufAsync,
              (int32_t routing_id, int32_t object_id,
               const std::string& document_url, const std::string& method_name,
               const base::Value::List& arguments, int fd),
              (override));
  MOCK_METHOD(void, OnObjectWrapperDeleted,
              (int32_t routing_id, int32_t object_id),
              (override));
  MOCK_METHOD(void, OnHasAsyncThreadMethod,
              (int32_t object_id, const std::string& method_name, bool* result),
              (override));
};

class OhGinJavascriptBridgeMessageFilterTest : public testing::Test {
 protected:
  void SetUp() override {
    render_process_host_ = std::make_unique<content::MockRenderProcessHost>();
    filter_ = base::MakeRefCounted<OhGinJavascriptBridgeMessageFilter>(
        base::PassKey<OhGinJavascriptBridgeMessageFilter>(),
        render_process_host_->GetAgentSchedulingGroup());
  }

  void TearDown() override {
    filter_ = nullptr;
    render_process_host_.reset();
  }

  // Helper to create a test IPC message with a platform file attachment
  IPC::Message CreateMessageWithPlatformFileAttachment(int32_t routing_id,
                                                       int fd) {
    IPC::Message message(
        routing_id, OhGinJavascriptBridgeHostMsg_InvokeMethod_Flowbuf);
    base::PickleIterator iter(message);
    message.WriteInt(routing_id);
    message.WriteInt(123);  // object_id
    message.WriteString("https://example.com");  // document_url
    message.WriteString("testMethod");  // method_name
    message.WriteInt(0);  // arguments count

    // Add a platform file attachment
    auto attachment =
        base::MakeRefCounted<IPC::internal::PlatformFileAttachment>(fd);
    message.AddAttachment(attachment);
    return message;
  }

  // Helper to create a test IPC message with a mojo handle attachment
  IPC::Message CreateMessageWithMojoHandleAttachment(int32_t routing_id) {
    IPC::Message message(
        routing_id, OhGinJavascriptBridgeHostMsg_InvokeMethod_Flowbuf);
    base::PickleIterator iter(message);
    message.WriteInt(routing_id);
    message.WriteInt(123);  // object_id
    message.WriteString("https://example.com");  // document_url
    message.WriteString("testMethod");  // method_name
    message.WriteInt(0);  // arguments count

    // Create a dummy mojo handle attachment (invalid handle for testing)
    auto attachment = IPC::MessageAttachment::CreateFromMojoHandle(
        mojo::ScopedHandle(mojo::Handle()),
        IPC::MessageAttachment::Type::MOJO_HANDLE);
    message.AddAttachment(attachment);
    return message;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  scoped_refptr<OhGinJavascriptBridgeMessageFilter> filter_;
};

// ========================================================================
// Test: OnMessageReceivedThreadFlowbuf with valid attachment type
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnMessageReceivedThreadFlowbuf_ValidPlatformFileAttachment_Success) {
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
// This tests the type safety fix
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnMessageReceivedThreadFlowbuf_InvalidMojoHandleAttachment_ReturnsFalse) {
  // Arrange
  IPC::Message message = CreateMessageWithMojoHandleAttachment(1);

  // Act & Assert
  // The message should handle the invalid attachment type gracefully
  bool result = filter_->OnMessageReceivedThreadFlowbuf(message);
  // Should return false (handled but no matching message)
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: AddRoutingIdForHost and FindHost (private method)
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       AddRoutingIdForHost_FindHost_Success) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 100;
  constexpr int32_t kCurrentRoutingId = 100;

  // Set the current routing_id for FindHost() test
  extern thread_local int32_t current_routing_id;
  current_routing_id = kCurrentRoutingId;

  // Act
  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  // Assert - FindHost should return the host we just added
  auto found_host = filter_->FindHost();
  EXPECT_EQ(found_host.get(), mock_host.get());
}

// ========================================================================
// Test: FindHost with specific routing_id (private method)
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       FindHost_WithRoutingId_Success) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 200;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  // Act
  auto found_host = filter_->FindHost(kRoutingId);

  // Assert
  EXPECT_EQ(found_host.get(), mock_host.get());
}

// ========================================================================
// Test: FindHost when host not found (private method)
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       FindHost_HostNotFound_ReturnsNull) {
  // Arrange
  extern thread_local int32_t current_routing_id;
  current_routing_id = 999;  // Non-existent routing ID

  // Act
  auto found_host = filter_->FindHost();

  // Assert
  EXPECT_EQ(found_host.get(), nullptr);
}

// ========================================================================
// Test: RemoveHost (private method)
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       RemoveHost_Success) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 300;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  // Verify host was added
  auto found_host = filter_->FindHost(kRoutingId);
  EXPECT_NE(found_host.get(), nullptr);

  // Act
  filter_->RemoveHost(mock_host.get());

  // Assert - host should no longer be found
  found_host = filter_->FindHost(kRoutingId);
  EXPECT_EQ(found_host.get(), nullptr);
}

// ========================================================================
// Test: IsSameSite with same URL (private method)
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
// Test: IsSameSite with subdomain (private method)
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
// Test: IsSameSite with different host (private method)
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
// Test: IsSameSite with different scheme (private method)
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
// Test: IsSameSite with non-HTTP scheme (private method)
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
// Test: IsSameSite with unisolated invalid URL (private method)
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
// Test: OnGetMethods with valid host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnGetMethods_ValidHost_ReturnsMethods) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 400;
  constexpr int32_t kObjectId = 1000;

  extern thread_local int32_t current_routing_id;
  current_routing_id = kRoutingId;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  std::set<std::string> expected_methods = {"method1", "method2", "method3"};
  EXPECT_CALL(*mock_host, OnGetMethods(kObjectId, _))
      .WillOnce(testing::SetArgPointee<1>(expected_methods));

  // Act
  std::set<std::string> returned_methods;
  filter_->OnGetMethods(kObjectId, &returned_methods);

  // Assert
  EXPECT_EQ(returned_methods, expected_methods);
}

// ========================================================================
// Test: OnGetMethods with no host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnGetMethods_NoHost_ReturnsEmptySet) {
  // Arrange
  extern thread_local int32_t current_routing_id;
  current_routing_id = 999;  // Non-existent routing ID

  // Act
  std::set<std::string> returned_methods;
  filter_->OnGetMethods(1000, &returned_methods);

  // Assert
  EXPECT_TRUE(returned_methods.empty());
}

// ========================================================================
// Test: OnHasMethod with valid host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnHasMethod_ValidHost_ReturnsTrue) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 500;
  constexpr int32_t kObjectId = 2000;
  const std::string method_name = "testMethod";

  extern thread_local int32_t current_routing_id;
  current_routing_id = kRoutingId;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  EXPECT_CALL(*mock_host, OnHasMethod(kObjectId, method_name, _))
      .WillOnce(testing::SetArgPointee<2>(true));

  // Act
  bool result = false;
  filter_->OnHasMethod(kObjectId, method_name, &result);

  // Assert
  EXPECT_TRUE(result);
}

// ========================================================================
// Test: OnHasMethod with no host
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnHasMethod_NoHost_ReturnsFalse) {
  // Arrange
  extern thread_local int32_t current_routing_id;
  current_routing_id = 999;  // Non-existent routing ID

  // Act
  bool result = true;
  filter_->OnHasMethod(2000, "testMethod", &result);

  // Assert
  EXPECT_FALSE(result);
}

// ========================================================================
// Test: OnObjectWrapperDeleted with native object ID
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnObjectWrapperDeleted_NativeObjectId_DoesNotCallHost) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 600;
  constexpr int32_t kNativeObjectId =
      OhGinJavascriptBridgeDispatcherHost::MIN_NATIVE_OBJ_ID + 1;

  extern thread_local int32_t current_routing_id;
  current_routing_id = kRoutingId;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  // Native objects should NOT call OnObjectWrapperDeleted
  EXPECT_CALL(*mock_host, OnObjectWrapperDeleted(_, _)).Times(0);

  // Act
  filter_->OnObjectWrapperDeleted(kNativeObjectId);
}

// ========================================================================
// Test: OnObjectWrapperDeleted with web object ID
// ========================================================================
TEST_F(OhGinJavascriptBridgeMessageFilterTest,
       OnObjectWrapperDeleted_WebObjectId_CallsHost) {
  // Arrange
  auto mock_host =
      std::make_unique<MockOhGinJavascriptBridgeDispatcherHost>();
  constexpr int32_t kRoutingId = 700;
  constexpr int32_t kWebObjectId =
      OhGinJavascriptBridgeDispatcherHost::MIN_NATIVE_OBJ_ID - 1;

  extern thread_local int32_t current_routing_id;
  current_routing_id = kRoutingId;

  filter_->AddRoutingIdForHost(mock_host.get(),
                               render_process_host_->GetMainFrame());

  // Web objects SHOULD call OnObjectWrapperDeleted
  EXPECT_CALL(*mock_host, OnObjectWrapperDeleted(kRoutingId, kWebObjectId))
      .Times(1);

  // Act
  filter_->OnObjectWrapperDeleted(kWebObjectId);
}

}  // namespace
}  // namespace NWEB
