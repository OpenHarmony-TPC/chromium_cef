#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#include <ranges>
#define private public
#include "cef/ohos_cef_ext/libcef/browser/anti_tracking/third_party_cookie_access_policy.h"
#undef private
#include <fstream>
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "base/logging.h"
namespace ohos_anti_tracking {
namespace third_party_cookie_access_policy {
bool CheckIsInResult(const std::set<std::string>& result,
                     const std::string& host);
std::unique_ptr<autofill::Trie<std::string>> ParsingTBWOnFileThread(
    const base::FilePath& tbw_path);
std::vector<uint8_t> ToByteArray(const std::string& text);
class CheckIsInResultTest : public testing::Test {
protected:
    void SetUp();
    void TearDown() {}
    void CreateTestFile(const std::string& filename, const std::string& content) {
        base::FilePath path = temp_dir_.GetPath().AppendASCII(filename);
        ASSERT_TRUE(base::WriteFile(path, content));
        
        std::string file_content;
        ASSERT_TRUE(base::ReadFileToString(path, &file_content));
    }

    bool IsDomainInTrie(const autofill::Trie<std::string>& trie, const std::string& domain) {
        std::vector<uint8_t> key = ToByteArray(domain);
        std::reverse(key.begin(), key.end());
        
        std::set<std::string> results;
        trie.FindDataForKeyPrefix(key, &results);
        
        return results.find(domain) != results.end();
    }
    base::ScopedTempDir temp_dir_;
};

void CheckIsInResultTest::SetUp()
{
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

TEST_F(CheckIsInResultTest, CheckIsInResult_001) {
    std::set<std::string> result = {"example.com", "test.org"};
    
    EXPECT_TRUE(CheckIsInResult(result, "example.com"));
    EXPECT_TRUE(CheckIsInResult(result, "test.org"));
    
    EXPECT_FALSE(CheckIsInResult(result, "google.com"));
}

TEST_F(CheckIsInResultTest, CheckIsInResult_002) {
    std::set<std::string> result = {"com", "org"};
    
    EXPECT_TRUE(CheckIsInResult(result, "example.com"));
    EXPECT_TRUE(CheckIsInResult(result, "sub.test.org"));
    EXPECT_TRUE(CheckIsInResult(result, "a.b.c.com"));
}

TEST_F(CheckIsInResultTest, CheckIsInResult_003) {
    std::set<std::string> result = {"example.com", "test", "com"};
    
    EXPECT_TRUE(CheckIsInResult(result, "test"));
    EXPECT_TRUE(CheckIsInResult(result, "mytest.com"));
    EXPECT_FALSE(CheckIsInResult(result, "example.org"));
    EXPECT_TRUE(CheckIsInResult(result, "test.com"));
}

TEST_F(CheckIsInResultTest, CheckIsInResult_004) {
    std::set<std::string> result = {"com", "org", "localhost"};
    
    EXPECT_TRUE(CheckIsInResult(result, "localhost"));
    EXPECT_TRUE(CheckIsInResult(result, "com"));
    EXPECT_FALSE(CheckIsInResult(result, "net"));
}

TEST_F(CheckIsInResultTest, CheckIsInResult_005) {
    std::set<std::string> result;
    
    EXPECT_FALSE(CheckIsInResult(result, "example.com"));
    EXPECT_FALSE(CheckIsInResult(result, "com"));
    EXPECT_FALSE(CheckIsInResult(result, ""));
}

TEST_F(CheckIsInResultTest, CheckIsInResult_006) {
    std::set<std::string> result = {"example.com", "com"};
    
    EXPECT_FALSE(CheckIsInResult(result, ""));
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_001) {
    base::FilePath non_existent_path = temp_dir_.GetPath().AppendASCII("nonexistent.json");
    
    auto result = ParsingTBWOnFileThread(non_existent_path);
    
    ASSERT_NE(result, nullptr);
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_002) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":[{"domain":"example.com"},{"domain":"test.example.com"},{"domain":"another-test.com"}]}})";
    
    CreateTestFile("valid.json", json_content);
    base::FilePath valid_path = temp_dir_.GetPath().AppendASCII("valid.json");
    
    auto result = ParsingTBWOnFileThread(valid_path);
    
    ASSERT_NE(result, nullptr);
    
    EXPECT_FALSE(result->empty());
    EXPECT_TRUE(IsDomainInTrie(*result, "example.com"));
    EXPECT_TRUE(IsDomainInTrie(*result, "test.example.com"));
    EXPECT_TRUE(IsDomainInTrie(*result, "another-test.com"));
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_003) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":[{"domain":"  example.com  "},{"domain":"test.example.com"}]}})";
    
    CreateTestFile("trimming.json", json_content);
    base::FilePath trimming_path = temp_dir_.GetPath().AppendASCII("trimming.json");
    
    auto result = ParsingTBWOnFileThread(trimming_path);
    
    ASSERT_NE(result, nullptr);
    
    EXPECT_FALSE(result->empty());
    EXPECT_TRUE(IsDomainInTrie(*result, "example.com"));
    EXPECT_TRUE(IsDomainInTrie(*result, "test.example.com"));
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_004) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":[{"domain":"example.com"},{"domain":123},{"not_domain":"missing-field.com"},{"domain":"another-example.com"}]}})";
    
