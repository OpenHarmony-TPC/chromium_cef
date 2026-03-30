/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "arkweb/chromium_ext/base/arkweb_report_statistics.h"
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_reader_mode_config.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "libcef/browser/dom_distiller/oh_distiller_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_READER_MODE)

namespace oh_dom_distiller {

class OhDistillerUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}

  base::test::TaskEnvironment task_environment_;
};

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_Null_Nullptr) {
  GURL url("http://example.com");
  auto result = FormatDistilledArticleProtoToJsonData(nullptr, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_EQ(dict.FindInt("pageCount").value_or(-1), 0);
  EXPECT_EQ(dict.FindInt("resultCode").value_or(-1), kNoPageErrorCode);
  ASSERT_NE(dict.FindString("resultMessage"), nullptr);
  EXPECT_EQ(*dict.FindString("resultMessage"), kErrorMsgNoPage);
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_Null_EmptyProto) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_EQ(dict.FindInt("pageCount").value_or(-1), 0);
  EXPECT_EQ(dict.FindInt("resultCode").value_or(-1), kNoPageErrorCode);
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithValidPage) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithHwDistillerResult) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  std::string hw_result = R"({
    "bookName": "Test Book",
    "chapterName": "Chapter 1",
    "type": "content"
  })";
  page->set_hw_distiller_result_json(hw_result);

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      ASSERT_NE(first_page->FindString("bookName"), nullptr);
      EXPECT_EQ(*first_page->FindString("bookName"), "Test Book");
      ASSERT_NE(first_page->FindString("chapterName"), nullptr);
      EXPECT_EQ(*first_page->FindString("chapterName"), "Chapter 1");
      ASSERT_NE(first_page->FindString("type"), nullptr);
      EXPECT_EQ(*first_page->FindString("type"), "content");
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithInvalidHwDistillerResult) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");
  page->set_hw_distiller_result_json("{ invalid json }");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithEmptyHwDistillerResult) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");
  page->set_hw_distiller_result_json("");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithFlatFields) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  flat->mutable_distilled_result()->set_code(0);
  flat->set_book_name("Test Book");
  flat->set_chapter_name("Chapter 1");
  flat->set_author("Test Author");
  flat->set_img_url("http://example.com/image.jpg");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      ASSERT_NE(first_page->FindString("bookName"), nullptr);
      EXPECT_EQ(*first_page->FindString("bookName"), "Test Book");
      ASSERT_NE(first_page->FindString("chapterName"), nullptr);
      EXPECT_EQ(*first_page->FindString("chapterName"), "Chapter 1");
      ASSERT_NE(first_page->FindString("author"), nullptr);
      EXPECT_EQ(*first_page->FindString("author"), "Test Author");
      ASSERT_NE(first_page->FindString("coverUrl"), nullptr);
      EXPECT_EQ(*first_page->FindString("coverUrl"), "http://example.com/image.jpg");
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithContentInfo) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  auto* content_info = flat->mutable_content_info();
  content_info->set_next_page("http://example.com/next");
  content_info->set_prev_page("http://example.com/prev");
  content_info->set_catalog_page("http://example.com/catalog");
  flat->set_content("Test content");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      ASSERT_NE(first_page->FindString("nextPageUrl"), nullptr);
      EXPECT_EQ(*first_page->FindString("nextPageUrl"), "http://example.com/next");
      ASSERT_NE(first_page->FindString("prevPageUrl"), nullptr);
      EXPECT_EQ(*first_page->FindString("prevPageUrl"), "http://example.com/prev");
      ASSERT_NE(first_page->FindString("catalogPageUrl"), nullptr);
      EXPECT_EQ(*first_page->FindString("catalogPageUrl"), "http://example.com/catalog");
      ASSERT_NE(first_page->FindString("pageContent"), nullptr);
      EXPECT_EQ(*first_page->FindString("pageContent"), "Test content");
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithCatalogInfo) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  auto* catalog = flat->mutable_catalog_info();

  auto* chapter1 = catalog->add_latest_chapters();
  chapter1->set_chapter_name("Chapter 1");
  chapter1->set_chapter_url("http://example.com/chapter1");

  auto* chapter2 = catalog->add_all_chapters();
  chapter2->set_chapter_name("Chapter 2");
  chapter2->set_chapter_url("http://example.com/chapter2");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      if (const auto* latest_chapters = first_page->FindList("latestChapters")) {
        EXPECT_GT(latest_chapters->size(), 0U);
      }
      if (const auto* all_chapters = first_page->FindList("allChapters")) {
        EXPECT_GT(all_chapters->size(), 0U);
      }
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithPaginationInfo) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  auto* catalog = flat->mutable_catalog_info();
  auto* pagination = catalog->mutable_pagination_info();
  pagination->set_next_catalog_page("http://example.com/next-catalog");
  pagination->set_prev_catalog_page("http://example.com/prev-catalog");
  pagination->set_whole_catalog_page("http://example.com/whole-catalog");

  auto* page1 = pagination->add_all_catalog_pages();
  page1->set_page_name("Page 1");
  page1->set_page_url("http://example.com/page1");
  page1->set_selected(true);

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      if (const auto* catalog_pagination = first_page->FindDict("catalogPagination")) {
        ASSERT_NE(catalog_pagination->FindString("nextPageUrl"), nullptr);
        EXPECT_EQ(*catalog_pagination->FindString("nextPageUrl"),
                  "http://example.com/next-catalog");
        ASSERT_NE(catalog_pagination->FindString("prevPageUrl"), nullptr);
        EXPECT_EQ(*catalog_pagination->FindString("prevPageUrl"),
                  "http://example.com/prev-catalog");
        ASSERT_NE(catalog_pagination->FindString("morePageUrl"), nullptr);
        EXPECT_EQ(*catalog_pagination->FindString("morePageUrl"),
                  "http://example.com/whole-catalog");
      }
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithTimingInfo) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  auto* timing = flat->mutable_timing_info();
  timing->set_total_time(100.5);
  timing->set_distill_time(50.25);

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      if (const auto* timing_info = first_page->FindDict("timingInfo")) {
        ASSERT_NE(timing_info->FindString("total_time"), nullptr);
        EXPECT_EQ(*timing_info->FindString("total_time"), "100.50");
        ASSERT_NE(timing_info->FindString("distill_time"), nullptr);
        EXPECT_EQ(*timing_info->FindString("distill_time"), "50.25");
      }
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithErrorCode) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  auto* distilled_result = flat->mutable_distilled_result();
  distilled_result->set_code(-1);
  distilled_result->set_message("Test error message");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      EXPECT_EQ(first_page->FindInt("resultCode").value_or(0), -1);
      ASSERT_NE(first_page->FindString("resultMessage"), nullptr);
      EXPECT_EQ(*first_page->FindString("resultMessage"), "Test error message");
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithNoContent) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  flat->mutable_content_info();
  flat->set_content("");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      EXPECT_EQ(first_page->FindInt("resultCode").value_or(0), kNoContentErrorCode);
      ASSERT_NE(first_page->FindString("resultMessage"), nullptr);
      EXPECT_EQ(*first_page->FindString("resultMessage"), kErrorMsgNoContent);
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithNoChapters) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page");

  auto* flat = page->mutable_flat_fields();
  flat->mutable_catalog_info();

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);

  if (const auto* result_list = dict.FindList("resultList")) {
    ASSERT_GT(result_list->size(), 0U);
    if (const auto* first_page = (*result_list)[0].GetIfDict()) {
      EXPECT_EQ(first_page->FindInt("resultCode").value_or(0), kNoChaptersErrorCode);
      ASSERT_NE(first_page->FindString("resultMessage"), nullptr);
      EXPECT_EQ(*first_page->FindString("resultMessage"), kErrorMsgNoChapters);
    }
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_MultiplePages) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  for (int i = 0; i < 3; ++i) {
    auto* page = article_proto.add_pages();
    page->set_url("http://example.com/page" + std::to_string(i + 1));
    page->set_title("Test Page " + std::to_string(i + 1));

    auto* flat = page->mutable_flat_fields();
    flat->set_book_name("Test Book");
    flat->set_chapter_name("Chapter " + std::to_string(i + 1));
  }

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_EQ(dict.FindInt("pageCount").value_or(0), 3);

  if (const auto* result_list = dict.FindList("resultList")) {
    EXPECT_EQ(result_list->size(), 3U);
  }
}

TEST_F(OhDistillerUtilsTest, FormatDistilledArticleProtoToJsonData_WithSpecialCharacters) {
  GURL url("http://example.com");
  dom_distiller::DistilledArticleProto article_proto;

  auto* page = article_proto.add_pages();
  page->set_url("http://example.com/page1");
  page->set_title("Test Page with \"quotes\" and 'apostrophes'");
  
  auto* flat = page->mutable_flat_fields();
  flat->set_book_name("Book with \n newline");
  flat->set_content("Content with \t tab");

  auto result = FormatDistilledArticleProtoToJsonData(&article_proto, url);

  ASSERT_NE(result, nullptr);

  auto parsed = base::JSONReader::Read(*result);
  ASSERT_TRUE(parsed.has_value());
  ASSERT_TRUE(parsed->is_dict());

  const auto& dict = parsed->GetDict();
  EXPECT_GT(dict.FindInt("pageCount").value_or(0), 0);
}

}  // namespace oh_dom_distiller

#endif  // ARKWEB_READER_MODE