    CreateTestFile("mixed.json", json_content);
    base::FilePath mixed_path = temp_dir_.GetPath().AppendASCII("mixed.json");
    
    auto result = ParsingTBWOnFileThread(mixed_path);
    
    ASSERT_NE(result, nullptr);
    
    EXPECT_FALSE(result->empty());
    EXPECT_TRUE(IsDomainInTrie(*result, "example.com"));
    EXPECT_TRUE(IsDomainInTrie(*result, "another-example.com"));
    EXPECT_FALSE(IsDomainInTrie(*result, "missing-field.com"));
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_005) {
    std::string json_content = R"({"other_field": "value"})";
    
    CreateTestFile("no_body.json", json_content);
    base::FilePath no_body_path = temp_dir_.GetPath().AppendASCII("no_body.json");
    
    auto result = ParsingTBWOnFileThread(no_body_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_006) {
    std::string json_content = R"({"body": {"other_field": "value"}})";
    
    CreateTestFile("no_tracking.json", json_content);
    base::FilePath no_tracking_path = temp_dir_.GetPath().AppendASCII("no_tracking.json");
    
    auto result = ParsingTBWOnFileThread(no_tracking_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_007) {
    std::string json_content = R"({"body": {"trackingBehaviorWebsite": []}})";
    
    CreateTestFile("empty_tracking.json", json_content);
    base::FilePath empty_tracking_path = temp_dir_.GetPath().AppendASCII("empty_tracking.json");
    
    auto result = ParsingTBWOnFileThread(empty_tracking_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_008) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":null}})";
    
    CreateTestFile("null_tracking.json", json_content);
    base::FilePath null_tracking_path = temp_dir_.GetPath().AppendASCII("null_tracking.json");
    
    auto result = ParsingTBWOnFileThread(null_tracking_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_009) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":"not_an_array"}})";
    
    CreateTestFile("not_array_tracking.json", json_content);
    base::FilePath not_array_tracking_path = temp_dir_.GetPath().AppendASCII("not_array_tracking.json");
    
    auto result = ParsingTBWOnFileThread(not_array_tracking_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_010) {
    std::string json_content = "{ invalid json }";
    
    CreateTestFile("invalid_format.json", json_content);
    base::FilePath invalid_format_path = temp_dir_.GetPath().AppendASCII("invalid_format.json");
    
    auto result = ParsingTBWOnFileThread(invalid_format_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}

TEST_F(CheckIsInResultTest, ParsingTBWOnFileThread_011) {
    std::string json_content = R"({"body":{"trackingBehaviorWebsite":[{"domain":"example.com"})"; // 缺少闭合括号
    
    CreateTestFile("incomplete.json", json_content);
    base::FilePath incomplete_path = temp_dir_.GetPath().AppendASCII("incomplete.json");
    
    auto result = ParsingTBWOnFileThread(incomplete_path);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
}
} // namespace third_party_cookie_access_policy

class ThirdPartyCookieAccessPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_ = ThirdPartyCookieAccessPolicy::GetInstance();
        policy_->tbw_data_ = std::make_unique<autofill::Trie<std::string>>();
        policy_->bypassing_host_list_.clear();
    }
    
    void TearDown() override {
        policy_->tbw_data_.reset();
        policy_->bypassing_host_list_.clear();
        
    }

    void AddHostToTBW(const std::string& host) {
        std::string registrable_domain =
            net::registry_controlled_domains::GetDomainAndRegistry(
                host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
        
        std::vector<uint8_t> key = third_party_cookie_access_policy::ToByteArray(registrable_domain);
        std::reverse(std::begin(key), std::end(key));
        
        policy_->tbw_data_->AddDataForKey(key, registrable_domain);
    }

    void SetBypassingHostList(const std::set<std::string>& hosts) {
        policy_->bypassing_host_list_ = hosts;
    }

    bool IsHostInBypassingList(const std::string& host) {
        return policy_->bypassing_host_list_.find(host) != policy_->bypassing_host_list_.end();
    }

    size_t GetBypassingListSize() {
        return policy_->bypassing_host_list_.size();
    }

    std::set<std::string> GetBypassingList() {
        return policy_->bypassing_host_list_;
    }

    ThirdPartyCookieAccessPolicy* policy_;
};

TEST_F(ThirdPartyCookieAccessPolicyTest, AllowGetCookies_001) {
    network::ResourceRequest request;
    request.resource_type = static_cast<int>(blink::mojom::ResourceType::kMainFrame);
    request.url = GURL("https://example.com");
    request.site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://different.com"));
    
    GURL main_frame_url("https://mainframe.com");
    
    EXPECT_TRUE(policy_->AllowGetCookies(request, main_frame_url));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, AllowGetCookies_002) {
    network::ResourceRequest request;
    request.resource_type = static_cast<int>(blink::mojom::ResourceType::kSubFrame);
    request.url = GURL("https://example.com/path");
    request.site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://example.com"));
    
    GURL main_frame_url("https://mainframe.com");
    
    EXPECT_TRUE(policy_->AllowGetCookies(request, main_frame_url));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, AllowGetCookies_003) {
    network::ResourceRequest request;
    request.resource_type = static_cast<int>(blink::mojom::ResourceType::kSubFrame);
    request.url = GURL("https://example.com");
    request.site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://different.com"));
    
    GURL invalid_main_frame_url;
    
    EXPECT_TRUE(policy_->AllowGetCookies(request, invalid_main_frame_url));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, AllowGetCookies_004) {
    network::ResourceRequest request;
    request.resource_type = static_cast<int>(blink::mojom::ResourceType::kSubFrame);
    request.url = GURL("https://example.com");
    request.site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://different.com"));
    
    GURL main_frame_url("data:text/plain,Hello World");
    
    EXPECT_TRUE(policy_->AllowGetCookies(request, main_frame_url));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, CheckIsInTBW_001) {
    EXPECT_FALSE(policy_->CheckIsInTBW(""));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, CheckIsInTBW_002) {
    policy_->tbw_data_.reset();
    EXPECT_FALSE(policy_->CheckIsInTBW("example.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, CheckIsInTBW_003) {
    AddHostToTBW("tracking1.com");
    AddHostToTBW("tracking2.com");
    EXPECT_FALSE(policy_->CheckIsInTBW("example.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, CheckIsInTBW_004) {
    AddHostToTBW("tracking.com");
    EXPECT_TRUE(policy_->CheckIsInTBW("tracking.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, CheckIsInTBW_005) {
    policy_->tbw_data_ = std::make_unique<autofill::Trie<std::string>>();
    EXPECT_FALSE(policy_->CheckIsInTBW("example.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_001) {
    std::vector<std::string> hosts_to_remove = {"example.com", "test.com"};
    
    EXPECT_EQ(0, GetBypassingListSize());
    
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    
    EXPECT_EQ(0, GetBypassingListSize());
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_002) {
    std::set<std::string> hosts = {"example.com", "test.com", "demo.com"};
    SetBypassingHostList(hosts);
    
    EXPECT_EQ(3, GetBypassingListSize());
    EXPECT_TRUE(IsHostInBypassingList("example.com"));
    EXPECT_TRUE(IsHostInBypassingList("test.com"));
    EXPECT_TRUE(IsHostInBypassingList("demo.com"));
    
    std::vector<std::string> hosts_to_remove = {"test.com"};
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    
    EXPECT_EQ(2, GetBypassingListSize());
    EXPECT_TRUE(IsHostInBypassingList("example.com"));
    EXPECT_FALSE(IsHostInBypassingList("test.com"));
    EXPECT_TRUE(IsHostInBypassingList("demo.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_003) {
    std::set<std::string> hosts = {"example.com", "test.com", "demo.com", "sample.com"};
    SetBypassingHostList(hosts);
    
    EXPECT_EQ(4, GetBypassingListSize());
    
    std::vector<std::string> hosts_to_remove = {"test.com", "demo.com"};
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    
    EXPECT_EQ(2, GetBypassingListSize());
    EXPECT_TRUE(IsHostInBypassingList("example.com"));
    EXPECT_FALSE(IsHostInBypassingList("test.com"));
    EXPECT_FALSE(IsHostInBypassingList("demo.com"));
    EXPECT_TRUE(IsHostInBypassingList("sample.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_004) {
    std::set<std::string> hosts = {"example.com", "test.com", "demo.com"};
    SetBypassingHostList(hosts);
    
    EXPECT_EQ(3, GetBypassingListSize());
    
    std::vector<std::string> hosts_to_remove = {"example.com", "test.com", "demo.com"};
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    
    EXPECT_EQ(0, GetBypassingListSize());
    EXPECT_FALSE(IsHostInBypassingList("example.com"));
    EXPECT_FALSE(IsHostInBypassingList("test.com"));
    EXPECT_FALSE(IsHostInBypassingList("demo.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_005) {
    std::set<std::string> hosts = {"example.com", "test.com"};
    SetBypassingHostList(hosts);
    
    EXPECT_EQ(2, GetBypassingListSize());
    
    std::vector<std::string> hosts_to_remove = {"demo.com", "sample.com"};
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    
    EXPECT_EQ(2, GetBypassingListSize());
    EXPECT_TRUE(IsHostInBypassingList("example.com"));
    EXPECT_TRUE(IsHostInBypassingList("test.com"));
    EXPECT_FALSE(IsHostInBypassingList("demo.com"));
    EXPECT_FALSE(IsHostInBypassingList("sample.com"));
}

TEST_F(ThirdPartyCookieAccessPolicyTest, RemoveITPBypassingListOnIOThread_006) {
    std::set<std::string> hosts = {"example.com", "test.com"};
    SetBypassingHostList(hosts);

    EXPECT_EQ(2, GetBypassingListSize());

    std::vector<std::string> hosts_to_remove = {};
    policy_->RemoveITPBypassingListOnIOThread(hosts_to_remove);
    EXPECT_EQ(2, GetBypassingListSize());
}
} // namespace ohos_anti_tracking